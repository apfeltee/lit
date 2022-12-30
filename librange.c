
#include "lit.h"

static LitValue objfn_range_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    int number;
    LitRange* range;
    range = AS_RANGE(instance);
    number = range->from;
    (void)vm;
    (void)argc;
    if(lit_value_isnumber(argv[0]))
    {
        number = lit_value_to_number(argv[0]);
        if((range->to > range->from) ? (number >= range->to) : (number >= range->from))
        {
            return NULL_VALUE;
        }
        number += (((range->from - range->to) > 0) ? -1 : 1);
    }
    return lit_number_to_value(number);
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
    range = AS_RANGE(instance);
    return lit_value_objectvalue(lit_string_format(vm->state, "Range(#, #)", range->from, range->to));
}

static LitValue objfn_range_from(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argv;
    (void)argc;
    return lit_number_to_value(AS_RANGE(instance)->from);
}

static LitValue objfn_range_set_from(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    AS_RANGE(instance)->from = lit_value_to_number(argv[0]);
    return argv[0];
}

static LitValue objfn_range_to(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(AS_RANGE(instance)->to);
}

static LitValue objfn_range_set_to(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    AS_RANGE(instance)->to = lit_value_to_number(argv[0]);
    return argv[0];
}

static LitValue objfn_range_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LitRange* range;
    range = AS_RANGE(instance);
    return lit_number_to_value(range->to - range->from);
}

void lit_open_range_library(LitState* state)
{
    LIT_BEGIN_CLASS("Range");
    {
        LIT_INHERIT_CLASS(state->objectvalue_class);
        LIT_BIND_CONSTRUCTOR(util_invalid_constructor);
        LIT_BIND_METHOD("iterator", objfn_range_iterator);
        LIT_BIND_METHOD("iteratorValue", objfn_range_iteratorvalue);
        LIT_BIND_METHOD("toString", objfn_range_tostring);
        LIT_BIND_FIELD("from", objfn_range_from, objfn_range_set_from);
        LIT_BIND_FIELD("to", objfn_range_to, objfn_range_set_to);
        LIT_BIND_GETTER("length", objfn_range_length);
        state->rangevalue_class  = klass;
    }
    LIT_END_CLASS();
}
