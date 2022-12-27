
#include <memory.h>
#include <math.h>
#include "lit.h"

bool lit_is_callable_function(LitValue value)
{
    if(IS_OBJECT(value))
    {
        LitObjectType type = OBJECT_TYPE(value);
        return (
            (type == LITTYPE_CLOSURE) ||
            (type == LITTYPE_FUNCTION) ||
            (type == LITTYPE_NATIVE_FUNCTION) ||
            (type == LITTYPE_NATIVE_PRIMITIVE) ||
            (type == LITTYPE_NATIVE_METHOD) ||
            (type == LITTYPE_PRIMITIVE_METHOD) ||
            (type == LITTYPE_BOUND_METHOD)
        );
    }

    return false;
}

LitObject* lit_allocate_object(LitState* state, size_t size, LitObjectType type)
{
    LitObject* object;
    object = (LitObject*)lit_reallocate(state, NULL, 0, size);
    object->type = type;
    object->marked = false;
    object->next = state->vm->objects;
    state->vm->objects = object;
#ifdef LIT_LOG_ALLOCATION
    printf("%p allocate %ld for %s\n", (void*)object, size, lit_get_value_type(type));
#endif
    return object;
}

LitFunction* lit_create_function(LitState* state, LitModule* module)
{
    LitFunction* function;
    function = ALLOCATE_OBJECT(state, LitFunction, LITTYPE_FUNCTION);
    lit_init_chunk(&function->chunk);
    function->name = NULL;
    function->arg_count = 0;
    function->upvalue_count = 0;
    function->max_slots = 0;
    function->module = module;
    function->vararg = false;
    return function;
}

LitValue lit_get_function_name(LitVM* vm, LitValue instance)
{
    LitString* name;
    LitField* field;
    name = NULL;
    switch(OBJECT_TYPE(instance))
    {
        case LITTYPE_FUNCTION:
            {
                name = AS_FUNCTION(instance)->name;
            }
            break;
        case LITTYPE_CLOSURE:
            {
                name = AS_CLOSURE(instance)->function->name;
            }
            break;
        case LITTYPE_FIELD:
            {
                field = AS_FIELD(instance);
                if(field->getter != NULL)
                {
                    return lit_get_function_name(vm, OBJECT_VALUE(field->getter));
                }
                return lit_get_function_name(vm, OBJECT_VALUE(field->setter));
            }
            break;
        case LITTYPE_NATIVE_PRIMITIVE:
            {
                name = AS_NATIVE_PRIMITIVE(instance)->name;
            }
            break;
        case LITTYPE_NATIVE_FUNCTION:
            {
                name = AS_NATIVE_FUNCTION(instance)->name;
            }
            break;
        case LITTYPE_NATIVE_METHOD:
            {
                name = AS_NATIVE_METHOD(instance)->name;
            }
            break;
        case LITTYPE_PRIMITIVE_METHOD:
            {
                name = AS_PRIMITIVE_METHOD(instance)->name;
            }
            break;
        case LITTYPE_BOUND_METHOD:
            {
                return lit_get_function_name(vm, AS_BOUND_METHOD(instance)->method);
            }
            break;
        default:
            {
            }
            break;
    }
    if(name == NULL)
    {
        return OBJECT_VALUE(lit_string_format(vm->state, "function #", *((double*)lit_as_object(instance))));
    }

    return OBJECT_VALUE(lit_string_format(vm->state, "function @", OBJECT_VALUE(name)));
}

