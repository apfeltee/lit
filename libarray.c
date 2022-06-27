
#include "lit.h"

int lit_array_indexof(LitArray* array, LitValue value)
{
    size_t i;
    for(i = 0; i < array->values.count; i++)
    {
        if(array->values.values[i] == value)
        {
            return (int)i;
        }
    }
    return -1;
}


LitValue lit_array_removeat(LitArray* array, size_t index)
{
    size_t i;
    size_t count;
    LitValue value;
    LitValues* values;
    values = &array->values;
    count = values->count;
    if(index >= count)
    {
        return NULL_VALUE;
    }
    value = values->values[index];
    if(index == count - 1)
    {
        values->values[index] = NULL_VALUE;
    }
    else
    {
        for(i = index; i < values->count - 1; i++)
        {
            values->values[i] = values->values[i + 1];
        }
        values->values[count - 1] = NULL_VALUE;
    }
    values->count--;
    return value;
}

void lit_array_push(LitState* state, LitArray* array, LitValue val)
{
    lit_values_write(state, &array->values, val);
}

static LitValue objfn_array_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_create_array(vm->state));
}

static LitValue objfn_array_splice(LitVM* vm, LitArray* array, int from, int to)
{
    size_t i;
    size_t length;
    LitArray* new_array;
    length = array->values.count;
    if(from < 0)
    {
        from = (int)length + from;
    }
    if(to < 0)
    {
        to = (int)length + to;
    }
    if(from > to)
    {
        lit_runtime_error_exiting(vm, "Array.splice argument 'from' is larger than argument 'to'");
    }
    from = fmax(from, 0);
    to = fmin(to, (int)length - 1);
    length = fmin(length, to - from + 1);
    new_array = lit_create_array(vm->state);
    for(i = 0; i < length; i++)
    {
        lit_values_write(vm->state, &new_array->values, array->values.values[from + i]);
    }
    return OBJECT_VALUE(new_array);
}

static LitValue objfn_array_slice(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int from;
    int to;
    from = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    to = LIT_CHECK_NUMBER(vm, argv, argc, 1);

    return objfn_array_splice(vm, AS_ARRAY(instance), from, to);
}


static LitValue objfn_array_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    if(argc == 2)
    {
        if(!IS_NUMBER(argv[0]))
        {
            lit_runtime_error_exiting(vm, "array index must be a number");
        }

        LitValues* values = &AS_ARRAY(instance)->values;
        int index = lit_value_to_number(argv[0]);

        if(index < 0)
        {
            index = fmax(0, values->count + index);
        }

        lit_values_ensure_size(vm->state, values, index + 1);
        return values->values[index] = argv[1];
    }

    if(!IS_NUMBER(argv[0]))
    {
        if(IS_RANGE(argv[0]))
        {
            LitRange* range = AS_RANGE(argv[0]);
            return objfn_array_splice(vm, AS_ARRAY(instance), (int)range->from, (int)range->to);
        }

        lit_runtime_error_exiting(vm, "array index must be a number");
        return NULL_VALUE;
    }

    LitValues* values = &AS_ARRAY(instance)->values;
    int index = lit_value_to_number(argv[0]);

    if(index < 0)
    {
        index = fmax(0, values->count + index);
    }

    if(values->capacity <= (size_t)index)
    {
        return NULL_VALUE;
    }

    return values->values[index];
}


static LitValue objfn_array_add(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    for(i=0; i<argc; i++)
    {
        lit_array_push(vm->state, AS_ARRAY(instance), argv[i]);
    }
    return NULL_VALUE;
}


static LitValue objfn_array_insert(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(2)

        LitValues* values = &AS_ARRAY(instance)->values;
    int index = LIT_CHECK_NUMBER(vm, argv, argc, 0);

    if(index < 0)
    {
        index = fmax(0, values->count + index);
    }

    LitValue value = argv[1];

    if((int)values->count <= index)
    {
        lit_values_ensure_size(vm->state, values, index + 1);
    }
    else
    {
        lit_values_ensure_size(vm->state, values, values->count + 1);

        for(int i = values->count - 1; i > index; i--)
        {
            values->values[i] = values->values[i - 1];
        }
    }

    values->values[index] = value;
    return NULL_VALUE;
}


