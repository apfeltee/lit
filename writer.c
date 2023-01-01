#include <stdarg.h>
#include <stdio.h>
#include "lit.h"
#include "sds.h"

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
            if(lit_value_isarray(lit_vallist_get(&array->list, i)) && (array == lit_value_asarray(lit_vallist_get(&array->list,i))))
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
                if(lit_value_ismap(entry->value) && (map == lit_value_asmap(entry->value)))
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
    obj = lit_value_asobject(value);
    if(obj != NULL)
    {
        switch(obj->type)
        {
            case LITTYPE_STRING:
                {
                    lit_writer_writeformat(wr, "%s", lit_value_ascstring(value));
                }
                break;
            case LITTYPE_FUNCTION:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asfunction(value)->name->chars);
                }
                break;
            case LITTYPE_CLOSURE:
                {
                    lit_writer_writeformat(wr, "closure %s", lit_value_asclosure(value)->function->name->chars);
                }
                break;
            case LITTYPE_NATIVE_PRIMITIVE:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asnativeprimitive(value)->name->chars);
                }
                break;
            case LITTYPE_NATIVE_FUNCTION:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asnativefunction(value)->name->chars);
                }
                break;
            case LITTYPE_PRIMITIVE_METHOD:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asprimitivemethod(value)->name->chars);
                }
                break;
            case LITTYPE_NATIVE_METHOD:
                {
                    lit_writer_writeformat(wr, "function %s", lit_value_asnativemethod(value)->name->chars);
                }
                break;
            case LITTYPE_FIBER:
                {
                    lit_writer_writeformat(wr, "fiber");
                }
                break;
            case LITTYPE_MODULE:
                {
                    lit_writer_writeformat(wr, "module %s", lit_value_asmodule(value)->name->chars);
                }
                break;

            case LITTYPE_UPVALUE:
                {
                    upvalue = lit_value_asupvalue(value);
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
                    lit_writer_writeformat(wr, "class %s", lit_value_asclass(value)->name->chars);
                }
                break;
            case LITTYPE_INSTANCE:
                {
                    /*
                    if(lit_value_asinstance(value)->klass->object.type == LITTYPE_MAP)
                    {
                        fprintf(stderr, "instance is a map\n");
                    }
                    printf("%s instance", lit_value_asinstance(value)->klass->name->chars);
                    */
                    lit_writer_writeformat(wr, "<instance '%s' ", lit_value_asinstance(value)->klass->name->chars);
                    map = lit_value_asmap(value);
                    size = map->values.count;
                    print_map(state, wr, map, size);
                    lit_writer_writestring(wr, ">");
                }
                break;
            case LITTYPE_BOUND_METHOD:
                {
                    lit_print_value(state, wr, lit_value_asboundmethod(value)->method);
                    return;
                }
                break;
            case LITTYPE_ARRAY:
                {
                    #ifdef LIT_MINIMIZE_CONTAINERS
                        lit_writer_writestring(wr, "array");
                    #else
                        array = lit_value_asarray(value);
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
                        map = lit_value_asmap(value);
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
                    range = lit_value_asrange(value);
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
                    slot = lit_value_asreference(value)->slot;
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
    fprintf(stderr, "lit_print_value: checking if toString() exists for '%s' ...\n", lit_value_typename(value));
    if(lit_value_asclass(value) != NULL)
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
                    tstring = lit_value_asstring(tstrval);
                    printf("%.*s", (int)lit_string_getlength(tstring), tstring->chars);
                    return;
                }
            }
        }
    }
    fprintf(stderr, "lit_print_value: nope, no toString(), or it didn't return a string. falling back to manual stringification\n");
    */
    if(lit_value_isbool(value))
    {
        lit_writer_writestring(wr, lit_value_asbool(value) ? "true" : "false");
    }
    else if(lit_value_isnull(value))
    {
        lit_writer_writestring(wr, "null");
    }
    else if(lit_value_isnumber(value))
    {
        lit_writer_writeformat(wr, "%g", lit_value_asnumber(value));
    }
    else if(lit_value_isobject(value))
    {
        print_object(state, wr, value);
    }
}


const char* lit_value_typename(LitValue value)
{
    if(lit_value_isbool(value))
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
        return lit_object_type_names[lit_value_type(value)];
    }
    return "unknown";
}


