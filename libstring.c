
#include "lit.h"
#include "sds.h"

/*
int sdsHdrSize(char type);
char sdsReqType(size_t string_size);
size_t sdslen(const dynstring_t *s);
size_t sdsavail(const dynstring_t *s);
void sdssetlen(dynstring_t *s, size_t newlen);
void sdsinclen(dynstring_t *s, size_t inc);
size_t sdsalloc(const dynstring_t *s);
void sdssetalloc(dynstring_t *s, size_t newlen);
dynstring_t *sdsnewlen(const void *init, size_t initlen);
dynstring_t *sdsempty(void);
dynstring_t *sdsnew(const char *init);
dynstring_t *sdsdup(const dynstring_t *s);
void sdsfree(dynstring_t *s);
void sdsupdatelen(dynstring_t *s);
void sdsclear(dynstring_t *s);
dynstring_t *sdsMakeRoomFor(dynstring_t *s, size_t addlen);
dynstring_t *sdsRemoveFreeSpace(dynstring_t *s);
size_t sdsAllocSize(dynstring_t *s);
void *sdsAllocPtr(dynstring_t *s);
void sdsIncrLen(dynstring_t *s, ssize_t incr);
dynstring_t *sdsgrowzero(dynstring_t *s, size_t len);
dynstring_t *sdscatlen(dynstring_t *s, const void *t, size_t len);
dynstring_t *sdscat(dynstring_t *s, const char *t);
dynstring_t *sdscatsds(dynstring_t *s, const dynstring_t *t);
dynstring_t *sdscpylen(dynstring_t *s, const char *t, size_t len);
dynstring_t *sdscpy(dynstring_t *s, const char *t);
int sdsll2str(char *s, long long value);
int sdsull2str(char *s, unsigned long long v);
dynstring_t *sdsfromlonglong(long long value);
dynstring_t *sdscatvprintf(dynstring_t *s, const char *fmt, va_list ap);
dynstring_t *sdscatprintf(dynstring_t *s, const char *fmt, ...);
dynstring_t *sdscatfmt(dynstring_t *s, char const *fmt, ...);
dynstring_t *sdstrim(dynstring_t *s, const char *cset);
void sdsrange(dynstring_t *s, ssize_t start, ssize_t end);
void sdstolower(dynstring_t *s);
void sdstoupper(dynstring_t *s);
int sdscmp(const dynstring_t *s1, const dynstring_t *s2);
dynstring_t **sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count);
void sdsfreesplitres(dynstring_t **tokens, int count);
dynstring_t *sdscatrepr(dynstring_t *s, const char *p, size_t len);
int is_hex_digit(char c);
int hex_digit_to_int(char c);
dynstring_t **sdssplitargs(const char *line, int *argc);
dynstring_t *sdsmapchars(dynstring_t *s, const char *from, const char *to, size_t setlen);
dynstring_t *sdsjoin(char **argv, int argc, char *sep);
dynstring_t *sdsjoinsds(dynstring_t **argv, int argc, const char *sep, size_t seplen);
void *sds_malloc(size_t size);
void *sds_realloc(void *ptr, size_t size);
void sds_free(void *ptr);
*/

uint32_t lit_hash_string(const char* key, size_t length)
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

LitString* lit_string_alloc_empty(LitState* state, size_t length)
{
    LitString* string;
    string = ALLOCATE_OBJECT(state, LitString, LITTYPE_STRING);
    string->chars = sdsempty();
    string->chars = sdsMakeRoomFor(string->chars, length);
    //string->chars = NULL;
    string->hash = 0;
    //sdsMakeRoomFor(string->chars, length);
    return string;
}


LitString* lit_string_alloc(LitState* state, char* chars, size_t length, uint32_t hash)
{
    LitString* string;
    string = lit_string_alloc_empty(state, length);
    //string->chars = sdsnewlen(chars, length);
    string->chars = sdscatlen(string->chars, chars, length);
    string->hash = hash;
    LIT_FREE(state, char, chars);
    lit_register_string(state, string);
    return string;
}

void lit_register_string(LitState* state, LitString* string)
{
    if(lit_string_length(string) > 0)
    {
        lit_push_root(state, (LitObject*)string);
        lit_table_set(state, &state->vm->strings, string, NULL_VALUE);
        lit_pop_root(state);
    }
}

