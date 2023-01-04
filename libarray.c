
#include "lit.h"
#include "sds.h"

void lit_datalist_init(LitDataList* dl, size_t typsz)
{
    dl->values = NULL;
    dl->capacity = 0;
    dl->count = 0;
    dl->rawelemsz = typsz;
    dl->elemsz = dl->rawelemsz + sizeof(intptr_t);
}

void lit_datalist_destroy(LitState* state, LitDataList* dl)
{
    LIT_FREE_ARRAY(state, dl->elemsz, dl->values, dl->capacity);
    lit_datalist_init(dl, dl->rawelemsz);
}

size_t lit_datalist_count(LitDataList* dl)
{
    return dl->count;
}

size_t lit_datalist_size(LitDataList* dl)
{
    return dl->count;
}

size_t lit_datalist_capacity(LitDataList* dl)
{
    return dl->capacity;
}

void lit_datalist_clear(LitDataList* dl)
{
    dl->count = 0;
}

void lit_datalist_setcount(LitDataList* dl, size_t nc)
{
    dl->count = nc;
}

void lit_datalist_deccount(LitDataList* dl)
{
    dl->count--;
}

intptr_t lit_datalist_get(LitDataList* dl, size_t idx)
{
    return dl->values[idx];
}

intptr_t lit_datalist_set(LitDataList* dl, size_t idx, intptr_t val)
{
    dl->values[idx] = val;
    return val;
}

void lit_datalist_push(LitState* state, LitDataList* dl, intptr_t value)
{
    size_t old_capacity;
    if(dl->capacity < (dl->count + 1))
    {
        old_capacity = dl->capacity;
        dl->capacity = LIT_GROW_CAPACITY(old_capacity);
        dl->values = LIT_GROW_ARRAY(state, dl->values, dl->elemsz, old_capacity, dl->capacity);
    }
    dl->values[dl->count] = value;
    dl->count++;
}

void lit_datalist_ensuresize(LitState* state, LitDataList* dl, size_t size)
{
    size_t i;
    size_t old_capacity;
    if(dl->capacity < size)
    {
        old_capacity = dl->capacity;
        dl->capacity = size;
        dl->values = LIT_GROW_ARRAY(state, dl->values, dl->elemsz, old_capacity, size);
        for(i = old_capacity; i < size; i++)
        {
            dl->values[i] = NULL_VALUE;
        }
    }
    if(dl->count < size)
    {
        dl->count = size;
    }
}

/* -------------------------*/

void lit_vallist_init(LitValueList* vl)
{
    lit_datalist_init(&vl->list, sizeof(LitValue));
}

void lit_vallist_destroy(LitState* state, LitValueList* vl)
{
    lit_datalist_destroy(state, &vl->list);
}

size_t lit_vallist_size(LitValueList* vl)
{
    return lit_datalist_count(&vl->list);
}

size_t lit_vallist_count(LitValueList* vl)
{
    return lit_vallist_size(vl);
}

size_t lit_vallist_capacity(LitValueList* vl)
{
    return lit_datalist_capacity(&vl->list);
}

void lit_vallist_setcount(LitValueList* vl, size_t nc)
{
    lit_datalist_setcount(&vl->list, nc);
}

void lit_vallist_clear(LitValueList* vl)
{
    lit_datalist_setcount(&vl->list, 0);
}

void lit_vallist_deccount(LitValueList* vl)
{
    lit_datalist_deccount(&vl->list);
}

void lit_vallist_ensuresize(LitState* state, LitValueList* values, size_t size)
{
    lit_datalist_ensuresize(state, &values->list, size);
}


LitValue lit_vallist_set(LitValueList* vl, size_t idx, LitValue val)
{
    lit_datalist_set(&vl->list, idx, val);
    return val;
}

LitValue lit_vallist_get(LitValueList* vl, size_t idx)
{
    return (LitValue)lit_datalist_get(&vl->list, idx);
}

void lit_vallist_push(LitState* state, LitValueList* vl, LitValue value)
{
    lit_datalist_push(state, &vl->list, (intptr_t)value);
}

/* ---- Array object instance functions */

LitArray* lit_create_array(LitState* state)
{
    LitArray* array;
    array = (LitArray*)lit_gcmem_allocobject(state, sizeof(LitArray), LITTYPE_ARRAY, false);
    lit_vallist_init(&array->list);
    return array;
}


size_t lit_array_count(LitArray* arr)
{
    return lit_vallist_count(&arr->list);
}

