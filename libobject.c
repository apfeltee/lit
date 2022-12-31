
#include <memory.h>
#include <math.h>
#include "lit.h"

bool lit_is_callable_function(LitValue value)
{
    if(lit_value_isobject(value))
    {
        LitObjectType type = lit_value_type(value);
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


LitFunction* lit_create_function(LitState* state, LitModule* module)
{
    LitFunction* function;
    function = (LitFunction*)lit_allocate_object(state, sizeof(LitFunction), LITTYPE_FUNCTION, false);
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
            }
            break;
    }
    if(name == NULL)
    {
        return lit_value_objectvalue(lit_string_format(vm->state, "function #", *((double*)lit_value_asobject(instance))));
    }

    return lit_value_objectvalue(lit_string_format(vm->state, "function @", lit_value_objectvalue(name)));
}

LitUpvalue* lit_create_upvalue(LitState* state, LitValue* slot)
{
    LitUpvalue* upvalue;
    upvalue = (LitUpvalue*)lit_allocate_object(state, sizeof(LitUpvalue), LITTYPE_UPVALUE, false);
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
    closure = (LitClosure*)lit_allocate_object(state, sizeof(LitClosure), LITTYPE_CLOSURE, false);
    lit_state_pushroot(state, (LitObject*)closure);
    upvalues = LIT_ALLOCATE(state, sizeof(LitUpvalue*), function->upvalue_count);
    lit_state_poproot(state);
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
    native = (LitNativeFunction*)lit_allocate_object(state, sizeof(LitNativeFunction), LITTYPE_NATIVE_FUNCTION, false);
    native->function = function;
    native->name = name;
    return native;
}

LitNativePrimFunction* lit_create_native_primitive(LitState* state, LitNativePrimitiveFn function, LitString* name)
{
    LitNativePrimFunction* native;
    native = (LitNativePrimFunction*)lit_allocate_object(state, sizeof(LitNativePrimFunction), LITTYPE_NATIVE_PRIMITIVE, false);
    native->function = function;
    native->name = name;
    return native;
}

LitNativeMethod* lit_create_native_method(LitState* state, LitNativeMethodFn method, LitString* name)
{
    LitNativeMethod* native;
    native = (LitNativeMethod*)lit_allocate_object(state, sizeof(LitNativeMethod), LITTYPE_NATIVE_METHOD, false);
    native->method = method;
    native->name = name;
    return native;
}

LitPrimitiveMethod* lit_create_primitive_method(LitState* state, LitPrimitiveMethodFn method, LitString* name)
{
    LitPrimitiveMethod* native;
    native = (LitPrimitiveMethod*)lit_allocate_object(state, sizeof(LitPrimitiveMethod), LITTYPE_PRIMITIVE_METHOD, false);
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
    stack = LIT_ALLOCATE(state, sizeof(LitValue), stack_capacity);
    frames = LIT_ALLOCATE(state, sizeof(LitCallFrame), LIT_INITIAL_CALL_FRAMES);
    fiber = (LitFiber*)lit_allocate_object(state, sizeof(LitFiber), LITTYPE_FIBER, false);
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
    module = (LitModule*)lit_allocate_object(state, sizeof(LitModule), LITTYPE_MODULE, false);
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
    klass = (LitClass*)lit_allocate_object(state, sizeof(LitClass), LITTYPE_CLASS, false);
    klass->name = name;
    klass->init_method = NULL;
    klass->super = NULL;
    lit_table_init(state, &klass->methods);
    lit_table_init(state, &klass->static_fields);
    return klass;
}

LitInstance* lit_create_instance(LitState* state, LitClass* klass)
{
    LitInstance* instance;
    instance = (LitInstance*)lit_allocate_object(state, sizeof(LitInstance), LITTYPE_INSTANCE, false);
    instance->klass = klass;
    lit_table_init(state, &instance->fields);
    instance->fields.count = 0;
    return instance;
}

LitBoundMethod* lit_create_bound_method(LitState* state, LitValue receiver, LitValue method)
{
    LitBoundMethod* bound_method;
    bound_method = (LitBoundMethod*)lit_allocate_object(state, sizeof(LitBoundMethod), LITTYPE_BOUND_METHOD, false);
    bound_method->receiver = receiver;
    bound_method->method = method;
    return bound_method;
}

LitArray* lit_create_array(LitState* state)
{
    LitArray* array;
    array = (LitArray*)lit_allocate_object(state, sizeof(LitArray), LITTYPE_ARRAY, false);
    lit_vallist_init(&array->list);
    return array;
}

LitMap* lit_create_map(LitState* state)
{
    LitMap* map;
    map = (LitMap*)lit_allocate_object(state, sizeof(LitMap), LITTYPE_MAP, false);
    lit_table_init(state, &map->values);
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
    userdata = (LitUserdata*)lit_allocate_object(state, sizeof(LitUserdata), LITTYPE_USERDATA, false);
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
    range = (LitRange*)lit_allocate_object(state, sizeof(LitRange), LITTYPE_RANGE, false);
    range->from = from;
    range->to = to;
    return range;
}

LitField* lit_create_field(LitState* state, LitObject* getter, LitObject* setter)
{
    LitField* field;
    field = (LitField*)lit_allocate_object(state, sizeof(LitField), LITTYPE_FIELD, false);
    field->getter = getter;
    field->setter = setter;
    return field;
}

LitReference* lit_create_reference(LitState* state, LitValue* slot)
{
    LitReference* reference;
    reference = (LitReference*)lit_allocate_object(state, sizeof(LitReference), LITTYPE_REFERENCE, false);
    reference->slot = slot;
    return reference;
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