/* todo: track if $chars is a sds instance - additional argument $fromsds? */
LitString* lit_string_take(LitState* state, char* chars, size_t length)
{
    uint32_t hash;
    hash = lit_hash_string(chars, length);
    LitString* interned;
    interned = lit_table_find_string(&state->vm->strings, chars, length, hash);
    if(interned != NULL)
    {
        LIT_FREE(state, char, chars);
        //sdsfree(chars);
        return interned;
    }
    return lit_string_alloc(state, (char*)chars, length, hash);
}

LitString* lit_string_copy(LitState* state, const char* chars, size_t length)
{
    uint32_t hash;
    char* heap_chars;
    LitString* interned;
    hash= lit_hash_string(chars, length);
    interned = lit_table_find_string(&state->vm->strings, chars, length, hash);
    if(interned != NULL)
    {
        return interned;
    }
    heap_chars = LIT_ALLOCATE(state, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
#ifdef LIT_LOG_ALLOCATION
    printf("Allocated new string '%s'\n", chars);
#endif
    return lit_string_alloc(state, heap_chars, length, hash);
}

size_t lit_string_length(LitString* ls)
{
    if(ls->chars == NULL)
    {
        return 0;
    }
    return sdslen(ls->chars);
}

void lit_string_append_string(LitString* ls, const char* s, size_t len)
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

void lit_string_append_strobj(LitString* ls, LitString* other)
{
    lit_string_append_string(ls, other->chars, lit_string_length(other));
}

LitValue lit_string_number_to_string(LitState* state, double value)
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

    return OBJECT_VALUE(lit_string_copy(state, buffer, length));
}


LitValue lit_string_format(LitState* state, const char* format, ...)
{
    char ch;
    size_t length;
    size_t total_length;
    bool was_allowed;
    //char* start;
    const char* c;
    const char* cc;
    const char* strval;
    va_list arg_list;
    LitValue val;
    LitString* ss;
    LitString* string;
    LitString* result;
    was_allowed = state->allow_gc;
    state->allow_gc = false;
    va_start(arg_list, format);
    total_length = strlen(format);
    /* thanks to sds, this is mostly unnecessary */
    /*
    for(c = format; *c != '\0'; c++)
    {
        switch(*c)
        {
            case '$':
                {
                    cc = va_arg(arg_list, const char*);
                    if(cc != NULL)
                    {
                        total_length += strlen(cc);
                        break;
                    }
                    goto default_ending;
                }
                break;                
            case '@':
                {
                    LitValue v = va_arg(arg_list, LitValue);
                    ss = AS_STRING(v);
                    if(ss != NULL)
                    {
                        total_length += lit_string_length(ss);
                        break;
                    }
                    goto default_ending;
                }
                break;
            case '#':
                {
                    total_length += lit_string_length(AS_STRING(lit_string_number_to_string(state, va_arg(arg_list, double))));
                    break;
                }
                break;                
            default:
                {
                default_ending:
                    total_length++;
                }
                break;

        }
    }
    */
    va_end(arg_list);
    result = lit_string_alloc_empty(state, total_length + 1);
    //result->chars = LIT_ALLOCATE(state, char, total_length + 1);
    //result->chars[total_length] = '\0';
    //start = result->chars;
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
                        //memcpy(start, strval, length);
                        //start += length;
                        result->chars = sdscatlen(result->chars, strval, length);
                        break;
                    }
                    goto default_ending_copying;
                }
                break;
            case '@':
                {
                    val = va_arg(arg_list, LitValue);
                    if(IS_STRING(val))
                    {
                        string = AS_STRING(val);
                        if(string != NULL)
                        {
                            //memcpy(start, string->chars, lit_string_length(string));
                            //start += lit_string_length(string);
                            length = sdslen(string->chars);
                            if(length > 0)
                            {
                                result->chars = sdscatlen(result->chars, string->chars, length);
                            }
                            break;
                        }
                    }
                    goto default_ending_copying;
                }
                break;

            case '#':
                {
                    string = AS_STRING(lit_string_number_to_string(state, va_arg(arg_list, double)));
                    //memcpy(start, string->chars, lit_string_length(string));
                    //start += lit_string_length(string);
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
                    // *start++ = *c;
                    ch = *c;
                    result->chars = sdscatlen(result->chars, &ch, 1);
                }
                break;
        }
    }
    va_end(arg_list);
    result->hash = lit_hash_string(result->chars, lit_string_length(result));
    lit_register_string(state, result);
    state->allow_gc = was_allowed;
    return OBJECT_VALUE(result);
}


