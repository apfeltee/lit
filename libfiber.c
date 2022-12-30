
#include "lit.h"

static LitValue objfn_fiber_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    if(argc < 1 || !lit_value_isfunction(argv[0]))
    {
        lit_runtime_error_exiting(vm, "Fiber constructor expects a function as its argument");
    }

    LitFunction* function = AS_FUNCTION(argv[0]);
    LitModule* module = vm->fiber->module;
    LitFiber* fiber = lit_create_fiber(vm->state, module, function);

    fiber->parent = vm->fiber;

    return lit_value_objectvalue(fiber);
}


static LitValue objfn_fiber_done(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_boolvalue(util_is_fiber_done(AS_FIBER(instance)));
}


static LitValue objfn_fiber_error(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return AS_FIBER(instance)->error;
}


static LitValue objfn_fiber_current(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(vm->fiber);
}


static bool objfn_fiber_run(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    util_run_fiber(vm, AS_FIBER(instance), argv, argc, false);
    return true;
}


static bool objfn_fiber_try(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    util_run_fiber(vm, AS_FIBER(instance), argv, argc, true);
    return true;
}


static bool objfn_fiber_yield(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    if(vm->fiber->parent == NULL)
    {
        lit_handle_runtime_error(vm, argc == 0 ? CONST_STRING(vm->state, "Fiber was yielded") :
        lit_to_string(vm->state, argv[0]));
        return true;
    }

    LitFiber* fiber = vm->fiber;

    vm->fiber = vm->fiber->parent;
    vm->fiber->stack_top -= fiber->arg_count;
    vm->fiber->stack_top[-1] = argc == 0 ? NULL_VALUE : lit_value_objectvalue(lit_to_string(vm->state, argv[0]));

    argv[-1] = NULL_VALUE;
    return true;
}


static bool objfn_fiber_yeet(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    if(vm->fiber->parent == NULL)
    {
        lit_handle_runtime_error(vm, argc == 0 ? CONST_STRING(vm->state, "Fiber was yeeted") :
        lit_to_string(vm->state, argv[0]));
        return true;
    }

    LitFiber* fiber = vm->fiber;

    vm->fiber = vm->fiber->parent;
    vm->fiber->stack_top -= fiber->arg_count;
    vm->fiber->stack_top[-1] = argc == 0 ? NULL_VALUE : lit_value_objectvalue(lit_to_string(vm->state, argv[0]));

    argv[-1] = NULL_VALUE;
    return true;
}


static bool objfn_fiber_abort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    lit_handle_runtime_error(vm, argc == 0 ? CONST_STRING(vm->state, "Fiber was aborted") :
    lit_to_string(vm->state, argv[0]));
    argv[-1] = NULL_VALUE;
    return true;
}

static LitValue objfn_fiber_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(lit_string_format(vm->state, "fiber@%p", &instance));

}

void lit_open_fiber_library(LitState* state)
{
    LIT_BEGIN_CLASS("Fiber");
    {
        LIT_INHERIT_CLASS(state->objectvalue_class);
        LIT_BIND_CONSTRUCTOR(objfn_fiber_constructor);
        LIT_BIND_METHOD("toString", objfn_fiber_tostring);
        LIT_BIND_PRIMITIVE("run", objfn_fiber_run);
        LIT_BIND_PRIMITIVE("try", objfn_fiber_try);
        LIT_BIND_GETTER("done", objfn_fiber_done);
        LIT_BIND_GETTER("error", objfn_fiber_error);
        LIT_BIND_STATIC_PRIMITIVE("yield", objfn_fiber_yield);
        LIT_BIND_STATIC_PRIMITIVE("yeet", objfn_fiber_yeet);
        LIT_BIND_STATIC_PRIMITIVE("abort", objfn_fiber_abort);
        LIT_BIND_STATIC_GETTER("current", objfn_fiber_current);
        state->fibervalue_class = klass;
    }
    LIT_END_CLASS();
}

