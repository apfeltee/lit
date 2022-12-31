
#include "lit.h"

static LitValue objfn_fiber_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    if(argc < 1 || !lit_value_isfunction(argv[0]))
    {
        lit_runtime_error_exiting(vm, "Fiber constructor expects a function as its argument");
    }

    LitFunction* function = lit_value_asfunction(argv[0]);
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
    return lit_bool_to_value(vm->state, util_is_fiber_done(lit_value_asfiber(instance)));
}


static LitValue objfn_fiber_error(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_asfiber(instance)->error;
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
    util_run_fiber(vm, lit_value_asfiber(instance), argv, argc, false);
    return true;
}


static bool objfn_fiber_try(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    util_run_fiber(vm, lit_value_asfiber(instance), argv, argc, true);
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
    LitClass* klass;
    klass = lit_create_classobject(state, "Fiber");
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
        lit_class_bindconstructor(state, klass, objfn_fiber_constructor);
        lit_class_bindmethod(state, klass, "toString", objfn_fiber_tostring);
        lit_class_bindprimitive(state, klass, "run", objfn_fiber_run);
        lit_class_bindprimitive(state, klass, "try", objfn_fiber_try);
        lit_class_bindgetset(state, klass, "done", objfn_fiber_done, NULL, false);
        lit_class_bindgetset(state, klass, "error", objfn_fiber_error, NULL, false);
        lit_class_bindstaticprimitive(state, klass, "yield", objfn_fiber_yield);
        lit_class_bindstaticprimitive(state, klass, "yeet", objfn_fiber_yeet);
        lit_class_bindstaticprimitive(state, klass, "abort", objfn_fiber_abort);
        lit_class_bindgetset(state, klass, "current", objfn_fiber_current, NULL, true);
        state->fibervalue_class = klass;
    }
    lit_set_global(state, klass->name, lit_value_objectvalue(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    };
}