LitValue lit_array_pop(LitState* state, LitArray* arr)
{
    LitValue val;
    if(lit_vallist_count(&arr->list) > 0)
    {
        val = lit_vallist_get(&arr->list, lit_vallist_count(&arr->list) - 1);
        lit_vallist_deccount(&arr->list);
        return val;
    }
    return NULL_VALUE;
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

LitValue lit_array_get(LitState* state, LitArray* array, size_t idx)
{
    if(idx <= lit_vallist_count(&array->list))
    {
        return lit_vallist_get(&array->list, idx);
    }
    return NULL_VALUE;
}

LitArray* lit_array_splice(LitState* state, LitArray* oa, int from, int to)
{
    size_t i;
    size_t length;
    LitArray* newarr;
    length = lit_array_count(oa);
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
        lit_vm_raiseexitingerror(state->vm, "Array.splice argument 'from' is larger than argument 'to'");
        return NULL;
    }
    from = fmax(from, 0);
    to = fmin(to, (int)length - 1);
    length = fmin(length, to - from + 1);
    newarr = lit_create_array(state);
    for(i = 0; i < length; i++)
    {
        lit_array_push(state, newarr, lit_array_get(state, oa, from + i));
    }
    return newarr;
}

static LitValue objfn_array_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(lit_create_array(vm->state));
}

static LitValue objfn_array_splice(LitVM* vm, LitArray* array, int from, int to)
{
    LitArray* newarr;
    newarr = lit_array_splice(vm->state, array, from, to);
    return lit_value_objectvalue(newarr);
}

static LitValue objfn_array_slice(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int from;
    int to;
    from = lit_value_checknumber(vm, argv, argc, 0);
    to = lit_value_checknumber(vm, argv, argc, 1);
    return objfn_array_splice(vm, lit_value_asarray(instance), from, to);
}

static LitValue objfn_array_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    LitRange* range;
    LitValueList* values;
    if(argc == 2)
    {
        if(!lit_value_isnumber(argv[0]))
        {
            lit_vm_raiseexitingerror(vm, "array index must be a number");
        }
        values = &lit_value_asarray(instance)->list;
        index = lit_value_asnumber(argv[0]);
        if(index < 0)
        {
            index = fmax(0, lit_vallist_count(values) + index);
        }
        lit_vallist_ensuresize(vm->state, values, index + 1);
        return lit_vallist_set(values, index, argv[1]);
    }
    if(!lit_value_isnumber(argv[0]))
    {
        if(lit_value_isrange(argv[0]))
        {
            range = lit_value_asrange(argv[0]);
            return objfn_array_splice(vm, lit_value_asarray(instance), (int)range->from, (int)range->to);
        }
        lit_vm_raiseexitingerror(vm, "array index must be a number");
        return NULL_VALUE;
    }
    values = &lit_value_asarray(instance)->list;
    index = lit_value_asnumber(argv[0]);
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


static LitValue objfn_array_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitArray* self;
    LitArray* other;
    (void)argc;
    fprintf(stderr, "lit_vm_callcallable to objfn_array_compare\n");
    self = lit_value_asarray(instance);
    if(lit_value_isarray(argv[0]))
    {
        other = lit_value_asarray(argv[0]);
        if(lit_vallist_count(&self->list) == lit_vallist_count(&other->list))
        {
            for(i=0; i<lit_vallist_count(&self->list); i++)
            {
                if(!lit_value_compare(vm->state, lit_vallist_get(&self->list, i), lit_vallist_get(&other->list, i)))
                {
                    return FALSE_VALUE;
                }
            }
            return TRUE_VALUE;
        }
        return FALSE_VALUE;
    }
    lit_vm_raiseexitingerror(vm, "can only compare array to another array or null");
    return FALSE_VALUE;
}


static LitValue objfn_array_add(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    for(i=0; i<argc; i++)
    {
        lit_array_push(vm->state, lit_value_asarray(instance), argv[i]);
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
    values = &lit_value_asarray(instance)->list;
    index = lit_value_checknumber(vm, argv, argc, 0);

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
    if(!lit_value_isarray(argv[0]))
    {
        lit_vm_raiseexitingerror(vm, "expected array as the argument");
    }
    array = lit_value_asarray(instance);
    toadd = lit_value_asarray(argv[0]);
    for(i = 0; i < lit_vallist_count(&toadd->list); i++)
    {
        lit_array_push(vm->state, array, lit_vallist_get(&toadd->list, i));
    }
    return NULL_VALUE;
}

static LitValue objfn_array_indexof(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

        int index = lit_array_indexof(lit_value_asarray(instance), argv[0]);
    return index == -1 ? NULL_VALUE : lit_value_numbertovalue(vm->state, index);
}


static LitValue objfn_array_remove(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    LitArray* array;
    LIT_ENSURE_ARGS(1);
    array = lit_value_asarray(instance);
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
    index = lit_value_checknumber(vm, argv, argc, 0);
    if(index < 0)
    {
        return NULL_VALUE;
    }
    return lit_array_removeat(lit_value_asarray(instance), (size_t)index);
}

static LitValue objfn_array_contains(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    return lit_bool_to_value(vm->state, lit_array_indexof(lit_value_asarray(instance), argv[0]) != -1);
}

static LitValue objfn_array_clear(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    lit_vallist_clear(&lit_value_asarray(instance)->list);
    return NULL_VALUE;
}

static LitValue objfn_array_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int number;
    LitArray* array;
    (void)vm;
    LIT_ENSURE_ARGS(1);
    array = lit_value_asarray(instance);
    number = 0;
    if(lit_value_isnumber(argv[0]))
    {
        number = lit_value_asnumber(argv[0]);
        if(number >= (int)lit_vallist_count(&array->list) - 1)
        {
            return NULL_VALUE;
        }
        number++;
    }
    return lit_vallist_count(&array->list) == 0 ? NULL_VALUE : lit_value_numbertovalue(vm->state, number);
}

