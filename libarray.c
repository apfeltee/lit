
#include "lit.h"
#include "sds.h"

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
    LitValueList* values;
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
    from = lit_check_number(vm, argv, argc, 0);
    to = lit_check_number(vm, argv, argc, 1);

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

        LitValueList* values = &AS_ARRAY(instance)->values;
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

    LitValueList* values = &AS_ARRAY(instance)->values;
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


/*

LitInterpretResult lit_call_function(LitState* state, LitFunction* callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult lit_call_method(LitState* state, LitValue instance, LitValue callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult lit_call(LitState* state, LitValue callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult lit_find_and_call_method(LitState* state, LitValue callee, LitString* method_name, LitValue* arguments, uint8_t argument_count, bool ignfiber);

*/
bool lit_compare_values(LitState* state, const LitValue a, const LitValue b)
{
    LitInterpretResult inret;
    LitValue args[3];
    if(IS_INSTANCE(a))
    {
        args[0] = b;
        inret = lit_instance_call_method(state, a, CONST_STRING(state, "=="), args, 1);
        if(inret.type == LITRESULT_OK)
        {
            if(BOOL_VALUE(inret.result) == TRUE_VALUE)
            {
                return true;
            }
            return false;
        }
    }
    return (a == b);
}

static LitValue objfn_array_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitArray* self;
    LitArray* other;
    fprintf(stderr, "call to objfn_array_compare\n");
    self = AS_ARRAY(instance);
    if(IS_ARRAY(argv[0]))
    {
        other = AS_ARRAY(argv[0]);
        if(self->values.count == other->values.count)
        {
            for(i=0; i<self->values.count; i++)
            {
                if(!lit_compare_values(vm->state, self->values.values[i], other->values.values[i]))
                {
                    return FALSE_VALUE;
                }
            }
            return TRUE_VALUE;
        }
        return FALSE_VALUE;
    }
    lit_runtime_error_exiting(vm, "can only compare array to another array or null");
    return FALSE_VALUE;
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

        LitValueList* values = &AS_ARRAY(instance)->values;
    int index = lit_check_number(vm, argv, argc, 0);

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
    int index = lit_check_number(vm, argv, argc, 0);

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
    LitValueList* values;
    index = lit_check_number(vm, argv, argc, 0);
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
    size_t jlen;
    size_t index;
    size_t length;
    char* chars;
    
    LitValueList* values;
    LitString* string;
    LitString* joinee;
    LitString** strings;
    
    (void)argc;
    (void)argv;
    joinee = NULL;
    length = 0;
    if(argc > 0)
    {
        joinee = AS_STRING(argv[0]);
    }
    values = &AS_ARRAY(instance)->values;
    //LitString* strings[values->count];
    strings = LIT_ALLOCATE(vm->state, LitString*, values->count+1);
    for(i = 0; i < values->count; i++)
    {
        string = lit_to_string(vm->state, values->values[i]);
        strings[i] = string;
        length += lit_string_length(string);
        if(joinee != NULL)
        {
            length += lit_string_length(joinee);
        }
    }
    jlen = 0;
    index = 0;
    chars = sdsempty();
    chars = sdsMakeRoomFor(chars, length + 1);
    if(joinee != NULL)
    {
        jlen = lit_string_length(joinee);
    }
    for(i = 0; i < values->count; i++)
    {
        string = strings[i];
        memcpy(chars + index, string->chars, lit_string_length(string));
        chars = sdscatlen(chars, string->chars, lit_string_length(string));
        index += lit_string_length(string);
        if(joinee != NULL)
        {
            
            //if((i+1) < values->count)
            {
                chars = sdscatlen(chars, joinee->chars, jlen);
            }
            index += jlen;
        }
    }
    LIT_FREE(vm->state, LitString*, strings);
    return OBJECT_VALUE(lit_string_take(vm->state, chars, length, true));
}


static LitValue objfn_array_sort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitValueList* values;
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
    LitValueList* values;
    LitArray* array;
    LitValueList* new_values;
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
    size_t value_amount;
    size_t olength;
    char* buffer;
    LitArray* self;
    LitValueList* values;
    LitValue val;
    LitState* state;
    LitString* rt;
    LitString* part;
    LitString* stringified;
    LitString** values_converted;
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
    values_converted = LIT_ALLOCATE(vm->state, LitString*, value_amount+1);
    // "[ ]"
    olength = 3;
    if(has_more)
    {
        olength += 3;
    }
    buffer = sdsempty();
    buffer = sdsMakeRoomFor(buffer, olength+1);
    buffer = sdscat(buffer, "[");
    for(i = 0; i < value_amount; i++)
    {
        val = values->values[(has_more && i == value_amount - 1) ? values->count - 1 : i];
        if(IS_ARRAY(val) && (AS_ARRAY(val) == self))
        {
            stringified = lit_string_copy(state, recstring, strlen(recstring));
        }
        else
        {
            stringified = lit_to_string(state, val);
        }
        part = stringified;
        buffer = sdscatlen(buffer, part->chars, lit_string_length(part));
        if(has_more && i == value_amount - 2)
        {
            buffer = sdscat(buffer, " ... ");
        }
        else
        {
            if(i == (value_amount - 1))
            {
                buffer = sdscat(buffer, " ]");
            }
            else
            {
                buffer = sdscat(buffer, ", ");
            }
        }
    }
    LIT_FREE(vm->state, LitString*, values_converted);
    // should be lit_string_take, but it doesn't get picked up by the GC for some reason
    //rt = lit_string_take(vm->state, buffer, olength);
    rt = lit_string_take(vm->state, buffer, olength, true);
    return OBJECT_VALUE(rt);
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
        LIT_BIND_METHOD("==", objfn_array_compare);
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


