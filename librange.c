
#include "lit.h"

static LitValue objfn_range_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    int number;
    LitRange* range;
    range = lit_value_asrange(instance);
    number = range->from;
    (void)vm;
    (void)argc;
    if(lit_value_isnumber(argv[0]))
    {
        number = lit_value_asnumber(argv[0]);
        if((range->to > range->from) ? (number >= range->to) : (number >= range->from))
        {
            return NULL_VALUE;
        }
        number += (((range->from - range->to) > 0) ? -1 : 1);
    }
    return lit_value_numbertovalue(vm->state, number);
}

static LitValue objfn_range_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    (void)vm;
    (void)instance;
    return argv[0];
}

static LitValue objfn_range_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitRange* range;
    range = lit_value_asrange(instance);
    return lit_value_objectvalue(lit_string_format(vm->state, "Range(#, #)", range->from, range->to));
}

static LitValue objfn_range_from(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argv;
    (void)argc;
    return lit_value_numbertovalue(vm->state, lit_value_asrange(instance)->from);
}

static LitValue objfn_range_set_from(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    lit_value_asrange(instance)->from = lit_value_asnumber(argv[0]);
    return argv[0];
}

static LitValue objfn_range_to(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_numbertovalue(vm->state, lit_value_asrange(instance)->to);
}

static LitValue objfn_range_set_to(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    lit_value_asrange(instance)->to = lit_value_asnumber(argv[0]);
    return argv[0];
}

static LitValue objfn_range_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LitRange* range;
    range = lit_value_asrange(instance);
    return lit_value_numbertovalue(vm->state, range->to - range->from);
}

void lit_open_range_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "Range");
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
        lit_class_bindconstructor(state, klass, util_invalid_constructor);
        lit_class_bindmethod(state, klass, "iterator", objfn_range_iterator);
        lit_class_bindmethod(state, klass, "iteratorValue", objfn_range_iteratorvalue);
        lit_class_bindmethod(state, klass, "toString", objfn_range_tostring);
        lit_class_bindgetset(state, klass, "from", objfn_range_from, objfn_range_set_from, false);
        lit_class_bindgetset(state, klass, "to", objfn_range_to, objfn_range_set_to, false);
        lit_class_bindgetset(state, klass, "length", objfn_range_length, NULL, false);
        state->rangevalue_class = klass;
    }
    lit_set_global(state, klass->name, lit_value_objectvalue(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    };
}