LitValue util_invalid_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv);

static LitValue objfn_string_plus(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t length;
    LitString* selfstr;
    LitString* result;
    LitValue value;
    (void)argc;
    selfstr = AS_STRING(instance);
    value = argv[0];
    LitString* strval = NULL;
    if(IS_STRING(value))
    {
        strval = AS_STRING(value);
    }
    else
    {
        strval = lit_to_string(vm->state, value);
    }
    result = lit_string_alloc_empty(vm->state, lit_string_length(selfstr) + lit_string_length(strval));
    lit_string_append_strobj(result, selfstr);
    lit_string_append_strobj(result, strval);
    lit_register_string(vm->state, result);
    return OBJECT_VALUE(result);
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
    from = lit_uchar_offset(string->chars, from);
    to = lit_uchar_offset(string->chars, to);
    return OBJECT_VALUE(lit_ustring_from_range(vm->state, string, from, to - from + 1));
}

static LitValue objfn_string_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    LitString* c;
    LitString* string;
    if(IS_RANGE(argv[0]))
    {
        LitRange* range = AS_RANGE(argv[0]);
        return objfn_string_splice(vm, AS_STRING(instance), range->from, range->to);
    }
    string = AS_STRING(instance);
    index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
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
    c = lit_ustring_code_point_at(vm->state, string, lit_uchar_offset(string->chars, index));
    return c == NULL ? NULL_VALUE : OBJECT_VALUE(c);
}

/*
static LitValue string_format(LitState* state, size_t argc, LitValue* argv)
{
    
}
*/

static LitValue objfn_string_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitString* self;
    LitString* other;
    self = AS_STRING(instance);
    if(IS_STRING(argv[0]))
    {
        other = AS_STRING(argv[0]);
        if(lit_string_length(self) == lit_string_length(other))
        {
            //fprintf(stderr, "string: same length(self=\"%s\" other=\"%s\")... strncmp=%d\n", self->chars, other->chars, strncmp(self->chars, other->chars, self->length));
            if(memcmp(self->chars, other->chars, lit_string_length(self)) == 0)
            {
                return TRUE_VALUE;
            }
        }
        return FALSE_VALUE;
    }
    else if(IS_NULL(argv[0]))
    {
        if((self == NULL) || IS_NULL(instance))
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
    return BOOL_VALUE(strcmp(AS_STRING(instance)->chars, LIT_CHECK_STRING(vm, argv, argc, 0)) < 0);
}

static LitValue objfn_string_greater(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    return BOOL_VALUE(strcmp(AS_STRING(instance)->chars, LIT_CHECK_STRING(vm, argv, argc, 0)) > 0);
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
    result = strtod(AS_STRING(instance)->chars, NULL);
    if(errno == ERANGE)
    {
        errno = 0;
        return NULL_VALUE;
    }
    return lit_number_to_value(result);
}


static LitValue objfn_string_touppercase(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    char* buffer;
    LitString* rt;
    LitString* string;
    string = AS_STRING(instance);
    (void)argc;
    (void)argv;
    buffer = LIT_ALLOCATE(vm->state, char, lit_string_length(string) + 1);
    for(i = 0; i < lit_string_length(string); i++)
    {
        buffer[i] = (char)toupper(string->chars[i]);
    }
    buffer[i] = 0;
    rt = lit_string_take(vm->state, buffer, lit_string_length(string));
    return OBJECT_VALUE(rt);
}


static LitValue objfn_string_tolowercase(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    char* buffer;
    LitString* rt;
    LitString* string;
    string = AS_STRING(instance);
    (void)argc;
    (void)argv;
    buffer = LIT_ALLOCATE(vm->state, char, lit_string_length(string) + 1);
    for(i = 0; i < lit_string_length(string); i++)
    {
        buffer[i] = (char)tolower(string->chars[i]);
    }
    buffer[i] = 0;
    rt = lit_string_take(vm->state, buffer, lit_string_length(string));
    return OBJECT_VALUE(rt);
}


