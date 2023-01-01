
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "lit.h"

#if 0
    static LitObject g_stackmem[1024 * (1024 * 4)];
    static size_t g_objcount = 0;
#endif

LitObject* lit_gcmem_allocobject(LitState* state, size_t size, LitObjType type, bool islight)
{
    LitObject* obj;
    if(islight)
    {
        #if 0
            obj = &g_stackmem[g_objcount];
            g_objcount++;
        #else
            LitObject stackobj[1];
            obj = stackobj;
        #endif
        obj->mustfree = false;
    }
    else
    {
        obj = (LitObject*)lit_gcmem_memrealloc(state, NULL, 0, size);
        obj->mustfree = true;
    }
    obj->type = type;
    obj->marked = false;
    obj->next = state->vm->objects;
    state->vm->objects = obj;
    #ifdef LIT_LOG_ALLOCATION
        printf("%p allocate %ld for %s\n", (void*)obj, size, lit_value_typename(type));
    #endif

    return obj;
}

void* lit_gcmem_memrealloc(LitState* state, void* pointer, size_t old_size, size_t new_size)
{
    void* ptr;
    state->bytes_allocated += (int64_t)new_size - (int64_t)old_size;
    if(new_size > old_size)
    {
#ifdef LIT_STRESS_TEST_GC
        lit_gcmem_collectgarbage(state->vm);
#endif
        if(state->bytes_allocated > state->next_gc)
        {
            lit_gcmem_collectgarbage(state->vm);
        }
    }
    if(new_size == 0)
    {
        free(pointer);
        return NULL;
    }
    ptr = (void*)realloc(pointer, new_size);
    if(ptr == NULL)
    {
        lit_state_raiseerror(state, RUNTIME_ERROR, "Fatal error:\nOut of memory\nProgram terminated");
        exit(111);
    }
    return ptr;
}

void lit_gcmem_marktable(LitVM* vm, LitTable* table)
{
    int i;
    LitTableEntry* entry;
    for(i = 0; i <= table->capacity; i++)
    {
        entry = &table->entries[i];
        lit_gcmem_markobject(vm, (LitObject*)entry->key);
        lit_gcmem_markvalue(vm, entry->value);
    }
}

void lit_gcmem_markobject(LitVM* vm, LitObject* object)
{
    if(object == NULL || object->marked)
    {
        return;
    }

    object->marked = true;

#ifdef LIT_LOG_MARKING
    printf("%p mark ", (void*)object);
    lit_print_value(lit_value_objectvalue(object));
    printf("\n");
#endif

    if(vm->gray_capacity < vm->gray_count + 1)
    {
        vm->gray_capacity = LIT_GROW_CAPACITY(vm->gray_capacity);
        vm->gray_stack = (LitObject**)realloc(vm->gray_stack, sizeof(LitObject*) * vm->gray_capacity);
    }

    vm->gray_stack[vm->gray_count++] = object;
}

void lit_gcmem_markvalue(LitVM* vm, LitValue value)
{
    if(lit_value_isobject(value))
    {
        lit_gcmem_markobject(vm, lit_value_asobject(value));
    }
}

void lit_gcmem_vmmarkroots(LitVM* vm)
{
    size_t i;
    LitState* state;
    state = vm->state;
    for(i = 0; i < state->root_count; i++)
    {
        lit_gcmem_markvalue(vm, state->roots[i]);
    }
    lit_gcmem_markobject(vm, (LitObject*)vm->fiber);
    lit_gcmem_markobject(vm, (LitObject*)state->classvalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->objectvalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->numbervalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->stringvalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->boolvalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->functionvalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->fibervalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->modulevalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->arrayvalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->mapvalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->rangevalue_class);
    lit_gcmem_markobject(vm, (LitObject*)state->api_name);
    lit_gcmem_markobject(vm, (LitObject*)state->api_function);
    lit_gcmem_markobject(vm, (LitObject*)state->api_fiber);
    lit_gcmem_marktable(vm, &state->preprocessor->defined);
    lit_gcmem_marktable(vm, &vm->modules->values);
    lit_gcmem_marktable(vm, &vm->globals->values);
}