LitUpvalue* lit_create_upvalue(LitState* state, LitValue* slot)
{
    LitUpvalue* upvalue;
    upvalue = ALLOCATE_OBJECT(state, LitUpvalue, LITTYPE_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NULL_VALUE;
    upvalue->next = NULL;
    return upvalue;
}

LitClosure* lit_create_closure(LitState* state, LitFunction* function)
{
    size_t i;
    LitClosure* closure;
    LitUpvalue** upvalues;
    closure = ALLOCATE_OBJECT(state, LitClosure, LITTYPE_CLOSURE);
    lit_push_root(state, (LitObject*)closure);
    upvalues = LIT_ALLOCATE(state, LitUpvalue*, function->upvalue_count);
    lit_pop_root(state);
    for(i = 0; i < function->upvalue_count; i++)
    {
        upvalues[i] = NULL;
    }
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

LitNativeFunction* lit_create_native_function(LitState* state, LitNativeFunctionFn function, LitString* name)
{
    LitNativeFunction* native;
    native = ALLOCATE_OBJECT(state, LitNativeFunction, LITTYPE_NATIVE_FUNCTION);
    native->function = function;
    native->name = name;
    return native;
}

LitNativePrimFunction* lit_create_native_primitive(LitState* state, LitNativePrimitiveFn function, LitString* name)
{
    LitNativePrimFunction* native;
    native = ALLOCATE_OBJECT(state, LitNativePrimFunction, LITTYPE_NATIVE_PRIMITIVE);
    native->function = function;
    native->name = name;
    return native;
}

LitNativeMethod* lit_create_native_method(LitState* state, LitNativeMethodFn method, LitString* name)
{
    LitNativeMethod* native;
    native = ALLOCATE_OBJECT(state, LitNativeMethod, LITTYPE_NATIVE_METHOD);
    native->method = method;
    native->name = name;
    return native;
}

LitPrimitiveMethod* lit_create_primitive_method(LitState* state, LitPrimitiveMethodFn method, LitString* name)
{
    LitPrimitiveMethod* native;
    native = ALLOCATE_OBJECT(state, LitPrimitiveMethod, LITTYPE_PRIMITIVE_METHOD);
    native->method = method;
    native->name = name;
    return native;
}

LitFiber* lit_create_fiber(LitState* state, LitModule* module, LitFunction* function)
{
    size_t stack_capacity;
    LitValue* stack;
    LitCallFrame* frame;
    LitCallFrame* frames;
    LitFiber* fiber;
    // Allocate in advance, just in case GC is triggered
    stack_capacity = function == NULL ? 1 : (size_t)lit_closest_power_of_two(function->max_slots + 1);
    stack = LIT_ALLOCATE(state, LitValue, stack_capacity);
    frames = LIT_ALLOCATE(state, LitCallFrame, LIT_INITIAL_CALL_FRAMES);
    fiber = ALLOCATE_OBJECT(state, LitFiber, LITTYPE_FIBER);
    if(module != NULL)
    {
        if(module->main_fiber == NULL)
        {
            module->main_fiber = fiber;
        }
    }
    fiber->stack = stack;
    fiber->stack_capacity = stack_capacity;
    fiber->stack_top = fiber->stack;
    fiber->frames = frames;
    fiber->frame_capacity = LIT_INITIAL_CALL_FRAMES;
    fiber->parent = NULL;
    fiber->frame_count = 1;
    fiber->arg_count = 0;
    fiber->module = module;
    fiber->catcher = false;
    fiber->error = NULL_VALUE;
    fiber->open_upvalues = NULL;
    fiber->abort = false;
    frame = &fiber->frames[0];
    frame->closure = NULL;
    frame->function = function;
    frame->slots = fiber->stack;
    frame->result_ignored = false;
    frame->return_to_c = false;
    if(function != NULL)
    {
        frame->ip = function->chunk.code;
    }
    return fiber;
}

void lit_ensure_fiber_stack(LitState* state, LitFiber* fiber, size_t needed)
{
    size_t i;
    size_t capacity;
    LitValue* old_stack;
    LitUpvalue* upvalue;
    if(fiber->stack_capacity >= needed)
    {
        return;
    }
    capacity = (size_t)lit_closest_power_of_two((int)needed);
    old_stack = fiber->stack;
    fiber->stack = (LitValue*)lit_reallocate(state, fiber->stack, sizeof(LitValue) * fiber->stack_capacity, sizeof(LitValue) * capacity);
    fiber->stack_capacity = capacity;
    if(fiber->stack != old_stack)
    {
        for(i = 0; i < fiber->frame_capacity; i++)
        {
            LitCallFrame* frame = &fiber->frames[i];
            frame->slots = fiber->stack + (frame->slots - old_stack);
        }
        for(upvalue = fiber->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
        {
            upvalue->location = fiber->stack + (upvalue->location - old_stack);
        }
        fiber->stack_top = fiber->stack + (fiber->stack_top - old_stack);
    }
}

LitModule* lit_create_module(LitState* state, LitString* name)
{
    LitModule* module;
    module = ALLOCATE_OBJECT(state, LitModule, LITTYPE_MODULE);
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

LitClass* lit_create_class(LitState* state, LitString* name)
{
    LitClass* klass;
    klass = ALLOCATE_OBJECT(state, LitClass, LITTYPE_CLASS);
    klass->name = name;
    klass->init_method = NULL;
    klass->super = NULL;
    lit_init_table(&klass->methods);
    lit_init_table(&klass->static_fields);
    return klass;
}

LitInstance* lit_create_instance(LitState* state, LitClass* klass)
{
    LitInstance* instance;
    instance = ALLOCATE_OBJECT(state, LitInstance, LITTYPE_INSTANCE);
    instance->klass = klass;
    lit_init_table(&instance->fields);
    instance->fields.count = 0;
    return instance;
}

LitBoundMethod* lit_create_bound_method(LitState* state, LitValue receiver, LitValue method)
{
    LitBoundMethod* bound_method;
    bound_method = ALLOCATE_OBJECT(state, LitBoundMethod, LITTYPE_BOUND_METHOD);
    bound_method->receiver = receiver;
    bound_method->method = method;
    return bound_method;
}

LitArray* lit_create_array(LitState* state)
{
    LitArray* array;
    array = ALLOCATE_OBJECT(state, LitArray, LITTYPE_ARRAY);
    lit_vallist_init(&array->list);
    return array;
}

LitMap* lit_create_map(LitState* state)
{
    LitMap* map;
    map = ALLOCATE_OBJECT(state, LitMap, LITTYPE_MAP);
    lit_init_table(&map->values);
    map->index_fn = NULL;
    return map;
}

bool lit_map_set(LitState* state, LitMap* map, LitString* key, LitValue value)
{
    if(value == NULL_VALUE)
    {
        lit_map_delete(map, key);
        return false;
    }
    return lit_table_set(state, &map->values, key, value);
}

bool lit_map_get(LitMap* map, LitString* key, LitValue* value)
{
    return lit_table_get(&map->values, key, value);
}

bool lit_map_delete(LitMap* map, LitString* key)
{
    return lit_table_delete(&map->values, key);
}

void lit_map_add_all(LitState* state, LitMap* from, LitMap* to)
{
    int i;
    LitTableEntry* entry;
    for(i = 0; i <= from->values.capacity; i++)
    {
        entry = &from->values.entries[i];
        if(entry->key != NULL)
        {
            lit_table_set(state, &to->values, entry->key, entry->value);
        }
    }
}

LitUserdata* lit_create_userdata(LitState* state, size_t size, bool ispointeronly)
{
    LitUserdata* userdata;
    userdata = ALLOCATE_OBJECT(state, LitUserdata, LITTYPE_USERDATA);
    userdata->data = NULL;
    if(size > 0)
    {
        if(!ispointeronly)
        {
            userdata->data = lit_reallocate(state, NULL, 0, size);
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
    range = ALLOCATE_OBJECT(state, LitRange, LITTYPE_RANGE);
    range->from = from;
    range->to = to;
    return range;
}

LitField* lit_create_field(LitState* state, LitObject* getter, LitObject* setter)
{
    LitField* field;
    field = ALLOCATE_OBJECT(state, LitField, LITTYPE_FIELD);
    field->getter = getter;
    field->setter = setter;
    return field;
}

LitReference* lit_create_reference(LitState* state, LitValue* slot)
{
    LitReference* reference;
    reference = ALLOCATE_OBJECT(state, LitReference, LITTYPE_REFERENCE);
    reference->slot = slot;
    return reference;
}


static LitValue objfn_object_class(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_get_class_for(vm->state, instance));
}

static LitValue objfn_object_super(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitClass* cl;
    cl = lit_get_class_for(vm->state, instance)->super;
    if(cl == NULL)
    {
        return NULL_VALUE;
    }
    return OBJECT_VALUE(cl);
}

static LitValue objfn_object_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_format(vm->state, "@ instance", OBJECT_VALUE(lit_get_class_for(vm->state, instance)->name)));
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
            lit_map_set(state, destmap, key, OBJECT_VALUE(val));
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
    if(!IS_INSTANCE(instance))
    {
        lit_runtime_error_exiting(vm, "toMap() can only be used on instances");
    }
    inst = AS_INSTANCE(instance);
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
        lit_map_set(vm->state, mclass, CONST_STRING(vm->state, "statics"), OBJECT_VALUE(mclstatics));
        lit_map_set(vm->state, mclass, CONST_STRING(vm->state, "methods"), OBJECT_VALUE(mclmethods));
    }
    lit_map_set(vm->state, map, CONST_STRING(vm->state, "instance"), OBJECT_VALUE(minst));
    lit_map_set(vm->state, map, CONST_STRING(vm->state, "class"), OBJECT_VALUE(mclass));
    return OBJECT_VALUE(map);
}

static LitValue objfn_object_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitValue value;
    LitInstance* inst;
    if(!IS_INSTANCE(instance))
    {
        lit_runtime_error_exiting(vm, "cannot modify built-in types");
    }
    inst = AS_INSTANCE(instance);
    if(argc == 2)
    {
        if(!IS_STRING(argv[0]))
        {
            lit_runtime_error_exiting(vm, "object index must be a string");
        }

        lit_table_set(vm->state, &inst->fields, lit_as_string(argv[0]), argv[1]);
        return argv[1];
    }
    if(!IS_STRING(argv[0]))
    {
        lit_runtime_error_exiting(vm, "object index must be a string");
    }
    if(lit_table_get(&inst->fields, lit_as_string(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&inst->klass->static_fields, lit_as_string(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&inst->klass->methods, lit_as_string(argv[0]), &value))
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
    self = AS_INSTANCE(instance);
    index = argv[0] == NULL_VALUE ? -1 : lit_value_to_number(argv[0]);
    value = util_table_iterator(&self->fields, index);
    return value == -1 ? NULL_VALUE : lit_number_to_value(value);
}


static LitValue objfn_object_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index;
    LitInstance* self;
    index = lit_check_number(vm, argv, argc, 0);
    self = AS_INSTANCE(instance);
    return util_table_iterator_key(&self->fields, index);
}

void lit_open_object_library(LitState* state)
{
    LIT_BEGIN_CLASS("Object");
    {
        LIT_INHERIT_CLASS(state->classvalue_class);
        LIT_BIND_GETTER("class", objfn_object_class);
        LIT_BIND_GETTER("super", objfn_object_super);
        LIT_BIND_METHOD("[]", objfn_object_subscript);
        #if 0
        LIT_BIND_METHOD("hasMethod", objfn_object_hasmethod);
        
        #endif
        LIT_BIND_METHOD("toString", objfn_object_tostring);
        LIT_BIND_METHOD("toMap", objfn_object_tomap);
        LIT_BIND_METHOD("iterator", objfn_object_iterator);
        LIT_BIND_METHOD("iteratorValue", objfn_object_iteratorvalue);

        state->objectvalue_class = klass;
        state->objectvalue_class->super = state->classvalue_class;
    }
    LIT_END_CLASS();
}