static LitValue objfn_array_addall(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

        if(!IS_ARRAY(argv[0]))
    {
        lit_runtime_error_exiting(vm, "expected array as the argument");
    }

    LitArray* array = AS_ARRAY(instance);
    LitArray* toAdd = AS_ARRAY(argv[0]);

    for(size_t i = 0; i < toAdd->values.count; i++)
    {
        lit_array_push(vm->state, array, toAdd->values.values[i]);
    }
    return NULL_VALUE;
}


static LitValue objfn_array_indexof(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

        int index = lit_array_indexof(AS_ARRAY(instance), argv[0]);
    return index == -1 ? NULL_VALUE : lit_number_to_value(index);
}


static LitValue objfn_array_remove(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

        LitArray* array = AS_ARRAY(instance);
    int index = lit_array_indexof(array, argv[0]);

    if(index != -1)
    {
        return lit_array_removeat(array, (size_t)index);
    }

    return NULL_VALUE;
}


static LitValue objfn_array_removeat(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index = LIT_CHECK_NUMBER(vm, argv, argc, 0);

    if(index < 0)
    {
        return NULL_VALUE;
    }

    return lit_array_removeat(AS_ARRAY(instance), (size_t)index);
}


static LitValue objfn_array_contains(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)
        return BOOL_VALUE(lit_array_indexof(AS_ARRAY(instance), argv[0]) != -1);
}


static LitValue objfn_array_clear(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    AS_ARRAY(instance)->values.count = 0;
    return NULL_VALUE;
}


static LitValue objfn_array_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int number;
    LitArray* array;
    (void)vm;
    LIT_ENSURE_ARGS(1)
    array = AS_ARRAY(instance);
    number = 0;
    if(IS_NUMBER(argv[0]))
    {
        number = lit_value_to_number(argv[0]);
        if(number >= (int)array->values.count - 1)
        {
            return NULL_VALUE;
        }
        number++;
    }
    return array->values.count == 0 ? NULL_VALUE : lit_number_to_value(number);
}

static LitValue objfn_array_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index;
    LitValues* values;
    index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    values = &AS_ARRAY(instance)->values;
    if(values->count <= index)
    {
        return NULL_VALUE;
    }
    return values->values[index];
}

static LitValue objfn_array_join(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    size_t index;
    size_t length;
    LitValues* values;
    LitString* string;
    (void)argc;
    (void)argv;
    length = 0;
    values = &AS_ARRAY(instance)->values;
    LitString* strings[values->count];
    for(i = 0; i < values->count; i++)
    {
        string = lit_to_string(vm->state, values->values[i]);
        strings[i] = string;
        length += string->length;
    }
    index = 0;
    char chars[length + 1];
    chars[length] = '\0';
    for(i = 0; i < values->count; i++)
    {
        LitString* string = strings[i];
        memcpy(chars + index, string->chars, string->length);
        index += string->length;
    }
    return OBJECT_VALUE(lit_copy_string(vm->state, chars, length));
}


static LitValue objfn_array_sort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitValues* values;
    values = &AS_ARRAY(instance)->values;
    if(argc == 1 && IS_CALLABLE_FUNCTION(argv[0]))
    {
        util_custom_quick_sort(vm, values->values, values->count, argv[0]);
    }
    else
    {
        util_basic_quick_sort(vm->state, values->values, values->count);
    }
    return instance;
}


static LitValue objfn_array_clone(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    size_t i;
    LitState* state;
    LitValues* values;
    LitArray* array;
    LitValues* new_values;
    state = vm->state;
    values = &AS_ARRAY(instance)->values;
    array = lit_create_array(state);
    new_values = &array->values;
    lit_values_ensure_size(state, new_values, values->count);
    // lit_values_ensure_size sets the count to max of previous count (0 in this case) and new count, so we have to reset it
    new_values->count = 0;
    for(i = 0; i < values->count; i++)
    {
        lit_values_write(state, new_values, values->values[i]);
    }
    return OBJECT_VALUE(array);
}

