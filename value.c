
#include <stdio.h>
#include "lit.h"


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


void lit_init_values(LitValueList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_free_values(LitState* state, LitValueList* array)
{
    LIT_FREE_ARRAY(state, LitValue, array->values, array->capacity);
    lit_init_values(array);
}

void lit_values_write(LitState* state, LitValueList* array, LitValue value)
{
    size_t old_capacity;
    if(array->capacity < array->count + 1)
    {
        old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, LitValue, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}


static void print_array(LitState* state, LitArray* array, size_t size)
{
    size_t i;
    printf("(%u) [", (unsigned int)size);
    if(size > 0)
    {
        printf(" ");
        for(i = 0; i < size; i++)
        {
            if(IS_ARRAY(array->values.values[i]) && (array == AS_ARRAY(array->values.values[i])))
            {
                printf("(recursion)");
            }
            else
            {
                lit_print_value(state, array->values.values[i]);
            }
            if(i + 1 < size)
            {
                printf(", ");
            }
            else
            {
                printf(" ");
            }
        }
    }
    printf("]");
}

static void print_map(LitState* state, LitMap* map, size_t size)
{
    bool had_before;
    size_t i;
    LitTableEntry* entry;
    printf("(%u) {", (unsigned int)size);
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
                    printf(", ");
                }
                else
                {
                    printf(" ");
                }
                printf("%s = ", entry->key->chars);
                if(IS_MAP(entry->value) && (map == AS_MAP(entry->value)))
                {
                    printf("(recursion)");
                }
                else
                {
                    lit_print_value(state, entry->value);
                }
                had_before = true;
            }
        }
    }
    if(had_before)
    {
        printf(" }");
    }
    else
    {
        printf("}");
    }
}

static void print_object(LitState* state, LitValue value)
{
    LitObject* obj;
    obj = AS_OBJECT(value);
    if(obj != NULL)
    {
        switch(obj->type)
        {
            case LITTYPE_STRING:
                {
                    printf("%s", AS_CSTRING(value));
                }
                break;
            case LITTYPE_FUNCTION:
                {
                    printf("function %s", AS_FUNCTION(value)->name->chars);
                }
                break;
            case LITTYPE_CLOSURE:
                {
                    printf("closure %s", AS_CLOSURE(value)->function->name->chars);
                }
                break;
            case LITTYPE_NATIVE_PRIMITIVE:
                {
                    printf("function %s", AS_NATIVE_PRIMITIVE(value)->name->chars);
                }
                break;
            case LITTYPE_NATIVE_FUNCTION:
                {
                    printf("function %s", AS_NATIVE_FUNCTION(value)->name->chars);
                }
                break;
            case LITTYPE_PRIMITIVE_METHOD:
                {
                    printf("function %s", AS_PRIMITIVE_METHOD(value)->name->chars);
                }
                break;
            case LITTYPE_NATIVE_METHOD:
                {
                    printf("function %s", AS_NATIVE_METHOD(value)->name->chars);
                }
                break;
            case LITTYPE_FIBER:
                {
                    printf("fiber");
                }
                break;
            case LITTYPE_MODULE:
                {
                    printf("module %s", AS_MODULE(value)->name->chars);
                }
                break;

            case LITTYPE_UPVALUE:
                {
                    LitUpvalue* upvalue = AS_UPVALUE(value);
                    if(upvalue->location == NULL)
                    {
                        lit_print_value(state, upvalue->closed);
                    }
                    else
                    {
                        print_object(state, *upvalue->location);
                    }
                }
                break;
            case LITTYPE_CLASS:
                {
                    printf("class %s", AS_CLASS(value)->name->chars);
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
                    printf("<instance '%s' ", AS_INSTANCE(value)->klass->name->chars);
                    LitMap* map = AS_MAP(value);
                    size_t size = map->values.count;
                    print_map(state, map, size);
                    printf(">");
                }
                break;
            case LITTYPE_BOUND_METHOD:
                {
                    lit_print_value(state, AS_BOUND_METHOD(value)->method);
                    return;
                }
                break;
            case LITTYPE_ARRAY:
                {
                    #ifdef LIT_MINIMIZE_CONTAINERS
                        printf("array");
                    #else
                        LitArray* array = AS_ARRAY(value);
                        size_t size = array->values.count;
                        print_array(state, array, size);
                    #endif
                }
                break;
            case LITTYPE_MAP:
                {
                    #ifdef LIT_MINIMIZE_CONTAINERS
                        printf("map");
                    #else
                        LitMap* map = AS_MAP(value);
                        size_t size = map->values.count;
                        print_map(state, map, size);
                    #endif
                }
                break;
            case LITTYPE_USERDATA:
                {
                    printf("userdata");
                }
                break;
            case LITTYPE_RANGE:
                {
                    LitRange* range = AS_RANGE(value);
                    printf("%g .. %g", range->from, range->to);
                }
                break;
            case LITTYPE_FIELD:
                {
                    printf("field");
                }
                break;
            case LITTYPE_REFERENCE:
                {
                    printf("reference => ");
                    LitValue* slot = AS_REFERENCE(value)->slot;
                    if(slot == NULL)
                    {
                        printf("null");
                    }
                    else
                    {
                        lit_print_value(state, *slot);
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
        printf("!nullpointer!");
    }
}

//LitInterpretResult lit_call_instance_method(LitState* state, LitInstance* instance, LitString* mthname, LitValue* argv, size_t argc)
//
void lit_print_value(LitState* state, LitValue value)
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
        if(!IS_NULL(mthtostring))
        {
            fprintf(stderr, "lit_print_value: we got toString()! now checking if calling it works ...\n");
            inret = lit_instance_call_method(state, value, mthname, args, 0);
            if(inret.type == LITRESULT_OK)
            {
                fprintf(stderr, "lit_print_value: calling toString() succeeded! but is it a string? ...\n");
                tstrval = inret.result;
                if(!IS_NULL(tstrval))
                {
                    fprintf(stderr, "lit_print_value: toString() returned a string! so that's what we'll use.\n");
                    tstring = AS_STRING(tstrval);
                    printf("%.*s", (int)lit_string_length(tstring), tstring->chars);
                    return;
                }
            }
        }
    }
    fprintf(stderr, "lit_print_value: nope, no toString(), or it didn't return a string. falling back to manual stringification\n");
    */
    if(IS_BOOL(value))
    {
        printf(AS_BOOL(value) ? "true" : "false");
    }
    else if(IS_NULL(value))
    {
        printf("null");
    }
    else if(IS_NUMBER(value))
    {
        printf("%g", lit_value_to_number(value));
    }
    else if(IS_OBJECT(value))
    {
        print_object(state, value);
    }
}

void lit_values_ensure_size(LitState* state, LitValueList* values, size_t size)
{
    size_t i;
    size_t old_capacity;
    if(values->capacity < size)
    {
        old_capacity = values->capacity;
        values->capacity = size;
        values->values = LIT_GROW_ARRAY(state, values->values, LitValue, old_capacity, size);
        for(i = old_capacity; i < size; i++)
        {
            values->values[i] = NULL_VALUE;
        }
    }
    if(values->count < size)
    {
        values->count = size;
    }
}

const char* lit_get_value_type(LitValue value)
{
    if(IS_BOOL(value))
    {
        return "bool";
    }
    else if(IS_NULL(value))
    {
        return "null";
    }
    else if(IS_NUMBER(value))
    {
        return "number";
    }
    else if(IS_OBJECT(value))
    {
        return lit_object_type_names[OBJECT_TYPE(value)];
    }
    return "unknown";
}
