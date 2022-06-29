
#include "lit.h"

LitValue util_invalid_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv);

static LitValue objfn_string_plus(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t length;
    LitString* string;
    LitString* result;
    LitValue value;
    (void)argc;
    string = AS_STRING(instance);
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
    length = string->length + strval->length;
    result = lit_allocate_empty_string(vm->state, length);
    result->chars = LIT_ALLOCATE(vm->state, char, length + 1);
    result->chars[length] = '\0';
    memcpy(result->chars, string->chars, string->length);
    memcpy(result->chars + string->length, strval->chars, strval->length);
    result->hash = lit_hash_string(result->chars, result->length);
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

static LitValue objfn_string_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitString* self;
    LitString* other;
    self = AS_STRING(instance);
    if(IS_STRING(argv[0]))
    {
        other = AS_STRING(argv[0]);
        if(self->length == other->length)
        {
            //fprintf(stderr, "string: same length(self=\"%s\" other=\"%s\")... strncmp=%d\n", self->chars, other->chars, strncmp(self->chars, other->chars, self->length));
            if(memcmp(self->chars, other->chars, self->length) == 0)
            {
                return TRUE_VALUE;
            }
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
    buffer = LIT_ALLOCATE(vm->state, char, string->length + 1);
    for(i = 0; i < string->length; i++)
    {
        buffer[i] = (char)toupper(string->chars[i]);
    }
    buffer[i] = 0;
    rt = lit_take_string(vm->state, buffer, string->length);
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
    buffer = LIT_ALLOCATE(vm->state, char, string->length + 1);
    for(i = 0; i < string->length; i++)
    {
        buffer[i] = (char)tolower(string->chars[i]);
    }
    buffer[i] = 0;
    rt = lit_take_string(vm->state, buffer, string->length);
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
    if(sub->length > string->length)
    {
        return FALSE_VALUE;
    }
    for(i = 0; i < sub->length; i++)
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
    if(sub->length > string->length)
    {
        return FALSE_VALUE;
    }
    start = string->length - sub->length;
    for(i = 0; i < sub->length; i++)
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
    for(i = 0; i < string->length; i++)
    {
        if(strncmp(string->chars + i, what->chars, what->length) == 0)
        {
            i += what->length - 1;
            buffer_length += with->length;
        }
        else
        {
            buffer_length++;
        }
    }
    buffer_index = 0;
    buffer = LIT_ALLOCATE(vm->state, char, buffer_length+1);
    for(i = 0; i < string->length; i++)
    {
        if(strncmp(string->chars + i, what->chars, what->length) == 0)
        {
            memcpy(buffer + buffer_index, with->chars, with->length);
            buffer_index += with->length;
            i += what->length - 1;
        }
        else
        {
            buffer[buffer_index] = string->chars[i];
            buffer_index++;
        }
    }
    buffer[buffer_length] = '\0';
    return OBJECT_VALUE(lit_take_string(vm->state, buffer, buffer_length));
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
        if(string->length == 0)
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
        if(index >= (int)string->length)
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

