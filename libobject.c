
#include <memory.h>
#include <math.h>
#include "lit.h"
#include "sds.h"

LitUpvalue* lit_create_upvalue(LitState* state, LitValue* slot)
{
    LitUpvalue* upvalue;
    upvalue = (LitUpvalue*)lit_gcmem_allocobject(state, sizeof(LitUpvalue), LITTYPE_UPVALUE, false);
    upvalue->location = slot;
    upvalue->closed = NULL_VALUE;
    upvalue->next = NULL;
    return upvalue;
}

LitModule* lit_create_module(LitState* state, LitString* name)
{
    LitModule* module;
    module = (LitModule*)lit_gcmem_allocobject(state, sizeof(LitModule), LITTYPE_MODULE, false);
    module->name = name;
    module->return_value = NULL_VALUE;
    module->main_function = NULL;
    module->privates = NULL;
    module->ran = false;
    module->main_fiber = NULL;
    module->private_count = 0;
    module->private_names = lit_create_map(state);
    return module;
}

LitUserdata* lit_create_userdata(LitState* state, size_t size, bool ispointeronly)
{
    LitUserdata* userdata;
    userdata = (LitUserdata*)lit_gcmem_allocobject(state, sizeof(LitUserdata), LITTYPE_USERDATA, false);
    userdata->data = NULL;
    if(size > 0)
    {
        if(!ispointeronly)
        {
            userdata->data = lit_gcmem_memrealloc(state, NULL, 0, size);
        }
    }
    userdata->size = size;
    userdata->cleanup_fn = NULL;
    userdata->canfree = true;
    return userdata;
}

LitRange* lit_create_range(LitState* state, double from, double to)
{
    LitRange* range;
    range = (LitRange*)lit_gcmem_allocobject(state, sizeof(LitRange), LITTYPE_RANGE, false);
    range->from = from;
    range->to = to;
    return range;
}

LitReference* lit_create_reference(LitState* state, LitValue* slot)
{
    LitReference* reference;
    reference = (LitReference*)lit_gcmem_allocobject(state, sizeof(LitReference), LITTYPE_REFERENCE, false);
    reference->slot = slot;
    return reference;
}

void lit_object_destroy(LitState* state, LitObject* object)
{
    LitString* string;
    LitFunction* function;
    LitFiber* fiber;
    LitModule* module;
    LitClosure* closure;
#ifdef LIT_LOG_ALLOCATION
    printf("(");
    lit_towriter_value(lit_value_objectvalue(object));
    printf(") %p free %s\n", (void*)object, lit_tostring_typename(object->type));
#endif

    switch(object->type)
    {
        case LITTYPE_NUMBER:
            {
                LitNumber* n = (LitNumber*)object;
                if(object->mustfree)
                {
                    LIT_FREE(state, sizeof(LitNumber), object);
                }
            }
            break;
        case LITTYPE_STRING:
            {
                string = (LitString*)object;
                //LIT_FREE_ARRAY(state, sizeof(char), string->chars, string->length + 1);
                sdsfree(string->chars);
                string->chars = NULL;
                LIT_FREE(state, sizeof(LitString), object);
            }
            break;

        case LITTYPE_FUNCTION:
            {
                function = (LitFunction*)object;
                lit_free_chunk(state, &function->chunk);
                LIT_FREE(state, sizeof(LitFunction), object);
            }
            break;
        case LITTYPE_NATIVE_FUNCTION:
            {
                LIT_FREE(state, sizeof(LitNativeFunction), object);
            }
            break;
        case LITTYPE_NATIVE_PRIMITIVE:
            {
                LIT_FREE(state, sizeof(LitNativePrimFunction), object);
            }
            break;
        case LITTYPE_NATIVE_METHOD:
            {
                LIT_FREE(state, sizeof(LitNativeMethod), object);
            }
            break;
        case LITTYPE_PRIMITIVE_METHOD:
            {
                LIT_FREE(state, sizeof(LitPrimitiveMethod), object);
            }
            break;
        case LITTYPE_FIBER:
            {
                fiber = (LitFiber*)object;
                LIT_FREE_ARRAY(state, sizeof(LitCallFrame), fiber->frames, fiber->frame_capacity);
                LIT_FREE_ARRAY(state, sizeof(LitValue), fiber->stack, fiber->stack_capacity);
                LIT_FREE(state, sizeof(LitFiber), object);
            }
            break;
        case LITTYPE_MODULE:
            {
                module = (LitModule*)object;
                LIT_FREE_ARRAY(state, sizeof(LitValue), module->privates, module->private_count);
                LIT_FREE(state, sizeof(LitModule), object);
            }
            break;
        case LITTYPE_CLOSURE:
            {
                closure = (LitClosure*)object;
                LIT_FREE_ARRAY(state, sizeof(LitUpvalue*), closure->upvalues, closure->upvalue_count);
                LIT_FREE(state, sizeof(LitClosure), object);
            }
            break;
        case LITTYPE_UPVALUE:
            {
                LIT_FREE(state, sizeof(LitUpvalue), object);
            }
            break;
        case LITTYPE_CLASS:
            {
                LitClass* klass = (LitClass*)object;
                lit_table_destroy(state, &klass->methods);
                lit_table_destroy(state, &klass->static_fields);
                LIT_FREE(state, sizeof(LitClass), object);
            }
            break;

        case LITTYPE_INSTANCE:
            {
                lit_table_destroy(state, &((LitInstance*)object)->fields);
                LIT_FREE(state, sizeof(LitInstance), object);
            }
            break;
        case LITTYPE_BOUND_METHOD:
            {
                LIT_FREE(state, sizeof(LitBoundMethod), object);
            }
            break;
        case LITTYPE_ARRAY:
            {
                lit_vallist_destroy(state, &((LitArray*)object)->list);
                LIT_FREE(state, sizeof(LitArray), object);
            }
            break;
        case LITTYPE_MAP:
            {
                lit_table_destroy(state, &((LitMap*)object)->values);
                LIT_FREE(state, sizeof(LitMap), object);
            }
            break;
        case LITTYPE_USERDATA:
            {
                LitUserdata* data = (LitUserdata*)object;
                if(data->cleanup_fn != NULL)
                {
                    data->cleanup_fn(state, data, false);
                }
                if(data->size > 0)
                {
                    if(data->canfree)
                    {
                        lit_gcmem_memrealloc(state, data->data, data->size, 0);
                    }
                }
                LIT_FREE(state, sizeof(LitUserdata), data);
                //free(data);
            }
            break;
        case LITTYPE_RANGE:
            {
                LIT_FREE(state, sizeof(LitRange), object);
            }
            break;
        case LITTYPE_FIELD:
            {
                LIT_FREE(state, sizeof(LitField), object);
            }
            break;
        case LITTYPE_REFERENCE:
            {
                LIT_FREE(state, sizeof(LitReference), object);
            }
            break;
        default:
            {
                fprintf(stderr, "internal error: trying to free something else!\n");
                UNREACHABLE
            }
            break;
    }
}