static LitValue objfn_array_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index;
    LitValueList* values;
    index = lit_value_checknumber(vm, argv, argc, 0);
    values = &lit_value_asarray(instance)->list;
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
        joinee = lit_value_asstring(argv[0]);
    }
    values = &lit_value_asarray(instance)->list;
    //LitString* strings[values->count];
    strings = LIT_ALLOCATE(vm->state, sizeof(LitString*), lit_vallist_count(values)+1);
    for(i = 0; i < lit_vallist_count(values); i++)
    {
        string = lit_value_tostring(vm->state, lit_vallist_get(values, i));
        strings[i] = string;
        length += lit_string_getlength(string);
        if(joinee != NULL)
        {
            length += lit_string_getlength(joinee);
        }
    }
    jlen = 0;
    index = 0;
    chars = sdsempty();
    chars = sdsMakeRoomFor(chars, length + 1);
    if(joinee != NULL)
    {
        jlen = lit_string_getlength(joinee);
    }
    for(i = 0; i < lit_vallist_count(values); i++)
    {
        string = strings[i];
        memcpy(chars + index, string->chars, lit_string_getlength(string));
        chars = sdscatlen(chars, string->chars, lit_string_getlength(string));
        index += lit_string_getlength(string);
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
    return lit_value_objectvalue(lit_string_take(vm->state, chars, length, true));
}

static LitValue objfn_array_sort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitValueList* values;
    values = &lit_value_asarray(instance)->list;
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
    values = &lit_value_asarray(instance)->list;
    array = lit_create_array(state);
    new_values = &array->list;
    lit_vallist_ensuresize(state, new_values, lit_vallist_count(values));
    // lit_vallist_ensuresize sets the count to max of previous count (0 in this case) and new count, so we have to reset it
    lit_vallist_setcount(new_values, 0);
    for(i = 0; i < lit_vallist_count(values); i++)
    {
        lit_vallist_push(state, new_values, lit_vallist_get(values, i));
    }
    return lit_value_objectvalue(array);
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
    self = lit_value_asarray(instance);
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
        if(lit_value_isarray(val) && (lit_value_asarray(val) == self))
        {
            stringified = lit_string_copy(state, recstring, strlen(recstring));
        }
        else
        {
            stringified = lit_value_tostring(state, val);
        }
        part = stringified;
        buffer = sdscatlen(buffer, part->chars, lit_string_getlength(part));
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
    return lit_value_objectvalue(rt);
}

static LitValue objfn_array_pop(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    size_t i;
    LitArray* self;
    LitValue val;
    LitState* state;
    self = lit_value_asarray(instance);

    return lit_array_pop(vm->state, self);
}


static LitValue objfn_array_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_numbertovalue(vm->state, lit_vallist_count(&lit_value_asarray(instance)->list));
}

void lit_open_array_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "Array");
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
        lit_class_bindconstructor(state, klass, objfn_array_constructor);
        lit_class_bindmethod(state, klass, "[]", objfn_array_subscript);
        lit_class_bindmethod(state, klass, "==", objfn_array_compare);
        lit_class_bindmethod(state, klass, "add", objfn_array_add);
        lit_class_bindmethod(state, klass, "push", objfn_array_add);
        lit_class_bindmethod(state, klass, "insert", objfn_array_insert);
        lit_class_bindmethod(state, klass, "slice", objfn_array_slice);
        lit_class_bindmethod(state, klass, "addAll", objfn_array_addall);
        lit_class_bindmethod(state, klass, "remove", objfn_array_remove);
        lit_class_bindmethod(state, klass, "removeAt", objfn_array_removeat);
        lit_class_bindmethod(state, klass, "indexOf", objfn_array_indexof);
        lit_class_bindmethod(state, klass, "contains", objfn_array_contains);
        lit_class_bindmethod(state, klass, "clear", objfn_array_clear);
        lit_class_bindmethod(state, klass, "iterator", objfn_array_iterator);
        lit_class_bindmethod(state, klass, "iteratorValue", objfn_array_iteratorvalue);
        lit_class_bindmethod(state, klass, "join", objfn_array_join);
        lit_class_bindmethod(state, klass, "sort", objfn_array_sort);
        lit_class_bindmethod(state, klass, "clone", objfn_array_clone);
        lit_class_bindmethod(state, klass, "toString", objfn_array_tostring);
        lit_class_bindmethod(state, klass, "pop", objfn_array_pop);
        lit_class_bindgetset(state, klass, "length", objfn_array_length, NULL, false);
        state->arrayvalue_class = klass;
    }
    lit_state_setglobal(state, klass->name, lit_value_objectvalue(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    }
}


