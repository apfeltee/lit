
#include "lit.h"
#include "sds.h"


void lit_datalist_init(LitDataList* array, size_t typsz)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
    array->elemsz = typsz;
}

void lit_datalist_destroy(LitState* state, LitDataList* array)
{
    LIT_FREE_ARRAY(state, array->elemsz, array->values, array->capacity);
    lit_datalist_init(array, array->elemsz);
}

size_t lit_datalist_count(LitDataList* arr)
{
    return arr->count;
}

size_t lit_datalist_size(LitDataList* arr)
{
    return arr->count;
}

size_t lit_datalist_capacity(LitDataList* arr)
{
    return arr->capacity;
}

void lit_datalist_clear(LitDataList* arr)
{
    arr->count = 0;
}

void lit_datalist_setcount(LitDataList* arr, size_t nc)
{
    arr->count = nc;
}

void lit_datalist_deccount(LitDataList* arr)
{
    arr->count--;
}

uintptr_t lit_datalist_get(LitDataList* arr, size_t idx)
{
    return arr->values[idx];
}

uintptr_t lit_datalist_set(LitDataList* arr, size_t idx, uintptr_t val)
{
    arr->values[idx] = val;
    return val;
}

void lit_datalist_push(LitState* state, LitDataList* array, uintptr_t value)
{
    size_t old_capacity;
    if(array->capacity < array->count + 1)
    {
        old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, array->elemsz, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void lit_datalist_ensuresize(LitState* state, LitDataList* array, size_t size)
{
    size_t i;
    size_t old_capacity;
    if(array->capacity < size)
    {
        old_capacity = array->capacity;
        array->capacity = size;
        array->values = LIT_GROW_ARRAY(state, array->values, array->elemsz, old_capacity, size);
        for(i = old_capacity; i < size; i++)
        {
            array->values[i] = NULL_VALUE;
        }
    }
    if(array->count < size)
    {
        array->count = size;
    }
}

/* -------------------------*/

void lit_vallist_init(LitValueList* array)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        lit_datalist_init(&array->list, sizeof(LitValue));
    #else
        array->values = NULL;
        array->capacity = 0;
        array->count = 0;
    #endif
}

void lit_vallist_destroy(LitState* state, LitValueList* array)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        lit_datalist_destroy(state, &array->list);
    #else
        LIT_FREE_ARRAY(state, sizeof(LitValue), array->values, array->capacity);
        lit_vallist_init(array);
    #endif
}

size_t lit_vallist_size(LitValueList* arr)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        return lit_datalist_count(&arr->list);
    #else
        return arr->count;
    #endif
}

size_t lit_vallist_count(LitValueList* arr)
{
    return lit_vallist_size(arr);
}

size_t lit_vallist_capacity(LitValueList* arr)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        return lit_datalist_capacity(&arr->list);
    #else
        return arr->capacity;
    #endif
}

void lit_vallist_setcount(LitValueList* arr, size_t nc)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        lit_datalist_setcount(&arr->list, nc);
    #else
        arr->count = nc;
    #endif
}


void lit_vallist_clear(LitValueList* arr)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        lit_datalist_setcount(&arr->list, 0);
    #else
        arr->count = 0;
    #endif
}


void lit_vallist_deccount(LitValueList* arr)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        lit_datalist_deccount(&arr->list);
    #else
        arr->count--;
    #endif
}

LitValue lit_vallist_set(LitValueList* arr, size_t idx, LitValue val)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        lit_datalist_set(&arr->list, idx, val);
    #else
        arr->values[idx] = val;
    #endif
    return val;
}


LitValue lit_vallist_get(LitValueList* arr, size_t idx)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        return (LitValue)lit_datalist_get(&arr->list, idx);
    #else
        return arr->values[idx];
    #endif
}

void lit_vallist_push(LitState* state, LitValueList* array, LitValue value)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        lit_datalist_push(state, &array->list, (uintptr_t)value);
    #else
        size_t old_capacity;
        if(array->capacity < array->count + 1)
        {
            old_capacity = array->capacity;
            array->capacity = LIT_GROW_CAPACITY(old_capacity);
            array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitValue), old_capacity, array->capacity);
        }
        array->values[array->count] = value;
        array->count++;
    #endif
}


void lit_vallist_ensuresize(LitState* state, LitValueList* values, size_t size)
{
    #if defined(USE_DATALIST) && (USE_DATALIST == 1)
        lit_datalist_ensuresize(state, &values->list, size);
    #else
        size_t i;
        size_t old_capacity;
        if(values->capacity < size)
        {
            old_capacity = values->capacity;
            values->capacity = size;
            values->values = LIT_GROW_ARRAY(state, values->values, sizeof(LitValue), old_capacity, size);
            for(i = old_capacity; i < size; i++)
            {
                values->values[i] = NULL_VALUE;
            }
        }
        if(values->count < size)
        {
            values->count = size;
        }
    #endif

}

