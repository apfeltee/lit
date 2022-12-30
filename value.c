
#include <stdarg.h>
#include <stdio.h>
#include "lit.h"
#include "sds.h"

#if 0
    #define pack754_32(f) (pack754((f), 32, 8))
    #define unpack754_32(i) (unpack754((i), 32, 8))
#endif

#define pack754_64(f) (pack754((f), 64, 11))
#define unpack754_64(i) (unpack754((i), 64, 11))

uint64_t pack754(long double f, unsigned bits, unsigned expbits)
{
    long double fnorm;
    int shift;
    long long sign;
    long long exp;
    long long significand;
    unsigned significandbits;
    /* -1 for sign bit */
    significandbits = bits - expbits - 1;
    /* get this special case out of the way */
    if(f == 0.0)
    {
        return 0;
    }
    /* check sign and begin normalization */
    if(f < 0)
    {
        sign = 1;
        fnorm = -f;
    }
    else
    {
        sign = 0;
        fnorm = f;
    }
    /* get the normalized form of f and track the exponent */
    shift = 0;
    while(fnorm >= 2.0)
    {
        fnorm /= 2.0;
        shift++;
    }
    while(fnorm < 1.0)
    {
        fnorm *= 2.0;
        shift--;
    }
    fnorm = fnorm - 1.0;
    /* calculate the binary form (non-float) of the significand data */
    significand = fnorm * ((1LL << significandbits) + 0.5f);
    /* get the biased exponent */
    /* shift + bias */
    exp = shift + ((1<<(expbits-1)) - 1);
    /* return the final answer */
    return (
        (sign << (bits - 1)) | (exp << (bits - expbits - 1)) | significand
    );
}

long double unpack754(uint64_t i, unsigned bits, unsigned expbits)
{
    long double result;
    long long shift;
    unsigned bias;
    unsigned significandbits;
    /* -1 for sign bit */
    significandbits = bits - expbits - 1;
    if(i == 0)
    {
        return 0.0;
    }
    /* pull the significand */
    /* mask */
    result = (i&((1LL<<significandbits)-1));
    /* convert back to float */
    result /= (1LL<<significandbits);
    /* add the one back on */
    result += 1.0f;
    /* deal with the exponent */
    bias = ((1 << (expbits - 1)) - 1);
    shift = (((i >> significandbits) & ((1LL << expbits) - 1)) - bias);
    while(shift > 0)
    {
        result *= 2.0;
        shift--;
    }
    while(shift < 0)
    {
        result /= 2.0;
        shift++;
    }
    /* sign it */
    if(((i>>(bits-1)) & 1) == 1)
    {
        result = result * -1.0;
    }
    else
    {
        result = result * 1.0;
    }
    return result;
}

/* this used to be done via type punning, which may not be portable */
double lit_util_uinttofloat(unsigned int val)
{
    return unpack754_64(val);
}

unsigned int lit_util_floattouint(double val)
{
    return pack754_64(val);
}

int lit_util_doubletoint(double n)
{
    if(n == 0)
    {
        return 0;
    }
    if(isnan(n))
    {
        return 0;
    }
    if(n < 0)
    {
        n = -floor(-n);
    }
    else
    {
        n = floor(n);
    }
    if(n < INT_MIN)
    {
        return INT_MIN;
    }
    if(n > INT_MAX)
    {
        return INT_MAX;
    }
    return (int)n;
}

int lit_util_numbertoint32(double n)
{
    /* magic. no idea. */
    double two32 = 4294967296.0;
    double two31 = 2147483648.0;
    if(!isfinite(n) || n == 0)
    {
        return 0;
    }
    n = fmod(n, two32);
    if(n >= 0)
    {
        n = floor(n);
    }
    else
    {
        n = ceil(n) + two32;
    }
    if(n >= two31)
    {
        return n - two32;
    }
    return n;
}

unsigned int lit_util_numbertouint32(double n)
{
    return (unsigned int)lit_util_numbertoint32(n);
}

