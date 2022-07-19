
#include "lit.h"

int util_table_iterator(LitTable* table, int number)
{
    if(table->count == 0)
    {
        return -1;
    }
    if(number >= (int)table->capacity)
    {
        return -1;
    }
    number++;
    for(; number < table->capacity; number++)
    {
        if(table->entries[number].key != NULL)
        {
            return number;
        }
    }

    return -1;
}

LitValue util_table_iterator_key(LitTable* table, int index)
{
    if(table->capacity <= index)
    {
        return NULL_VALUE;
    }
    return OBJECT_VALUE(table->entries[index].key);
}

static LitValue objfn_map_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_create_map(vm->state));
}

static LitValue objfn_map_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitValue val;
    LitValue value;
    LitMap* map;
    LitString* index;
    if(!IS_STRING(argv[0]))
    {
        lit_runtime_error_exiting(vm, "map index must be a string");
    }
    map = AS_MAP(instance);
    index = AS_STRING(argv[0]);
    if(argc == 2)
    {
        val = argv[1];
        if(map->index_fn != NULL)
        {
            return map->index_fn(vm, map, index, &val);
        }
        lit_map_set(vm->state, map, index, val);
        return val;
    }
    if(map->index_fn != NULL)
    {
        return map->index_fn(vm, map, index, NULL);
    }
    if(!lit_table_get(&map->values, index, &value))
    {
        return NULL_VALUE;
    }
    return value;
}

static LitValue objfn_map_addall(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    if(!IS_MAP(argv[0]))
    {
        lit_runtime_error_exiting(vm, "expected map as the argument");
    }
    lit_map_add_all(vm->state, AS_MAP(argv[0]), AS_MAP(instance));
    return NULL_VALUE;
}


static LitValue objfn_map_clear(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argv;
    (void)argc;
    AS_MAP(instance)->values.count = 0;
    return NULL_VALUE;
}

static LitValue objfn_map_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    (void)vm;
    int index;
    int value;
    index = argv[0] == NULL_VALUE ? -1 : lit_value_to_number(argv[0]);
    value = util_table_iterator(&AS_MAP(instance)->values, index);
    return value == -1 ? NULL_VALUE : lit_number_to_value(value);
}

static LitValue objfn_map_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index;
    index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    return util_table_iterator_key(&AS_MAP(instance)->values, index);
}

static LitValue objfn_map_clone(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitState* state;
    LitMap* map;
    state = vm->state;
    map = lit_create_map(state);
    lit_table_add_all(state, &AS_MAP(instance)->values, &map->values);
    return OBJECT_VALUE(map);
}

static LitValue objfn_map_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    bool has_wrapper;
    bool has_more;
    size_t i;
    size_t index;
    size_t value_amount;
    size_t olength;
    size_t buffer_index;
    char* buffer;
    LitState* state;
    LitMap* map;
    LitTable* values;
    LitTableEntry* entry;
    LitValue field;
    LitString* strobval;
    LitString* key;
    LitString* value;
    LitString** values_converted;
    LitString** keys;
    state = vm->state;
    map = AS_MAP(instance);
    values = &map->values;
    if(values->count == 0)
    {
        return OBJECT_CONST_STRING(state, "{}");
    }
    has_wrapper = map->index_fn != NULL;
    has_more = values->count > LIT_CONTAINER_OUTPUT_MAX;
    value_amount = has_more ? LIT_CONTAINER_OUTPUT_MAX : values->count;
    values_converted = LIT_ALLOCATE(vm->state, LitString*, value_amount+1);
    keys = LIT_ALLOCATE(vm->state, LitString*, value_amount+1);
    olength = 3;
    if(has_more)
    {
        olength += SINGLE_LINE_MAPS_ENABLED ? 5 : 6;
    }
    i = 0;
    index = 0;
    do
    {
        entry = &values->entries[index++];
        if(entry->key != NULL)
        {
            // Special hidden key
            field = has_wrapper ? map->index_fn(vm, map, entry->key, NULL) : entry->value;
            // This check is required to prevent infinite loops when playing with Module.privates and such
            strobval = (IS_MAP(field) && AS_MAP(field)->index_fn != NULL) ? CONST_STRING(state, "map") : lit_to_string(state, field);
            lit_push_root(state, (LitObject*)strobval);
            values_converted[i] = strobval;
            keys[i] = entry->key;
            olength += (
                lit_string_length(entry->key) + 3 + lit_string_length(strobval) +
                #ifdef SINGLE_LINE_MAPS
                    (i == value_amount - 1 ? 1 : 2)
                #else
                    (i == value_amount - 1 ? 2 : 3)
                #endif
            );
            i++;
        }
    } while(i < value_amount);
    buffer = LIT_ALLOCATE(vm->state, char, olength+1);
    #ifdef SINGLE_LINE_MAPS
    memcpy(buffer, "{ ", 2);
    #else
    memcpy(buffer, "{\n", 2);
    #endif
    buffer_index = 2;
    for(i = 0; i < value_amount; i++)
    {
        key = keys[i];
        value = values_converted[i];
        #ifndef SINGLE_LINE_MAPS
        buffer[buffer_index++] = '\t';
        #endif
        memcpy(&buffer[buffer_index], key->chars, lit_string_length(key));
        buffer_index += lit_string_length(key);
        memcpy(&buffer[buffer_index], " = ", 3);
        buffer_index += 3;
        memcpy(&buffer[buffer_index], value->chars, lit_string_length(value));
        buffer_index += lit_string_length(value);
        if(has_more && i == value_amount - 1)
        {
            #ifdef SINGLE_LINE_MAPS
            memcpy(&buffer[buffer_index], ", ... }", 7);
            #else
            memcpy(&buffer[buffer_index], ",\n\t...\n}", 8);
            #endif
            buffer_index += 8;
        }
        else
        {
            #ifdef SINGLE_LINE_MAPS
            memcpy(&buffer[buffer_index], (i == value_amount - 1) ? " }" : ", ", 2);
            #else
            memcpy(&buffer[buffer_index], (i == value_amount - 1) ? "\n}" : ",\n", 2);
            #endif
            buffer_index += 2;
        }
        lit_pop_root(state);
    }
    buffer[olength] = '\0';
    LIT_FREE(vm->state, LitString*, keys);
    LIT_FREE(vm->state, LitString*, values_converted);
    return OBJECT_VALUE(lit_string_take(vm->state, buffer, olength));
}

static LitValue objfn_map_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(AS_MAP(instance)->values.count);
}

void lit_open_map_library(LitState* state)
{
    LIT_BEGIN_CLASS("Map");
    {
        LIT_INHERIT_CLASS(state->objectvalue_class);
        LIT_BIND_CONSTRUCTOR(objfn_map_constructor);
        LIT_BIND_METHOD("[]", objfn_map_subscript);
        LIT_BIND_METHOD("addAll", objfn_map_addall);
        LIT_BIND_METHOD("clear", objfn_map_clear);
        LIT_BIND_METHOD("iterator", objfn_map_iterator);
        LIT_BIND_METHOD("iteratorValue", objfn_map_iteratorvalue);
        LIT_BIND_METHOD("clone", objfn_map_clone);
        LIT_BIND_METHOD("toString", objfn_map_tostring);
        LIT_BIND_GETTER("length", objfn_map_length);
        state->mapvalue_class = klass;
    }
    LIT_END_CLASS();
}
