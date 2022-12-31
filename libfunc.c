
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
