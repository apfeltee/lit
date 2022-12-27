
#include "lit.h"

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
    LIT_BEGIN_CLASS("Function");
    {
        LIT_INHERIT_CLASS(state->objectvalue_class);
        LIT_BIND_CONSTRUCTOR(util_invalid_constructor);
        LIT_BIND_METHOD("toString", objfn_function_tostring);
        LIT_BIND_GETTER("name", objfn_function_name);
        state->functionvalue_class = klass;
    }
    LIT_END_CLASS();
}