int lit_array_indexof(LitArray* array, LitValue value)
{
    size_t i;
    for(i = 0; i < lit_vallist_count(&array->list); i++)
    {
        if(lit_vallist_get(&array->list, i) == value)
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
    values = &array->list;
    count = lit_vallist_count(values);
    if(index >= count)
    {
        return NULL_VALUE;
    }
    value = lit_vallist_get(values, index);
    if(index == count - 1)
    {
        lit_vallist_set(values, index, NULL_VALUE);
    }
    else
    {
        for(i = index; i < lit_vallist_count(values) - 1; i++)
        {
            lit_vallist_set(values, i, lit_vallist_get(values, i + 1));
        }
        lit_vallist_set(values, count - 1, NULL_VALUE);
    }
    lit_vallist_deccount(values);
    return value;
}

void lit_array_push(LitState* state, LitArray* array, LitValue val)
{
    lit_vallist_push(state, &array->list, val);
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
    length = lit_vallist_count(&array->list);
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
        lit_vallist_push(vm->state, &new_array->list, lit_vallist_get(&array->list, from + i));
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
    int index;
    LitRange* range;
    LitValueList* values;
    if(argc == 2)
    {
        if(!IS_NUMBER(argv[0]))
        {
            lit_runtime_error_exiting(vm, "array index must be a number");
        }
        values = &AS_ARRAY(instance)->list;
        index = lit_value_to_number(argv[0]);
        if(index < 0)
        {
            index = fmax(0, lit_vallist_count(values) + index);
        }
        lit_vallist_ensuresize(vm->state, values, index + 1);
        return lit_vallist_set(values, index, argv[1]);
    }
    if(!IS_NUMBER(argv[0]))
    {
        if(IS_RANGE(argv[0]))
        {
            range = AS_RANGE(argv[0]);
            return objfn_array_splice(vm, AS_ARRAY(instance), (int)range->from, (int)range->to);
        }
        lit_runtime_error_exiting(vm, "array index must be a number");
        return NULL_VALUE;
    }
    values = &AS_ARRAY(instance)->list;
    index = lit_value_to_number(argv[0]);
    if(index < 0)
    {
        index = fmax(0, lit_vallist_count(values) + index);
    }
    if(lit_vallist_capacity(values) <= (size_t)index)
    {
        return NULL_VALUE;
    }
    return lit_vallist_get(values, index);
}

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
    (void)argc;
    fprintf(stderr, "call to objfn_array_compare\n");
    self = AS_ARRAY(instance);
    if(IS_ARRAY(argv[0]))
    {
        other = AS_ARRAY(argv[0]);
        if(lit_vallist_count(&self->list) == lit_vallist_count(&other->list))
        {
            for(i=0; i<lit_vallist_count(&self->list); i++)
            {
                if(!lit_compare_values(vm->state, lit_vallist_get(&self->list, i), lit_vallist_get(&other->list, i)))
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
    int i;
    int index;
    LitValue value;
    LitValueList* values;
    LIT_ENSURE_ARGS(2);
    values = &AS_ARRAY(instance)->list;
    index = lit_check_number(vm, argv, argc, 0);

    if(index < 0)
    {
        index = fmax(0, lit_vallist_count(values) + index);
    }
    value = argv[1];
    if((int)lit_vallist_count(values) <= index)
    {
        lit_vallist_ensuresize(vm->state, values, index + 1);
    }
    else
    {
        lit_vallist_ensuresize(vm->state, values, lit_vallist_count(values)  + 1);
        for(i = lit_vallist_count(values) - 1; i > index; i--)
        {
            lit_vallist_set(values, i, lit_vallist_get(values, i - 1));
        }
    }
    lit_vallist_set(values, index, value);
    return NULL_VALUE;
}

static LitValue objfn_array_addall(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitArray* array;
    LitArray* toadd;
    LIT_ENSURE_ARGS(1);
    if(!IS_ARRAY(argv[0]))
    {
        lit_runtime_error_exiting(vm, "expected array as the argument");
    }
    array = AS_ARRAY(instance);
    toadd = AS_ARRAY(argv[0]);
    for(i = 0; i < lit_vallist_count(&toadd->list); i++)
    {
        lit_array_push(vm->state, array, lit_vallist_get(&toadd->list, i));
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
    int index;
    LitArray* array;
    LIT_ENSURE_ARGS(1);
    array = AS_ARRAY(instance);
    index = lit_array_indexof(array, argv[0]);
    if(index != -1)
    {
        return lit_array_removeat(array, (size_t)index);
    }
    return NULL_VALUE;
}

static LitValue objfn_array_removeat(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    index = lit_check_number(vm, argv, argc, 0);
    if(index < 0)
    {
        return NULL_VALUE;
    }
    return lit_array_removeat(AS_ARRAY(instance), (size_t)index);
}

static LitValue objfn_array_contains(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    return BOOL_VALUE(lit_array_indexof(AS_ARRAY(instance), argv[0]) != -1);
}

static LitValue objfn_array_clear(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    lit_vallist_clear(&AS_ARRAY(instance)->list);
    return NULL_VALUE;
}

static LitValue objfn_array_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int number;
    LitArray* array;
    (void)vm;
    LIT_ENSURE_ARGS(1);
    array = AS_ARRAY(instance);
    number = 0;
    if(IS_NUMBER(argv[0]))
    {
        number = lit_value_to_number(argv[0]);
        if(number >= (int)lit_vallist_count(&array->list) - 1)
        {
            return NULL_VALUE;
        }
        number++;
    }
    return lit_vallist_count(&array->list) == 0 ? NULL_VALUE : lit_number_to_value(number);
}

static LitValue objfn_array_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index;
    LitValueList* values;
    index = lit_check_number(vm, argv, argc, 0);
    values = &AS_ARRAY(instance)->list;
    if(lit_vallist_count(values) <= index)
    {
        return NULL_VALUE;
    }
    return lit_vallist_get(values, index);
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
        joinee = lit_as_string(argv[0]);
    }
    values = &AS_ARRAY(instance)->list;
    //LitString* strings[values->count];
    strings = LIT_ALLOCATE(vm->state, sizeof(LitString*), lit_vallist_count(values)+1);
    for(i = 0; i < lit_vallist_count(values); i++)
    {
        string = lit_to_string(vm->state, lit_vallist_get(values, i));
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
    for(i = 0; i < lit_vallist_count(values); i++)
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
    LIT_FREE(vm->state, sizeof(LitString*), strings);
    return OBJECT_VALUE(lit_string_take(vm->state, chars, length, true));
}

static LitValue objfn_array_sort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitValueList* values;
    values = &AS_ARRAY(instance)->list;
    /*
    if(argc == 1 && lit_is_callable_function(argv[0]))
    {
        util_custom_quick_sort(vm, values->values, values->count, argv[0]);
    }
    else
    {
        util_basic_quick_sort(vm->state, values->values, values->count);
    }
    */
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
    values = &AS_ARRAY(instance)->list;
    array = lit_create_array(state);
    new_values = &array->list;
    lit_vallist_ensuresize(state, new_values, lit_vallist_count(values));
    // lit_vallist_ensuresize sets the count to max of previous count (0 in this case) and new count, so we have to reset it
    lit_vallist_setcount(new_values, 0);
    for(i = 0; i < lit_vallist_count(values); i++)
    {
        lit_vallist_push(state, new_values, lit_vallist_get(values, i));
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
    values = &self->list;
    state = vm->state;
    if(lit_vallist_count(values) == 0)
    {
        return OBJECT_CONST_STRING(state, "[]");
    }
    has_more = lit_vallist_count(values) > LIT_CONTAINER_OUTPUT_MAX;
    value_amount = has_more ? LIT_CONTAINER_OUTPUT_MAX : lit_vallist_count(values);
    values_converted = LIT_ALLOCATE(vm->state, sizeof(LitString*), value_amount+1);
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
        val = lit_vallist_get(values, (has_more && i == value_amount - 1) ? lit_vallist_count(values) - 1 : i);
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
    LIT_FREE(vm->state, sizeof(LitString*), values_converted);
    // should be lit_string_take, but it doesn't get picked up by the GC for some reason
    //rt = lit_string_take(vm->state, buffer, olength);
    rt = lit_string_take(vm->state, buffer, olength, true);
    return OBJECT_VALUE(rt);
}

static LitValue objfn_array_pop(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    size_t i;
    LitArray* self;
    LitValue val;
    LitState* state;
    self = AS_ARRAY(instance);
    if(lit_vallist_count(&self->list) > 0)
    {
        val = lit_vallist_get(&self->list, lit_vallist_count(&self->list) - 1);
        lit_vallist_deccount(&self->list);
        return val;
        
    }
    return NULL_VALUE;
}


static LitValue objfn_array_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_vallist_count(&AS_ARRAY(instance)->list));
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
        LIT_BIND_METHOD("pop", objfn_array_pop);
        LIT_BIND_GETTER("length", objfn_array_length);
        state->arrayvalue_class = klass;
    }
    LIT_END_CLASS();
}

