
#include "lit.h"

static LitValue gc_memory_used(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(vm->state->bytes_allocated);
}

static LitValue gc_next_round(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    return NUMBER_VALUE(vm->state->next_gc);
}

static LitValue gc_trigger(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args)
{
    vm->state->allow_gc = true;
    int64_t collected = lit_collect_garbage(vm);
    vm->state->allow_gc = false;

    return NUMBER_VALUE(collected);
}

void lit_open_gc_library(LitState* state)
{
    LIT_BEGIN_CLASS("GC")
    LIT_BIND_STATIC_GETTER("memoryUsed", gc_memory_used)
    LIT_BIND_STATIC_GETTER("nextRound", gc_next_round)

    LIT_BIND_STATIC_METHOD("trigger", gc_trigger)
    LIT_END_CLASS()
}