static void litwr_cb_writebyte(LitWriter* wr, int byte)
{
    LitString* ds;
    if(wr->stringmode)
    {
        ds = (LitString*)wr->uptr;
        lit_string_appendchar(ds, byte);        
    }
    else
    {
        fputc(byte, (FILE*)wr->uptr);
    }
}

static void litwr_cb_writestring(LitWriter* wr, const char* string, size_t len)
{
    LitString* ds;
    if(wr->stringmode)
    {
        ds = (LitString*)wr->uptr;
        lit_string_appendlen(ds, string, len);
    }
    else
    {
        fwrite(string, sizeof(char), len, (FILE*)wr->uptr);
    }
}

static void litwr_cb_writeformat(LitWriter* wr, const char* fmt, va_list va)
{
    LitString* ds;
    if(wr->stringmode)
    {
        ds = (LitString*)wr->uptr;
        ds->chars = sdscatvprintf(ds->chars, fmt, va);
    }
    else
    {
        vfprintf((FILE*)wr->uptr, fmt, va);
    }
}

static void lit_writer_init_default(LitState* state, LitWriter* wr)
{
    wr->state = state;
    wr->forceflush = false;
    wr->stringmode = false;
    wr->fnbyte = litwr_cb_writebyte;
    wr->fnstring = litwr_cb_writestring;
    wr->fnformat = litwr_cb_writeformat;
}

void lit_writer_init_file(LitState* state, LitWriter* wr, FILE* fh, bool forceflush)
{
    lit_writer_init_default(state, wr);
    wr->uptr = fh;
    wr->forceflush = forceflush;
}

void lit_writer_init_string(LitState* state, LitWriter* wr)
{
    lit_writer_init_default(state, wr);
    wr->stringmode = true;
    wr->uptr = lit_string_makeempty(state, 0, false);
}

void lit_writer_writebyte(LitWriter* wr, int byte)
{
    wr->fnbyte(wr, byte);
}

void lit_writer_writestringl(LitWriter* wr, const char* str, size_t len)
{
    wr->fnstring(wr, str, len);
}

void lit_writer_writestring(LitWriter* wr, const char* str)
{
    wr->fnstring(wr, str, strlen(str));
}

void lit_writer_writeformat(LitWriter* wr, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    wr->fnformat(wr, fmt, va);
    va_end(va);
}

LitString* lit_writer_get_string(LitWriter* wr)
{
    if(wr->stringmode)
    {
        return (LitString*)wr->uptr;
    }
    return NULL;
}

static const char* lit_object_type_names[] =
{
    "string",
    "function",
    "native_function",
    "native_primitive",
    "native_method",
    "primitive_method",
    "fiber",
    "module",
    "closure",
    "upvalue",
    "class",
    "instance",
    "bound_method",
    "array",
    "map",
    "userdata",
    "range",
    "field",
    "reference"
};



static void print_array(LitState* state, LitWriter* wr, LitArray* array, size_t size)
{
    size_t i;
    lit_writer_writeformat(wr, "(%u) [", (unsigned int)size);
    if(size > 0)
    {
        lit_writer_writestring(wr, " ");
        for(i = 0; i < size; i++)
        {
            if(lit_value_isarray(lit_vallist_get(&array->list, i)) && (array == AS_ARRAY(lit_vallist_get(&array->list,i))))
            {
                lit_writer_writestring(wr, "(recursion)");
            }
            else
            {
                lit_print_value(state, wr, lit_vallist_get(&array->list, i));
            }
            if(i + 1 < size)
            {
                lit_writer_writestring(wr, ", ");
            }
            else
            {
                lit_writer_writestring(wr, " ");
            }
        }
    }
    lit_writer_writestring(wr, "]");
}