void lit_object_destroylistof(LitState* state, LitObject* objects)
{
    LitObject* object = objects;

    while(object != NULL)
    {
        LitObject* next = object->next;
        lit_object_destroy(state, object);
        object = next;
    }

    free(state->vm->gray_stack);
    state->vm->gray_capacity = 0;
}

LitValue lit_get_function_name(LitVM* vm, LitValue instance)
{
    LitString* name;
    LitField* field;
    name = NULL;
    switch(lit_value_type(instance))
    {
        case LITTYPE_FUNCTION:
            {
                name = lit_value_asfunction(instance)->name;
            }
            break;
        case LITTYPE_CLOSURE:
            {
                name = lit_value_asclosure(instance)->function->name;
            }
            break;
        case LITTYPE_FIELD:
            {
                field = lit_value_asfield(instance);
                if(field->getter != NULL)
                {
                    return lit_get_function_name(vm, lit_value_objectvalue(field->getter));
                }
                return lit_get_function_name(vm, lit_value_objectvalue(field->setter));
            }
            break;
        case LITTYPE_NATIVE_PRIMITIVE:
            {
                name = lit_value_asnativeprimitive(instance)->name;
            }
            break;
        case LITTYPE_NATIVE_FUNCTION:
            {
                name = lit_value_asnativefunction(instance)->name;
            }
            break;
        case LITTYPE_NATIVE_METHOD:
            {
                name = lit_value_asnativemethod(instance)->name;
            }
            break;
        case LITTYPE_PRIMITIVE_METHOD:
            {
                name = lit_value_asprimitivemethod(instance)->name;
            }
            break;
        case LITTYPE_BOUND_METHOD:
            {
                return lit_get_function_name(vm, lit_value_asboundmethod(instance)->method);
            }
            break;
        default:
            {
                //return NULL_VALUE;
            }
            break;
    }
    if(name == NULL)
    {
        if(lit_value_isobject(instance))
        {
            return lit_value_objectvalue(lit_string_format(vm->state, "function #", *((double*)lit_value_asobject(instance))));
        }
    }
    return lit_value_objectvalue(lit_string_format(vm->state, "function @", lit_value_objectvalue(name)));
}

static LitValue objfn_object_class(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(lit_state_getclassfor(vm->state, instance));
}

static LitValue objfn_object_super(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitClass* cl;
    cl = lit_state_getclassfor(vm->state, instance)->super;
    if(cl == NULL)
    {
        return NULL_VALUE;
    }
    return lit_value_objectvalue(cl);
}

