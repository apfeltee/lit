
#include "lit.h"

LitFunction* lit_create_function(LitState* state, LitModule* module)
{
    LitFunction* function;
    function = (LitFunction*)lit_gcmem_allocobject(state, sizeof(LitFunction), LITTYPE_FUNCTION, false);
    lit_init_chunk(&function->chunk);
    function->name = NULL;
    function->arg_count = 0;
    function->upvalue_count = 0;
    function->max_slots = 0;
    function->module = module;
    function->vararg = false;
    return function;
}

LitClosure* lit_create_closure(LitState* state, LitFunction* function)
{
    size_t i;
    LitClosure* closure;
    LitUpvalue** upvalues;
    closure = (LitClosure*)lit_gcmem_allocobject(state, sizeof(LitClosure), LITTYPE_CLOSURE, false);
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
    native = (LitNativeFunction*)lit_gcmem_allocobject(state, sizeof(LitNativeFunction), LITTYPE_NATIVE_FUNCTION, false);
    native->function = function;
    native->name = name;
    return native;
}

LitNativePrimFunction* lit_create_native_primitive(LitState* state, LitNativePrimitiveFn function, LitString* name)
{
    LitNativePrimFunction* native;
    native = (LitNativePrimFunction*)lit_gcmem_allocobject(state, sizeof(LitNativePrimFunction), LITTYPE_NATIVE_PRIMITIVE, false);
    native->function = function;
    native->name = name;
    return native;
}

LitNativeMethod* lit_create_native_method(LitState* state, LitNativeMethodFn method, LitString* name)
{
    LitNativeMethod* native;
    native = (LitNativeMethod*)lit_gcmem_allocobject(state, sizeof(LitNativeMethod), LITTYPE_NATIVE_METHOD, false);
    native->method = method;
    native->name = name;
    return native;
}

LitPrimitiveMethod* lit_create_primitive_method(LitState* state, LitPrimitiveMethodFn method, LitString* name)
{
    LitPrimitiveMethod* native;
    native = (LitPrimitiveMethod*)lit_gcmem_allocobject(state, sizeof(LitPrimitiveMethod), LITTYPE_PRIMITIVE_METHOD, false);
    native->method = method;
    native->name = name;
    return native;
}

LitBoundMethod* lit_create_bound_method(LitState* state, LitValue receiver, LitValue method)
{
    LitBoundMethod* bound_method;
    bound_method = (LitBoundMethod*)lit_gcmem_allocobject(state, sizeof(LitBoundMethod), LITTYPE_BOUND_METHOD, false);
    bound_method->receiver = receiver;
    bound_method->method = method;
    return bound_method;
}

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


static LitValue objfn_function_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_get_function_name(vm, instance);
}


static LitValue objfn_function_name(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_get_function_name(vm, instance);
}

void lit_open_function_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "Function");
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
        lit_class_bindconstructor(state, klass, util_invalid_constructor);
        lit_class_bindmethod(state, klass, "toString", objfn_function_tostring);
        lit_class_bindgetset(state, klass, "name", objfn_function_name, NULL, false);
        state->functionvalue_class = klass;
    }
    lit_set_global(state, klass->name, lit_value_objectvalue(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    };
}