static LitValue objfn_string_contains(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LitString* sub;
    LitString* string;
    string = AS_STRING(instance);
    sub = LIT_CHECK_OBJECT_STRING(0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    return BOOL_VALUE(strstr(string->chars, sub->chars) != NULL);
}


static LitValue objfn_string_startswith(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitString* sub;
    LitString* string;
    string = AS_STRING(instance);
    sub = LIT_CHECK_OBJECT_STRING(0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    if(lit_string_length(sub) > lit_string_length(string))
    {
        return FALSE_VALUE;
    }
    for(i = 0; i < lit_string_length(sub); i++)
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
    string = AS_STRING(instance);
    sub = LIT_CHECK_OBJECT_STRING(0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    if(lit_string_length(sub) > lit_string_length(string))
    {
        return FALSE_VALUE;
    }
    start = lit_string_length(string) - lit_string_length(sub);
    for(i = 0; i < lit_string_length(sub); i++)
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
    if(!IS_STRING(argv[0]) || !IS_STRING(argv[1]))
    {
        lit_runtime_error_exiting(vm, "expected 2 string arguments");
    }
    string = AS_STRING(instance);
    what = AS_STRING(argv[0]);
    with = AS_STRING(argv[1]);
    buffer_length = 0;
    for(i = 0; i < lit_string_length(string); i++)
    {
        if(strncmp(string->chars + i, what->chars, lit_string_length(what)) == 0)
        {
            i += lit_string_length(what) - 1;
            buffer_length += lit_string_length(with);
        }
        else
        {
            buffer_length++;
        }
    }
    buffer_index = 0;
    buffer = LIT_ALLOCATE(vm->state, char, buffer_length+1);
    for(i = 0; i < lit_string_length(string); i++)
    {
        if(strncmp(string->chars + i, what->chars, lit_string_length(what)) == 0)
        {
            memcpy(buffer + buffer_index, with->chars, lit_string_length(with));
            buffer_index += lit_string_length(with);
            i += lit_string_length(what) - 1;
        }
        else
        {
            buffer[buffer_index] = string->chars[i];
            buffer_index++;
        }
    }
    buffer[buffer_length] = '\0';
    return OBJECT_VALUE(lit_string_take(vm->state, buffer, buffer_length));
}

static LitValue objfn_string_substring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int to;
    int from;
    from = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    to = LIT_CHECK_NUMBER(vm, argv, argc, 1);
    return objfn_string_splice(vm, AS_STRING(instance), from, to);
}


static LitValue objfn_string_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_ustring_length(AS_STRING(instance)));
}

static LitValue objfn_string_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    LitString* string;
    string = AS_STRING(instance);
    if(IS_NULL(argv[0]))
    {
        if(lit_string_length(string) == 0)
        {
            return NULL_VALUE;
        }
        return lit_number_to_value(0);
    }
    index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    if(index < 0)
    {
        return NULL_VALUE;
    }
    do
    {
        index++;
        if(index >= (int)lit_string_length(string))
        {
            return NULL_VALUE;
        }
    } while((string->chars[index] & 0xc0) == 0x80);
    return lit_number_to_value(index);
}


static LitValue objfn_string_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint32_t index;
    LitString* string;
    string = AS_STRING(instance);
    index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    if(index == UINT32_MAX)
    {
        return false;
    }
    return OBJECT_VALUE(lit_ustring_code_point_at(vm->state, string, index));
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
            LIT_BIND_METHOD("toNumber", objfn_string_tonumber);
            LIT_BIND_METHOD("toUpperCase", objfn_string_touppercase);
            LIT_BIND_METHOD("toUpper", objfn_string_touppercase);
            LIT_BIND_METHOD("toLowerCase", objfn_string_tolowercase);
            LIT_BIND_METHOD("toLower", objfn_string_tolowercase);
            LIT_BIND_METHOD("contains", objfn_string_contains);
            LIT_BIND_METHOD("startsWith", objfn_string_startswith);
            LIT_BIND_METHOD("endsWith", objfn_string_endswith);
            LIT_BIND_METHOD("replace", objfn_string_replace);
            LIT_BIND_METHOD("substring", objfn_string_substring);
            LIT_BIND_METHOD("iterator", objfn_string_iterator);
            LIT_BIND_METHOD("iteratorValue", objfn_string_iteratorvalue);
            LIT_BIND_GETTER("length", objfn_string_length);
            state->stringvalue_class = klass;
        }
        LIT_END_CLASS();
    }
}

