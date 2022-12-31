
#include "lit.h"

static LitValue objfn_gc_memory_used(LitVM* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    (void)instance;
    (void)arg_count;
    (void)args;
    return lit_value_numbertovalue(vm->state, vm->state->bytes_allocated);
}

static LitValue objfn_gc_next_round(LitVM* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    (void)instance;
    (void)arg_count;
    (void)args;
    return lit_value_numbertovalue(vm->state, vm->state->next_gc);
}

static LitValue objfn_gc_trigger(LitVM* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    (void)instance;
    (void)arg_count;
    (void)args;
    int64_t collected;
    vm->state->allow_gc = true;
    collected = lit_collect_garbage(vm);
    vm->state->allow_gc = false;

    return lit_value_numbertovalue(vm->state, collected);
}

void lit_open_gc_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "GC");
    {
        lit_class_bindgetset(state, klass, "memoryUsed", objfn_gc_memory_used, NULL, true);
        lit_class_bindgetset(state, klass, "nextRound", objfn_gc_next_round, NULL, true);
        lit_class_bindstaticmethod(state, klass, "trigger", objfn_gc_trigger);
    }
    lit_set_global(state, klass->name, lit_value_objectvalue(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    };
}