void lit_gcmem_markarray(LitVM* vm, LitValueList* array)
{
    size_t i;
    for(i = 0; i < lit_vallist_count(array); i++)
    {
        lit_gcmem_markvalue(vm, lit_vallist_get(array, i));
    }
}

void lit_gcmem_vmblackobject(LitVM* vm, LitObject* object)
{
    size_t i;
    LitUserdata* data;
    LitFunction* function;
    LitFiber* fiber;
    LitUpvalue* upvalue;
    LitCallFrame* frame;
    LitModule* module;
    LitClosure* closure;
    LitClass* klass;
    LitBoundMethod* bound_method;
    LitField* field;

#ifdef LIT_LOG_BLACKING
    printf("%p blacken ", (void*)object);
    lit_print_value(lit_value_objectvalue(object));
    printf("\n");
#endif
    switch(object->type)
    {
        case LITTYPE_NATIVE_FUNCTION:
        case LITTYPE_NATIVE_PRIMITIVE:
        case LITTYPE_NATIVE_METHOD:
        case LITTYPE_PRIMITIVE_METHOD:
        case LITTYPE_RANGE:
        case LITTYPE_STRING:
        case LITTYPE_NUMBER:
            {
            }
            break;
        case LITTYPE_USERDATA:
            {
                data = (LitUserdata*)object;
                if(data->cleanup_fn != NULL)
                {
                    data->cleanup_fn(vm->state, data, true);
                }
            }
            break;
        case LITTYPE_FUNCTION:
            {
                function = (LitFunction*)object;
                lit_gcmem_markobject(vm, (LitObject*)function->name);
                lit_gcmem_markarray(vm, &function->chunk.constants);
            }
            break;
        case LITTYPE_FIBER:
            {
                fiber = (LitFiber*)object;
                for(LitValue* slot = fiber->stack; slot < fiber->stack_top; slot++)
                {
                    lit_gcmem_markvalue(vm, *slot);
                }
                for(i = 0; i < fiber->frame_count; i++)
                {
                    frame = &fiber->frames[i];
                    if(frame->closure != NULL)
                    {
                        lit_gcmem_markobject(vm, (LitObject*)frame->closure);
                    }
                    else
                    {
                        lit_gcmem_markobject(vm, (LitObject*)frame->function);
                    }
                }
                for(upvalue = fiber->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
                {
                    lit_gcmem_markobject(vm, (LitObject*)upvalue);
                }
                lit_gcmem_markvalue(vm, fiber->error);
                lit_gcmem_markobject(vm, (LitObject*)fiber->module);
                lit_gcmem_markobject(vm, (LitObject*)fiber->parent);
            }
            break;
        case LITTYPE_MODULE:
            {
                module = (LitModule*)object;
                lit_gcmem_markvalue(vm, module->return_value);
                lit_gcmem_markobject(vm, (LitObject*)module->name);
                lit_gcmem_markobject(vm, (LitObject*)module->main_function);
                lit_gcmem_markobject(vm, (LitObject*)module->main_fiber);
                lit_gcmem_markobject(vm, (LitObject*)module->private_names);
                for(i = 0; i < module->private_count; i++)
                {
                    lit_gcmem_markvalue(vm, module->privates[i]);
                }
            }
            break;
        case LITTYPE_CLOSURE:
            {
                closure = (LitClosure*)object;
                lit_gcmem_markobject(vm, (LitObject*)closure->function);
                // Check for NULL is needed for a really specific gc-case
                if(closure->upvalues != NULL)
                {
                    for(i = 0; i < closure->upvalue_count; i++)
                    {
                        lit_gcmem_markobject(vm, (LitObject*)closure->upvalues[i]);
                    }
                }
            }
            break;
        case LITTYPE_UPVALUE:
            {
                lit_gcmem_markvalue(vm, ((LitUpvalue*)object)->closed);
            }
            break;
        case LITTYPE_CLASS:
            {
                klass = (LitClass*)object;
                lit_gcmem_markobject(vm, (LitObject*)klass->name);
                lit_gcmem_markobject(vm, (LitObject*)klass->super);
                lit_gcmem_marktable(vm, &klass->methods);
                lit_gcmem_marktable(vm, &klass->static_fields);
            }
            break;
        case LITTYPE_INSTANCE:
            {
                LitInstance* instance = (LitInstance*)object;
                lit_gcmem_markobject(vm, (LitObject*)instance->klass);
                lit_gcmem_marktable(vm, &instance->fields);
            }
            break;
        case LITTYPE_BOUND_METHOD:
            {
                bound_method = (LitBoundMethod*)object;
                lit_gcmem_markvalue(vm, bound_method->receiver);
                lit_gcmem_markvalue(vm, bound_method->method);
            }
            break;
        case LITTYPE_ARRAY:
            {
                lit_gcmem_markarray(vm, &((LitArray*)object)->list);
            }
            break;
        case LITTYPE_MAP:
            {
                lit_gcmem_marktable(vm, &((LitMap*)object)->values);
            }
            break;
        case LITTYPE_FIELD:
            {
                field = (LitField*)object;
                lit_gcmem_markobject(vm, (LitObject*)field->getter);
                lit_gcmem_markobject(vm, (LitObject*)field->setter);
            }
            break;
        case LITTYPE_REFERENCE:
            {
                lit_gcmem_markvalue(vm, *((LitReference*)object)->slot);
            }
            break;
        default:
            {
                fprintf(stderr, "internal error: trying to blacken something else!\n");
                UNREACHABLE
            }
            break;
    }
}

void lit_gcmem_vmtracerefs(LitVM* vm)
{
    LitObject* object;
    while(vm->gray_count > 0)
    {
        object = vm->gray_stack[--vm->gray_count];
        lit_gcmem_vmblackobject(vm, object);
    }
}

void lit_gcmem_vmsweep(LitVM* vm)
{
    LitObject* unreached;
    LitObject* previous;
    LitObject* object;
    previous = NULL;
    object = vm->objects;
    while(object != NULL)
    {
        if(object->marked)
        {
            object->marked = false;
            previous = object;
            object = object->next;
        }
        else
        {
            unreached = object;
            object = object->next;
            if(previous != NULL)
            {
                previous->next = object;
            }
            else
            {
                vm->objects = object;
            }
            lit_object_destroy(vm->state, unreached);
        }
    }
}

uint64_t lit_gcmem_collectgarbage(LitVM* vm)
{
    clock_t t;
    uint64_t before;
    uint64_t collected;
    (void)t;
    if(!vm->state->allow_gc)
    {
        return 0;
    }

    vm->state->allow_gc = false;
    before = vm->state->bytes_allocated;

#ifdef LIT_LOG_GC
    printf("-- gc begin\n");
    t = clock();
#endif

    lit_gcmem_vmmarkroots(vm);
    lit_gcmem_vmtracerefs(vm);
    lit_table_removewhite(&vm->strings);
    lit_gcmem_vmsweep(vm);
    vm->state->next_gc = vm->state->bytes_allocated * LIT_GC_HEAP_GROW_FACTOR;
    vm->state->allow_gc = true;
    collected = before - vm->state->bytes_allocated;

#ifdef LIT_LOG_GC
    printf("-- gc end. Collected %imb in %gms\n", ((int)((collected / 1024.0 + 0.5) / 10)) * 10,
           (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
#endif
    return collected;
}

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
    collected = lit_gcmem_collectgarbage(vm);
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


