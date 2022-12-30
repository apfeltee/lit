
#include "lit.h"
#include "sds.h"

char* itoa(int value, char* result, int base)
{
    int tmp_value;
    char* ptr;
    char* ptr1;
    char tmp_char;
    // check that the base if valid
    if (base < 2 || base > 36)
    {
        *result = '\0';
        return result;
    }
    ptr = result;
    ptr1 = result;
    do
    {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );
    // Apply negative sign
    if (tmp_value < 0)
    {
        *ptr++ = '-';
    }
    *ptr-- = '\0';
    while(ptr1 < ptr)
    {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

static char *lit_util_itoshelper(char *dest, size_t n, int x)
{
    if (n == 0)
    {
        return NULL;
    }
    if (x <= -10)
    {
        dest = lit_util_itoshelper(dest, n - 1, x / 10);
        if (dest == NULL)
        {
            return NULL;
        }
    }
    *dest = (char) ('0' - x % 10);
    return dest + 1;
}

char *lit_util_inttostring(char *dest, size_t n, int x)
{
    char *p;
    p = dest;
    if (n == 0)
    {
        return NULL;
    }
    n--;
    if (x < 0)
    {
        if(n == 0)
        {
            return NULL;
        }
        n--;
        *p++ = '-';
    }
    else
    {
        x = -x;
    }
    p = lit_util_itoshelper(p, n, x);
    if(p == NULL)
    {
        return NULL;
    }
    *p = 0;
    return dest;
}

uint32_t lit_util_hashstring(const char* key, size_t length)
{
    size_t i;
    uint32_t hash = 2166136261u;
    for(i = 0; i < length; i++)
    {
        hash ^= key[i];
        hash *= 16777619;
    }
    return hash;
}

int lit_util_decodenumbytes(uint8_t byte)
{
    if((byte & 0xc0) == 0x80)
    {
        return 0;
    }
    if((byte & 0xf8) == 0xf0)
    {
        return 4;
    }
    if((byte & 0xf0) == 0xe0)
    {
        return 3;
    }
    if((byte & 0xe0) == 0xc0)
    {
        return 2;
    }
    return 1;
}

int lit_ustring_length(LitString* string)
{
    int length;
    uint32_t i;
    length = 0;
    for(i = 0; i < lit_string_getlength(string);)
    {
        i += lit_util_decodenumbytes(string->chars[i]);
        length++;
    }
    return length;
}

LitString* lit_ustring_codepointat(LitState* state, LitString* string, uint32_t index)
{
    char bytes[2];
    int code_point;
    if(index >= lit_string_getlength(string))
    {
        return NULL;
    }
    code_point = lit_ustring_decode((uint8_t*)string->chars + index, lit_string_getlength(string) - index);
    if(code_point == -1)
    {
        bytes[0] = string->chars[index];
        bytes[1] = '\0';
        return lit_string_copy(state, bytes, 1);
    }
    return lit_ustring_fromcodepoint(state, code_point);
}

LitString* lit_ustring_fromcodepoint(LitState* state, int value)
{
    int length;
    char* bytes;
    LitString* rt;
    length = lit_util_encodenumbytes(value);
    bytes = LIT_ALLOCATE(state, sizeof(char), length + 1);
    lit_ustring_encode(value, (uint8_t*)bytes);
    /* this should be lit_string_take, but something prevents the memory from being free'd. */
    rt = lit_string_copy(state, bytes, length);
    LIT_FREE(state, sizeof(char), bytes);
    return rt;
}

LitString* lit_ustring_fromrange(LitState* state, LitString* source, int start, uint32_t count)
{
    int length;
    int index;
    int code_point;
    uint32_t i;
    uint8_t* to;
    uint8_t* from;
    char* bytes;
    from = (uint8_t*)source->chars;
    length = 0;
    for(i = 0; i < count; i++)
    {
        length += lit_util_decodenumbytes(from[start + i]);
    }
    bytes = (char*)malloc(length + 1);
    to = (uint8_t*)bytes;
    for(i = 0; i < count; i++)
    {
        index = start + i;
        code_point = lit_ustring_decode(from + index, lit_string_getlength(source) - index);
        if(code_point != -1)
        {
            to += lit_ustring_encode(code_point, to);
        }
    }
    return lit_string_take(state, bytes, length, false);
}

int lit_util_encodenumbytes(int value)
{
    if(value <= 0x7f)
    {
        return 1;
    }
    if(value <= 0x7ff)
    {
        return 2;
    }
    if(value <= 0xffff)
    {
        return 3;
    }
    if(value <= 0x10ffff)
    {
        return 4;
    }
    return 0;
}

int lit_ustring_encode(int value, uint8_t* bytes)
{
    if(value <= 0x7f)
    {
        *bytes = value & 0x7f;
        return 1;
    }
    else if(value <= 0x7ff)
    {
        *bytes = 0xc0 | ((value & 0x7c0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 2;
    }
    else if(value <= 0xffff)
    {
        *bytes = 0xe0 | ((value & 0xf000) >> 12);
        bytes++;
        *bytes = 0x80 | ((value & 0xfc0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 3;
    }
    else if(value <= 0x10ffff)
    {
        *bytes = 0xf0 | ((value & 0x1c0000) >> 18);
        bytes++;
        *bytes = 0x80 | ((value & 0x3f000) >> 12);
        bytes++;
        *bytes = 0x80 | ((value & 0xfc0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 4;
    }
    UNREACHABLE
    return 0;
}

int lit_ustring_decode(const uint8_t* bytes, uint32_t length)
{
    int value;
    uint32_t remaining_bytes;
    if(*bytes <= 0x7f)
    {
        return *bytes;
    }
    if((*bytes & 0xe0) == 0xc0)
    {
        value = *bytes & 0x1f;
        remaining_bytes = 1;
    }
    else if((*bytes & 0xf0) == 0xe0)
    {
        value = *bytes & 0x0f;
        remaining_bytes = 2;
    }
    else if((*bytes & 0xf8) == 0xf0)
    {
        value = *bytes & 0x07;
        remaining_bytes = 3;
    }
    else
    {
        return -1;
    }
    if(remaining_bytes > length - 1)
    {
        return -1;
    }
    while(remaining_bytes > 0)
    {
        bytes++;
        remaining_bytes--;
        if((*bytes & 0xc0) != 0x80)
        {
            return -1;
        }
        value = value << 6 | (*bytes & 0x3f);
    }
    return value;
}

int lit_util_ucharoffset(char* str, int index)
{
#define is_utf(c) (((c)&0xC0) != 0x80)
    int offset;
    offset = 0;
    while(index > 0 && str[offset])
    {
        (void)(is_utf(str[++offset]) || is_utf(str[++offset]) || is_utf(str[++offset]) || ++offset);
        index--;
    }
    return offset;
#undef is_utf
}

LitString* lit_string_makeempty(LitState* state, size_t length, bool reuse)
{
    LitString* string;
    string = (LitString*)lit_allocate_object(state, sizeof(LitString), LITTYPE_STRING);
    if(!reuse)
    {
        string->chars = sdsempty();
        /* reserving the required space may reduce number of allocations */
        string->chars = sdsMakeRoomFor(string->chars, length);
    }
    //string->chars = NULL;
    string->hash = 0;
    return string;
}

/*
* if given $chars was alloc'd via sds, then only a LitString instance is created, without initializing
* string->chars.
* if it was not, and not scheduled for reuse, then first an empty sds string is created,
* and $chars is appended, and finally, $chars is freed.
* NB. do *not* actually allocate any sds instance here - this is already done in lit_string_makeempty().
*/
LitString* lit_string_makelen(LitState* state, char* chars, size_t length, uint32_t hash, bool wassds, bool reuse)
{
    LitString* string;
    string = lit_string_makeempty(state, length, reuse);
    if(wassds && reuse)
    {
        string->chars = chars;
    }
    else
    {
        /*
        * string->chars is initialized in lit_string_makeempty(),
        * as an empty string!
        */
        string->chars = sdscatlen(string->chars, chars, length);
    }
    string->hash = hash;
    if(!wassds)
    {
        LIT_FREE(state, sizeof(char), chars);
    }
    lit_state_regstring(state, string);
    return string;
}

void lit_state_regstring(LitState* state, LitString* string)
{
    if(lit_string_getlength(string) > 0)
    {
        lit_state_pushroot(state, (LitObject*)string);
        lit_table_set(state, &state->vm->strings, string, NULL_VALUE);
        lit_state_poproot(state);
    }
}

/* todo: track if $chars is a sds instance - additional argument $fromsds? */
LitString* lit_string_take(LitState* state, char* chars, size_t length, bool wassds)
{
    bool reuse;
    uint32_t hash;
    hash = lit_util_hashstring(chars, length);
    LitString* interned;
    interned = lit_table_find_string(&state->vm->strings, chars, length, hash);
    if(interned != NULL)
    {
        if(!wassds)
        {
            LIT_FREE(state, sizeof(char), chars);
            //sdsfree(chars);
        }
        return interned;
    }
    reuse = wassds;
    return lit_string_makelen(state, (char*)chars, length, hash, wassds, reuse);
}

LitString* lit_string_copy(LitState* state, const char* chars, size_t length)
{
    uint32_t hash;
    char* heap_chars;
    LitString* interned;
    hash= lit_util_hashstring(chars, length);
    interned = lit_table_find_string(&state->vm->strings, chars, length, hash);
    if(interned != NULL)
    {
        return interned;
    }
    /*
    heap_chars = LIT_ALLOCATE(state, sizeof(char), length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    */
    heap_chars = sdsnewlen(chars, length);
#ifdef LIT_LOG_ALLOCATION
    printf("Allocated new string '%s'\n", chars);
#endif
    return lit_string_makelen(state, heap_chars, length, hash, true, true);
}

const char* lit_string_getdata(LitString* ls)
{
    return ls->chars;
}

size_t lit_string_getlength(LitString* ls)
{
    if(ls->chars == NULL)
    {
        return 0;
    }
    return sdslen(ls->chars);
}

void lit_string_appendlen(LitString* ls, const char* s, size_t len)
{
    if(len > 0)
    {
        if(ls->chars == NULL)
        {
            ls->chars = sdsnewlen(s, len);
        }
        else
        {
            ls->chars = sdscatlen(ls->chars, s, len);
        }
    }
}

void lit_string_appendobj(LitString* ls, LitString* other)
{
    lit_string_appendlen(ls, other->chars, lit_string_getlength(other));
}

void lit_string_appendchar(LitString* ls, char ch)
{
    ls->chars = sdscatlen(ls->chars, (const char*)&ch, 1);
}

LitValue lit_string_numbertostring(LitState* state, double value)
{
    if(isnan(value))
    {
        return OBJECT_CONST_STRING(state, "nan");
    }

    if(isinf(value))
    {
        if(value > 0.0)
        {
            return OBJECT_CONST_STRING(state, "infinity");
        }
        else
        {
            return OBJECT_CONST_STRING(state, "-infinity");
        }
    }

    char buffer[24];
    int length = sprintf(buffer, "%.14g", value);

    return lit_value_objectvalue(lit_string_copy(state, buffer, length));
}


LitValue lit_string_format(LitState* state, const char* format, ...)
{
    char ch;
    size_t length;
    size_t total_length;
    bool was_allowed;
    const char* c;
    const char* strval;
    va_list arg_list;
    LitValue val;
    LitString* string;
    LitString* result;
    was_allowed = state->allow_gc;
    state->allow_gc = false;
    va_start(arg_list, format);
    total_length = strlen(format);
    va_end(arg_list);
    result = lit_string_makeempty(state, total_length + 1, false);
    va_start(arg_list, format);
    for(c = format; *c != '\0'; c++)
    {
        switch(*c)
        {
            case '$':
                {
                    strval = va_arg(arg_list, const char*);
                    if(strval != NULL)
                    {
                        length = strlen(strval);
                        result->chars = sdscatlen(result->chars, strval, length);
                    }
                    else
                    {
                        goto default_ending_copying;
                    }
                }
                break;
            case '@':
                {
                    val = va_arg(arg_list, LitValue);
                    if(lit_value_isstring(val))
                    {
                        string = lit_value_asstring(val);
                    }
                    else
                    {
                        //fprintf(stderr, "format: not a string, but a '%s'\n", lit_get_value_type(val));
                        //string = lit_to_string(state, val);
                        goto default_ending_copying;
                    }
                    if(string != NULL)
                    {
                        length = sdslen(string->chars);
                        if(length > 0)
                        {
                            result->chars = sdscatlen(result->chars, string->chars, length);
                        }
                    }
                    else
                    {
                        goto default_ending_copying;
                    }
                }
                break;
            case '#':
                {
                    string = lit_value_asstring(lit_string_numbertostring(state, va_arg(arg_list, double)));
                    length = sdslen(string->chars);
                    if(length > 0)
                    {
                        result->chars = sdscatlen(result->chars, string->chars, length);
                    }
                }
                break;
            default:
                {
                    default_ending_copying:
                    ch = *c;
                    result->chars = sdscatlen(result->chars, &ch, 1);
                }
                break;
        }
    }
    va_end(arg_list);
    result->hash = lit_util_hashstring(result->chars, lit_string_getlength(result));
    lit_state_regstring(state, result);
    state->allow_gc = was_allowed;
    return lit_value_objectvalue(result);
}

bool lit_string_equal(LitState* state, LitString* a, LitString* b)
{
    (void)state;
    if((a == NULL) || (b == NULL))
    {
        return false;
    }
    return (sdscmp(a->chars, b->chars) == 0);
}

LitValue util_invalid_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv);

static LitValue objfn_string_plus(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* selfstr;
    LitString* result;
    LitValue value;
    (void)argc;
    selfstr = lit_value_asstring(instance);
    value = argv[0];
    LitString* strval = NULL;
    if(lit_value_isstring(value))
    {
        strval = lit_value_asstring(value);
    }
    else
    {
        strval = lit_to_string(vm->state, value);
    }
    result = lit_string_makeempty(vm->state, lit_string_getlength(selfstr) + lit_string_getlength(strval), false);
    lit_string_appendobj(result, selfstr);
    lit_string_appendobj(result, strval);
    lit_state_regstring(vm->state, result);
    return lit_value_objectvalue(result);
}

static LitValue objfn_string_splice(LitVM* vm, LitString* string, int from, int to)
{
    int length;
    length = lit_ustring_length(string);
    if(from < 0)
    {
        from = length + from;
    }
    if(to < 0)
    {
        to = length + to;
    }
    from = fmax(from, 0);
    to = fmin(to, length - 1);
    if(from > to)
    {
        lit_runtime_error_exiting(vm, "String.splice argument 'from' is larger than argument 'to'");
    }
    from = lit_util_ucharoffset(string->chars, from);
    to = lit_util_ucharoffset(string->chars, to);
    return lit_value_objectvalue(lit_ustring_fromrange(vm->state, string, from, to - from + 1));
}

static LitValue objfn_string_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    LitString* c;
    LitString* string;
    if(lit_value_isrange(argv[0]))
    {
        LitRange* range = lit_value_asrange(argv[0]);
        return objfn_string_splice(vm, lit_value_asstring(instance), range->from, range->to);
    }
    string = lit_value_asstring(instance);
    index = lit_check_number(vm, argv, argc, 0);
    if(argc != 1)
    {
        lit_runtime_error_exiting(vm, "cannot modify strings with the subscript op");
    }
    if(index < 0)
    {
        index = lit_ustring_length(string) + index;

        if(index < 0)
        {
            return NULL_VALUE;
        }
    }
    c = lit_ustring_codepointat(vm->state, string, lit_util_ucharoffset(string->chars, index));
    return c == NULL ? NULL_VALUE : lit_value_objectvalue(c);
}


static LitValue objfn_string_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* self;
    LitString* other;
    (void)argc;
    self = lit_value_asstring(instance);
    if(lit_value_isstring(argv[0]))
    {
        other = lit_value_asstring(argv[0]);
        if(lit_string_getlength(self) == lit_string_getlength(other))
        {
            //fprintf(stderr, "string: same length(self=\"%s\" other=\"%s\")... strncmp=%d\n", self->chars, other->chars, strncmp(self->chars, other->chars, self->length));
            if(memcmp(self->chars, other->chars, lit_string_getlength(self)) == 0)
            {
                return TRUE_VALUE;
            }
        }
        return FALSE_VALUE;
    }
    else if(lit_value_isnull(argv[0]))
    {
        if((self == NULL) || lit_value_isnull(instance))
        {
            return TRUE_VALUE;
        }
        return FALSE_VALUE;
    }
    lit_runtime_error_exiting(vm, "can only compare string to another string or null");
    return FALSE_VALUE;
}

static LitValue objfn_string_less(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    return lit_bool_to_value(vm->state, strcmp(lit_value_asstring(instance)->chars, lit_check_string(vm, argv, argc, 0)) < 0);
}

static LitValue objfn_string_greater(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    return lit_bool_to_value(vm->state, strcmp(lit_value_asstring(instance)->chars, lit_check_string(vm, argv, argc, 0)) > 0);
}

static LitValue objfn_string_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return instance;
}

static LitValue objfn_string_tonumber(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    double result;
    (void)vm;
    (void)argc;
    (void)argv;
    result = strtod(lit_value_asstring(instance)->chars, NULL);
    if(errno == ERANGE)
    {
        errno = 0;
        return NULL_VALUE;
    }
    return lit_number_to_value(vm->state, result);
}

static LitValue objfn_string_touppercase(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    char* buffer;
    LitString* rt;
    LitString* string;
    string = lit_value_asstring(instance);
    (void)argc;
    (void)argv;
    buffer = LIT_ALLOCATE(vm->state, sizeof(char), lit_string_getlength(string) + 1);
    for(i = 0; i < lit_string_getlength(string); i++)
    {
        buffer[i] = (char)toupper(string->chars[i]);
    }
    buffer[i] = 0;
    rt = lit_string_take(vm->state, buffer, lit_string_getlength(string), false);
    return lit_value_objectvalue(rt);
}

static LitValue objfn_string_tolowercase(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    char* buffer;
    LitString* rt;
    LitString* string;
    string = lit_value_asstring(instance);
    (void)argc;
    (void)argv;
    buffer = LIT_ALLOCATE(vm->state, sizeof(char), lit_string_getlength(string) + 1);
    for(i = 0; i < lit_string_getlength(string); i++)
    {
        buffer[i] = (char)tolower(string->chars[i]);
    }
    buffer[i] = 0;
    rt = lit_string_take(vm->state, buffer, lit_string_getlength(string), false);
    return lit_value_objectvalue(rt);
}

static LitValue objfn_string_tobyte(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int iv;
    LitString* selfstr;
    (void)vm;
    (void)argc;
    (void)argv;
    selfstr = lit_value_asstring(instance);
    iv = lit_ustring_decode((const uint8_t*)selfstr->chars, lit_string_getlength(selfstr));
    return lit_number_to_value(vm->state, iv);
}

static LitValue objfn_string_contains(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LitString* sub;
    LitString* string;
    string = lit_value_asstring(instance);
    sub = lit_check_object_string(vm, argv, argc, 0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    return lit_bool_to_value(vm->state, strstr(string->chars, sub->chars) != NULL);
}

static LitValue objfn_string_startswith(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitString* sub;
    LitString* string;
    string = lit_value_asstring(instance);
    sub = lit_check_object_string(vm, argv, argc, 0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    if(lit_string_getlength(sub) > lit_string_getlength(string))
    {
        return FALSE_VALUE;
    }
    for(i = 0; i < lit_string_getlength(sub); i++)
    {
        if(sub->chars[i] != string->chars[i])
        {
            return FALSE_VALUE;
        }
    }
    return TRUE_VALUE;
}

static LitValue objfn_string_endswith(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    size_t start;
    LitString* sub;
    LitString* string;
    string = lit_value_asstring(instance);
    sub = lit_check_object_string(vm, argv, argc, 0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    if(lit_string_getlength(sub) > lit_string_getlength(string))
    {
        return FALSE_VALUE;
    }
    start = lit_string_getlength(string) - lit_string_getlength(sub);
    for(i = 0; i < lit_string_getlength(sub); i++)
    {
        if(sub->chars[i] != string->chars[i + start])
        {
            return FALSE_VALUE;
        }
    }
    return TRUE_VALUE;
}


static LitValue objfn_string_replace(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    size_t buffer_length;
    size_t buffer_index;
    char* buffer;
    LitString* string;
    LitString* what;
    LitString* with;
    LIT_ENSURE_ARGS(2);
    if(!lit_value_isstring(argv[0]) || !lit_value_isstring(argv[1]))
    {
        lit_runtime_error_exiting(vm, "expected 2 string arguments");
    }
    string = lit_value_asstring(instance);
    what = lit_value_asstring(argv[0]);
    with = lit_value_asstring(argv[1]);
    buffer_length = 0;
    for(i = 0; i < lit_string_getlength(string); i++)
    {
        if(strncmp(string->chars + i, what->chars, lit_string_getlength(what)) == 0)
        {
            i += lit_string_getlength(what) - 1;
            buffer_length += lit_string_getlength(with);
        }
        else
        {
            buffer_length++;
        }
    }
    buffer_index = 0;
    buffer = LIT_ALLOCATE(vm->state, sizeof(char), buffer_length+1);
    for(i = 0; i < lit_string_getlength(string); i++)
    {
        if(strncmp(string->chars + i, what->chars, lit_string_getlength(what)) == 0)
        {
            memcpy(buffer + buffer_index, with->chars, lit_string_getlength(with));
            buffer_index += lit_string_getlength(with);
            i += lit_string_getlength(what) - 1;
        }
        else
        {
            buffer[buffer_index] = string->chars[i];
            buffer_index++;
        }
    }
    buffer[buffer_length] = '\0';
    return lit_value_objectvalue(lit_string_take(vm->state, buffer, buffer_length, false));
}

static LitValue objfn_string_substring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int to;
    int from;
    from = lit_check_number(vm, argv, argc, 0);
    to = lit_check_number(vm, argv, argc, 1);
    return objfn_string_splice(vm, lit_value_asstring(instance), from, to);
}


static LitValue objfn_string_byteat(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int idx;
    idx = lit_check_number(vm, argv, argc, 0);
    return lit_number_to_value(vm->state, lit_value_asstring(instance)->chars[idx]);
}

static LitValue objfn_string_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(vm->state, lit_ustring_length(lit_value_asstring(instance)));
}

static LitValue objfn_string_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    LitString* string;
    string = lit_value_asstring(instance);
    if(lit_value_isnull(argv[0]))
    {
        if(lit_string_getlength(string) == 0)
        {
            return NULL_VALUE;
        }
        return lit_number_to_value(vm->state, 0);
    }
    index = lit_check_number(vm, argv, argc, 0);
    if(index < 0)
    {
        return NULL_VALUE;
    }
    do
    {
        index++;
        if(index >= (int)lit_string_getlength(string))
        {
            return NULL_VALUE;
        }
    } while((string->chars[index] & 0xc0) == 0x80);
    return lit_number_to_value(vm->state, index);
}


static LitValue objfn_string_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint32_t index;
    LitString* string;
    string = lit_value_asstring(instance);
    index = lit_check_number(vm, argv, argc, 0);
    if(index == UINT32_MAX)
    {
        return false;
    }
    return lit_value_objectvalue(lit_ustring_codepointat(vm->state, string, index));
}


bool check_fmt_arg(LitVM* vm, char* buf, size_t ai, size_t argc, LitValue* argv, const char* fmttext)
{
    (void)argv;
    if(ai <= argc)
    {
        return true;
    }
    sdsfree(buf);
    lit_runtime_error_exiting(vm, "too few arguments for format flag '%s' at position %d (argc=%d)", fmttext, ai, argc);
    return false;
}

static LitValue objfn_string_format(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    char ch;
    char pch;
    char nch;
    char tmpch;
    int iv;
    size_t i;
    size_t ai;
    size_t selflen;
    LitString* selfstr;
    LitValue rtv;
    char* buf;
    (void)pch;
    selfstr = lit_value_asstring(instance);
    selflen = lit_string_getlength(selfstr);
    buf = sdsempty();
    buf = sdsMakeRoomFor(buf, selflen + 10);
    ai = 0;
    ch = -1;
    pch = -1;
    nch = -1;
    for(i=0; i<selflen; i++)
    {
        pch = ch;
        ch = selfstr->chars[i];
        if(i <= selflen)
        {
            nch = selfstr->chars[i + 1];
        }
        if(ch == '%')
        {
            if(nch == '%')
            {
                buf = sdscatlen(buf, &ch, 1);
                i += 1;
            }
            else
            {
                i++;
                switch(nch)
                {
                    case 's':
                        {
                            if(!check_fmt_arg(vm, buf, ai, argc, argv, "%s"))
                            {
                                return NULL_VALUE;
                            }
                            buf = sdscatlen(buf, lit_value_asstring(argv[ai])->chars, lit_string_getlength(lit_value_asstring(argv[ai])));
                        }
                        break;
                    case 'd':
                    case 'i':
                        {
                            if(!check_fmt_arg(vm, buf, ai, argc, argv, "%d"))
                            {
                                return NULL_VALUE;
                            }
                            if(lit_value_isnumber(argv[ai]))
                            {
                                iv = lit_check_number(vm, argv, argc, ai);
                                buf = sdscatfmt(buf, "%i", iv);
                            }
                            break;
                        }
                        break;
                    case 'c':
                        {
                            if(!check_fmt_arg(vm, buf, ai, argc, argv, "%d"))
                            {
                                return NULL_VALUE;
                            }
                            if(!lit_value_isnumber(argv[ai]))
                            {
                                sdsfree(buf);
                                lit_runtime_error_exiting(vm, "flag 'c' expects a number");
                            }
                            iv = lit_check_number(vm, argv, argc, ai);
                            /* TODO: both of these use the same amount of memory. but which is faster? */
                            #if 0
                                buf = sdscatfmt(buf, "%c", iv);
                            #else
                                tmpch = iv;
                                buf = sdscatlen(buf, &tmpch, 1);
                            #endif
                        }
                        break;
                    default:
                        {
                            sdsfree(buf);
                            lit_runtime_error_exiting(vm, "unrecognized formatting flag '%c'", nch);
                            return NULL_VALUE;
                        }
                        break;
                }
                ai++;
            }
        }
        else
        {
            buf = sdscatlen(buf, &ch, 1);
        }
    }
    rtv = lit_value_objectvalue(lit_string_copy(vm->state, buf, sdslen(buf)));
    sdsfree(buf);
    return rtv;
}

void lit_open_string_library(LitState* state)
{
    {
        LIT_BEGIN_CLASS("String");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(util_invalid_constructor);
            LIT_BIND_METHOD("+", objfn_string_plus);
            LIT_BIND_METHOD("[]", objfn_string_subscript);
            LIT_BIND_METHOD("<", objfn_string_less);
            LIT_BIND_METHOD(">", objfn_string_greater);
            LIT_BIND_METHOD("==", objfn_string_compare);
            LIT_BIND_METHOD("toString", objfn_string_tostring);

            // js-isms
            LIT_BIND_METHOD("charCodeAt", objfn_string_byteat);
            LIT_BIND_METHOD("charAt", objfn_string_subscript);

            {
                LIT_BIND_METHOD("toNumber", objfn_string_tonumber);
                LIT_BIND_GETTER("to_i", objfn_string_tonumber);
            }
            {
                LIT_BIND_METHOD("toUpperCase", objfn_string_touppercase);
                LIT_BIND_METHOD("toUpper", objfn_string_touppercase);
                LIT_BIND_GETTER("upper", objfn_string_touppercase);
            }
            {
                LIT_BIND_METHOD("toLowerCase", objfn_string_tolowercase);
                LIT_BIND_METHOD("toLower", objfn_string_tolowercase);
                LIT_BIND_GETTER("lower", objfn_string_tolowercase);
            }
            {
                LIT_BIND_GETTER("toByte", objfn_string_tobyte);
            }
            LIT_BIND_METHOD("contains", objfn_string_contains);
            LIT_BIND_METHOD("startsWith", objfn_string_startswith);
            LIT_BIND_METHOD("endsWith", objfn_string_endswith);
            LIT_BIND_METHOD("replace", objfn_string_replace);
            LIT_BIND_METHOD("substring", objfn_string_substring);
            LIT_BIND_METHOD("iterator", objfn_string_iterator);
            LIT_BIND_METHOD("iteratorValue", objfn_string_iteratorvalue);
            LIT_BIND_GETTER("length", objfn_string_length);
            LIT_BIND_METHOD("format", objfn_string_format);
            state->stringvalue_class = klass;
        }
        LIT_END_CLASS();
    }
}