static void print_map(LitState* state, LitWriter* wr, LitMap* map, size_t size)
{
    bool had_before;
    size_t i;
    LitTableEntry* entry;
    lit_writer_writeformat(wr, "(%u) {", (unsigned int)size);
    had_before = false;
    if(size > 0)
    {
        for(i = 0; i < (size_t)map->values.capacity; i++)
        {
            entry = &map->values.entries[i];
            if(entry->key != NULL)
            {
                if(had_before)
                {
                    lit_writer_writestring(wr, ", ");
                }
                else
                {
                    lit_writer_writestring(wr, " ");
                }
                lit_writer_writeformat(wr, "%s = ", entry->key->chars);
                if(lit_value_ismap(entry->value) && (map == AS_MAP(entry->value)))
                {
                    lit_writer_writestring(wr, "(recursion)");
                }
                else
                {
                    lit_print_value(state, wr, entry->value);
                }
                had_before = true;
            }
        }
    }
    if(had_before)
    {
        lit_writer_writestring(wr, " }");
    }
    else
    {
        lit_writer_writestring(wr, "}");
    }
}

static void print_object(LitState* state, LitWriter* wr, LitValue value)
{
    size_t size;
    LitMap* map;
    LitArray* array;
    LitRange* range;
    LitValue* slot;
    LitObject* obj;
    LitUpvalue* upvalue;
    obj = lit_as_object(value);
    if(obj != NULL)
    {
        switch(obj->type)
        {
            case LITTYPE_STRING:
                {
                    lit_writer_writeformat(wr, "%s", lit_as_cstring(value));
                }
                break;
            case LITTYPE_FUNCTION:
                {
                    lit_writer_writeformat(wr, "function %s", AS_FUNCTION(value)->name->chars);
                }
                break;
            case LITTYPE_CLOSURE:
                {
                    lit_writer_writeformat(wr, "closure %s", AS_CLOSURE(value)->function->name->chars);
                }
                break;
            case LITTYPE_NATIVE_PRIMITIVE:
                {
                    lit_writer_writeformat(wr, "function %s", AS_NATIVE_PRIMITIVE(value)->name->chars);
                }
                break;
            case LITTYPE_NATIVE_FUNCTION:
                {
                    lit_writer_writeformat(wr, "function %s", AS_NATIVE_FUNCTION(value)->name->chars);
                }
                break;
            case LITTYPE_PRIMITIVE_METHOD:
                {
                    lit_writer_writeformat(wr, "function %s", AS_PRIMITIVE_METHOD(value)->name->chars);
                }
                break;
            case LITTYPE_NATIVE_METHOD:
                {
                    lit_writer_writeformat(wr, "function %s", AS_NATIVE_METHOD(value)->name->chars);
                }
                break;
            case LITTYPE_FIBER:
                {
                    lit_writer_writeformat(wr, "fiber");
                }
                break;
            case LITTYPE_MODULE:
                {
                    lit_writer_writeformat(wr, "module %s", AS_MODULE(value)->name->chars);
                }
                break;

            case LITTYPE_UPVALUE:
                {
                    upvalue = AS_UPVALUE(value);
                    if(upvalue->location == NULL)
                    {
                        lit_print_value(state, wr, upvalue->closed);
                    }
                    else
                    {
                        print_object(state, wr, *upvalue->location);
                    }
                }
                break;
            case LITTYPE_CLASS:
                {
                    lit_writer_writeformat(wr, "class %s", AS_CLASS(value)->name->chars);
                }
                break;
            case LITTYPE_INSTANCE:
                {
                    /*
                    if(AS_INSTANCE(value)->klass->object.type == LITTYPE_MAP)
                    {
                        fprintf(stderr, "instance is a map\n");
                    }
                    printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
                    */
                    lit_writer_writeformat(wr, "<instance '%s' ", AS_INSTANCE(value)->klass->name->chars);
                    map = AS_MAP(value);
                    size = map->values.count;
                    print_map(state, wr, map, size);
                    lit_writer_writestring(wr, ">");
                }
                break;
            case LITTYPE_BOUND_METHOD:
                {
                    lit_print_value(state, wr, AS_BOUND_METHOD(value)->method);
                    return;
                }
                break;
            case LITTYPE_ARRAY:
                {
                    #ifdef LIT_MINIMIZE_CONTAINERS
                        lit_writer_writestring(wr, "array");
                    #else
                        array = AS_ARRAY(value);
                        size = lit_vallist_count(&array->list);
                        print_array(state, wr, array, size);
                    #endif
                }
                break;
            case LITTYPE_MAP:
                {
                    #ifdef LIT_MINIMIZE_CONTAINERS
                        lit_writer_writeformat(wr, "map");
                    #else
                        map = AS_MAP(value);
                        size = map->values.count;
                        print_map(state, wr, map, size);
                    #endif
                }
                break;
            case LITTYPE_USERDATA:
                {
                    lit_writer_writeformat(wr, "userdata");
                }
                break;
            case LITTYPE_RANGE:
                {
                    range = AS_RANGE(value);
                    lit_writer_writeformat(wr, "%g .. %g", range->from, range->to);
                }
                break;
            case LITTYPE_FIELD:
                {
                    lit_writer_writeformat(wr, "field");
                }
                break;
            case LITTYPE_REFERENCE:
                {
                    lit_writer_writeformat(wr, "reference => ");
                    slot = AS_REFERENCE(value)->slot;
                    if(slot == NULL)
                    {
                        lit_writer_writestring(wr, "null");
                    }
                    else
                    {
                        lit_print_value(state, wr, *slot);
                    }
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else
    {
        lit_writer_writestring(wr, "!nullpointer!");
    }
}

//LitInterpretResult lit_call_instance_method(LitState* state, LitInstance* instance, LitString* mthname, LitValue* argv, size_t argc)
//
void lit_print_value(LitState* state, LitWriter* wr, LitValue value)
{
    /*
    LitValue mthtostring;
    LitValue tstrval;
    LitString* tstring;
    LitString* mthname;
    LitInterpretResult inret;
    LitValue args[1] = {NULL_VALUE};
    mthname = CONST_STRING(state, "toString");
    fprintf(stderr, "lit_print_value: checking if toString() exists for '%s' ...\n", lit_get_value_type(value));
    if(AS_CLASS(value) != NULL)
    {
        mthtostring = lit_instance_get_method(state, value, mthname);
        if(!lit_value_isnull(mthtostring))
        {
            fprintf(stderr, "lit_print_value: we got toString()! now checking if calling it works ...\n");
            inret = lit_instance_call_method(state, value, mthname, args, 0);
            if(inret.type == LITRESULT_OK)
            {
                fprintf(stderr, "lit_print_value: calling toString() succeeded! but is it a string? ...\n");
                tstrval = inret.result;
                if(!lit_value_isnull(tstrval))
                {
                    fprintf(stderr, "lit_print_value: toString() returned a string! so that's what we'll use.\n");
                    tstring = lit_as_string(tstrval);
                    printf("%.*s", (int)lit_string_getlength(tstring), tstring->chars);
                    return;
                }
            }
        }
    }
    fprintf(stderr, "lit_print_value: nope, no toString(), or it didn't return a string. falling back to manual stringification\n");
    */
    if(IS_BOOL(value))
    {
        lit_writer_writestring(wr, lit_as_bool(value) ? "true" : "false");
    }
    else if(lit_value_isnull(value))
    {
        lit_writer_writestring(wr, "null");
    }
    else if(lit_value_isnumber(value))
    {
        lit_writer_writeformat(wr, "%g", lit_value_to_number(value));
    }
    else if(lit_value_isobject(value))
    {
        print_object(state, wr, value);
    }
}


const char* lit_get_value_type(LitValue value)
{
    if(IS_BOOL(value))
    {
        return "bool";
    }
    else if(lit_value_isnull(value))
    {
        return "null";
    }
    else if(lit_value_isnumber(value))
    {
        return "number";
    }
    else if(lit_value_isobject(value))
    {
        return lit_object_type_names[OBJECT_TYPE(value)];
    }
    return "unknown";
}
