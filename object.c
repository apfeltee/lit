
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
        return OBJECT_VALUE(lit_string_format(vm->state, "function #", *((double*)AS_OBJECT(instance))));
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

LitNativePrimitive* lit_create_native_primitive(LitState* state, LitNativePrimitiveFn function, LitString* name)
{
    LitNativePrimitive* native;
    native = ALLOCATE_OBJECT(state, LitNativePrimitive, LITTYPE_NATIVE_PRIMITIVE);
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
    if(module->main_fiber == NULL)
    {
        module->main_fiber = fiber;
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
    lit_init_values(&array->values);
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

LitUserdata* lit_create_userdata(LitState* state, size_t size)
{
    LitUserdata* userdata;
    userdata = ALLOCATE_OBJECT(state, LitUserdata, LITTYPE_USERDATA);
    if(size > 0)
    {
        userdata->data = lit_reallocate(state, NULL, 0, size);
    }
    else
    {
        userdata->data = NULL;
    }
    userdata->size = size;
    userdata->cleanup_fn = NULL;
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
