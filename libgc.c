
#include "lit.h"

static LitValue objfn_gc_memory_used(LitVM* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    (void)instance;
    (void)arg_count;
    (void)args;
    return lit_number_to_value(vm->state->bytes_allocated);
}

static LitValue objfn_gc_next_round(LitVM* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    (void)instance;
    (void)arg_count;
    (void)args;
    return lit_number_to_value(vm->state->next_gc);
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

    return lit_number_to_value(collected);
}

void lit_open_gc_library(LitState* state)
{
    LIT_BEGIN_CLASS("GC");
    {
        LIT_BIND_STATIC_GETTER("memoryUsed", objfn_gc_memory_used);
        LIT_BIND_STATIC_GETTER("nextRound", objfn_gc_next_round);
        LIT_BIND_STATIC_METHOD("trigger", objfn_gc_trigger);
    }
    LIT_END_CLASS();
}