static LitValue objfn_array_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    bool has_more;
    size_t i;
    size_t buffer_index;
    size_t value_amount;
    size_t olength;
    LitArray* self;
    LitValues* values;
    LitValue val;
    LitState* state;
    LitString* part;
    LitString* stringified;
    static const char* recstring = "(recursion)";
    self = AS_ARRAY(instance);
    values = &self->values;
    state = vm->state;
    if(values->count == 0)
    {
        return OBJECT_CONST_STRING(state, "[]");
    }
    has_more = values->count > LIT_CONTAINER_OUTPUT_MAX;
    value_amount = has_more ? LIT_CONTAINER_OUTPUT_MAX : values->count;
    LitString* values_converted[value_amount];

    // "[ ]"
    olength = 3;
    if(has_more)
    {
        olength += 3;
    }
    for(i = 0; i < value_amount; i++)
    {
        val = values->values[(has_more && i == value_amount - 1) ? values->count - 1 : i];
        if(IS_ARRAY(val) && (AS_ARRAY(val) == self))
        {
            stringified = lit_copy_string(state, recstring, strlen(recstring));
        }
        else
        {
            stringified = lit_to_string(state, val);
        }
        values_converted[i] = stringified;
        olength += stringified->length + (i == value_amount - 1 ? 1 : 2);
    }
    char buffer[olength + 1];
    memcpy(buffer, "[ ", 2);
    buffer_index = 2;
    for(i = 0; i < value_amount; i++)
    {
        part = values_converted[i];
        memcpy(&buffer[buffer_index], part->chars, part->length);
        buffer_index += part->length;
        if(has_more && i == value_amount - 2)
        {
            memcpy(&buffer[buffer_index], " ... ", 5);
            buffer_index += 5;
        }
        else
        {
            memcpy(&buffer[buffer_index], (i == value_amount - 1) ? " ]" : ", ", 2);
            buffer_index += 2;
        }
    }
    buffer[olength] = '\0';
    return OBJECT_VALUE(lit_copy_string(vm->state, buffer, olength));
}

static LitValue objfn_array_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(AS_ARRAY(instance)->values.count);
}


void lit_open_array_library(LitState* state)
{
    LIT_BEGIN_CLASS("Array");
    {
        LIT_INHERIT_CLASS(state->objectvalue_class);
        LIT_BIND_CONSTRUCTOR(objfn_array_constructor);
        LIT_BIND_METHOD("[]", objfn_array_subscript);
        LIT_BIND_METHOD("add", objfn_array_add);
        LIT_BIND_METHOD("push", objfn_array_add);
        LIT_BIND_METHOD("insert", objfn_array_insert);
        LIT_BIND_METHOD("slice", objfn_array_slice);
        LIT_BIND_METHOD("addAll", objfn_array_addall);
        LIT_BIND_METHOD("remove", objfn_array_remove);
        LIT_BIND_METHOD("removeAt", objfn_array_removeat);
        LIT_BIND_METHOD("indexOf", objfn_array_indexof);
        LIT_BIND_METHOD("contains", objfn_array_contains);
        LIT_BIND_METHOD("clear", objfn_array_clear);
        LIT_BIND_METHOD("iterator", objfn_array_iterator);
        LIT_BIND_METHOD("iteratorValue", objfn_array_iteratorvalue);
        LIT_BIND_METHOD("join", objfn_array_join);
        LIT_BIND_METHOD("sort", objfn_array_sort);
        LIT_BIND_METHOD("clone", objfn_array_clone);
        LIT_BIND_METHOD("toString", objfn_array_tostring);
        LIT_BIND_GETTER("length", objfn_array_length);
        state->arrayvalue_class = klass;
    }
    LIT_END_CLASS();
}