static LitValue objfn_object_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(lit_string_format(vm->state, "@ instance", lit_value_objectvalue(lit_state_getclassfor(vm->state, instance)->name)));
}

static void fillmap(LitState* state, LitMap* destmap, LitTable* fromtbl, bool includenullkeys)
{
    size_t i;
    LitString* key;
    LitValue val;
    (void)includenullkeys;
    for(i=0; i<(size_t)(fromtbl->count); i++)
    {
        key = fromtbl->entries[i].key;
        if(key != NULL)
        {
            val = fromtbl->entries[i].value;
            lit_map_set(state, destmap, key, lit_value_objectvalue(val));
        }
    }
}

static LitValue objfn_object_tomap(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitMap* map;
    LitMap* minst;
    LitMap* mclass;
    LitMap* mclstatics;
    LitMap* mclmethods;
    LitInstance* inst;
    mclass = NULL;
    if(!lit_value_isinstance(instance))
    {
        lit_runtime_error_exiting(vm, "toMap() can only be used on instances");
    }
    inst = lit_value_asinstance(instance);
    map = lit_create_map(vm->state);
    {
        minst = lit_create_map(vm->state);
        fillmap(vm->state, minst, &(inst->fields), true);
    }
    {
        mclass = lit_create_map(vm->state);
        {
            mclstatics = lit_create_map(vm->state);
            fillmap(vm->state, mclstatics, &(inst->klass->static_fields), false);
        }
        {
            mclmethods = lit_create_map(vm->state);
            fillmap(vm->state, mclmethods, &(inst->klass->methods), false);
        }
        lit_map_set(vm->state, mclass, CONST_STRING(vm->state, "statics"), lit_value_objectvalue(mclstatics));
        lit_map_set(vm->state, mclass, CONST_STRING(vm->state, "methods"), lit_value_objectvalue(mclmethods));
    }
    lit_map_set(vm->state, map, CONST_STRING(vm->state, "instance"), lit_value_objectvalue(minst));
    lit_map_set(vm->state, map, CONST_STRING(vm->state, "class"), lit_value_objectvalue(mclass));
    return lit_value_objectvalue(map);
}

static LitValue objfn_object_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitValue value;
    LitInstance* inst;
    if(!lit_value_isinstance(instance))
    {
        lit_runtime_error_exiting(vm, "cannot modify built-in types");
    }
    inst = lit_value_asinstance(instance);
    if(argc == 2)
    {
        if(!lit_value_isstring(argv[0]))
        {
            lit_runtime_error_exiting(vm, "object index must be a string");
        }

        lit_table_set(vm->state, &inst->fields, lit_value_asstring(argv[0]), argv[1]);
        return argv[1];
    }
    if(!lit_value_isstring(argv[0]))
    {
        lit_runtime_error_exiting(vm, "object index must be a string");
    }
    if(lit_table_get(&inst->fields, lit_value_asstring(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&inst->klass->static_fields, lit_value_asstring(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&inst->klass->methods, lit_value_asstring(argv[0]), &value))
    {
        return value;
    }
    return NULL_VALUE;
}

static LitValue objfn_object_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    int value;
    int index;
    LitInstance* self;
    LIT_ENSURE_ARGS(1);
    self = lit_value_asinstance(instance);
    index = argv[0] == NULL_VALUE ? -1 : lit_value_asnumber(argv[0]);
    value = util_table_iterator(&self->fields, index);
    return value == -1 ? NULL_VALUE : lit_value_numbertovalue(vm->state, value);
}


static LitValue objfn_object_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index;
    LitInstance* self;
    index = lit_check_number(vm, argv, argc, 0);
    self = lit_value_asinstance(instance);
    return util_table_iterator_key(&self->fields, index);
}

void lit_open_object_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "Object");
    {
        lit_class_inheritfrom(state, klass, state->classvalue_class);
        lit_class_bindgetset(state, klass, "class", objfn_object_class, NULL, false);
        lit_class_bindgetset(state, klass, "super", objfn_object_super, NULL, false);
        lit_class_bindmethod(state, klass, "[]", objfn_object_subscript);
        #if 0
        lit_class_bindmethod(state, klass, "hasMethod", objfn_object_hasmethod);
        #endif
        lit_class_bindmethod(state, klass, "toString", objfn_object_tostring);
        lit_class_bindmethod(state, klass, "toMap", objfn_object_tomap);
        lit_class_bindmethod(state, klass, "iterator", objfn_object_iterator);
        lit_class_bindmethod(state, klass, "iteratorValue", objfn_object_iteratorvalue);
        state->objectvalue_class = klass;
        state->objectvalue_class->super = state->classvalue_class;
    }
    lit_set_global(state, klass->name, lit_value_objectvalue(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    };
}

