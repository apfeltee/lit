
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "lit.h"
#include "sds.h"


void* lit_reallocate(LitState* state, void* pointer, size_t old_size, size_t new_size)
{
    void* ptr;
    state->bytes_allocated += (int64_t)new_size - (int64_t)old_size;
    if(new_size > old_size)
    {
#ifdef LIT_STRESS_TEST_GC
        lit_collect_garbage(state->vm);
#endif
        if(state->bytes_allocated > state->next_gc)
        {
            lit_collect_garbage(state->vm);
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
        lit_error(state, RUNTIME_ERROR, "Fatal error:\nOut of memory\nProgram terminated");
        exit(111);
    }
    return ptr;
}

void lit_free_object(LitState* state, LitObject* object)
{
#ifdef LIT_LOG_ALLOCATION
    printf("(");
    lit_print_value(OBJECT_VALUE(object));
    printf(") %p free %s\n", (void*)object, lit_get_value_type(object->type));
#endif

    switch(object->type)
    {
        case LITTYPE_STRING:
            {
                LitString* string = (LitString*)object;
                //LIT_FREE_ARRAY(state, char, string->chars, string->length + 1);
                sdsfree(string->chars);
                string->chars = NULL;
                LIT_FREE(state, LitString, object);
            }
            break;

        case LITTYPE_FUNCTION:
            {
                LitFunction* function = (LitFunction*)object;
                lit_free_chunk(state, &function->chunk);
                LIT_FREE(state, LitFunction, object);
            }
            break;
        case LITTYPE_NATIVE_FUNCTION:
            {
                LIT_FREE(state, LitNativeFunction, object);
            }
            break;
        case LITTYPE_NATIVE_PRIMITIVE:
            {
                LIT_FREE(state, LitNativePrimitive, object);
            }
            break;
        case LITTYPE_NATIVE_METHOD:
            {
                LIT_FREE(state, LitNativeMethod, object);
            }
            break;
        case LITTYPE_PRIMITIVE_METHOD:
            {
                LIT_FREE(state, LitPrimitiveMethod, object);
            }
            break;
        case LITTYPE_FIBER:
            {
                LitFiber* fiber = (LitFiber*)object;
                LIT_FREE_ARRAY(state, LitCallFrame, fiber->frames, fiber->frame_capacity);
                LIT_FREE_ARRAY(state, LitValue, fiber->stack, fiber->stack_capacity);
                LIT_FREE(state, LitFiber, object);
            }
            break;
        case LITTYPE_MODULE:
            {
                LitModule* module = (LitModule*)object;
                LIT_FREE_ARRAY(state, LitValue, module->privates, module->private_count);
                LIT_FREE(state, LitModule, object);
            }
            break;
        case LITTYPE_CLOSURE:
            {
                LitClosure* closure = (LitClosure*)object;
                LIT_FREE_ARRAY(state, LitUpvalue*, closure->upvalues, closure->upvalue_count);
                LIT_FREE(state, LitClosure, object);
            }
            break;
        case LITTYPE_UPVALUE:
            {
                LIT_FREE(state, LitUpvalue, object);
            }
            break;
        case LITTYPE_CLASS:
            {
                LitClass* klass = (LitClass*)object;
                lit_free_table(state, &klass->methods);
                lit_free_table(state, &klass->static_fields);
                LIT_FREE(state, LitClass, object);
            }
            break;

        case LITTYPE_INSTANCE:
            {
                lit_free_table(state, &((LitInstance*)object)->fields);
                LIT_FREE(state, LitInstance, object);
            }
            break;
        case LITTYPE_BOUND_METHOD:
            {
                LIT_FREE(state, LitBoundMethod, object);
            }
            break;
        case LITTYPE_ARRAY:
            {
                lit_free_values(state, &((LitArray*)object)->values);
                LIT_FREE(state, LitArray, object);
            }
            break;
        case LITTYPE_MAP:
            {
                lit_free_table(state, &((LitMap*)object)->values);
                LIT_FREE(state, LitMap, object);
            }
            break;
        case LITTYPE_USERDATA:
            {
                LitUserdata* data = (LitUserdata*)object;
                if(data->cleanup_fn != NULL)
                {
                    data->cleanup_fn(state, data, false);
                }
                if(data->size > 0)
                {
                    lit_reallocate(state, data->data, data->size, 0);
                }
                LIT_FREE(state, LitUserdata, object);
            }
            break;
        case LITTYPE_RANGE:
            {
                LIT_FREE(state, LitRange, object);
            }
            break;
        case LITTYPE_FIELD:
            {
                LIT_FREE(state, LitField, object);
            }
            break;
        case LITTYPE_REFERENCE:
            {
                LIT_FREE(state, LitReference, object);
            }
            break;
        default:
            {
                UNREACHABLE
            }
            break;
    }
}

void lit_free_objects(LitState* state, LitObject* objects)
{
    LitObject* object = objects;

    while(object != NULL)
    {
        LitObject* next = object->next;
        lit_free_object(state, object);
        object = next;
    }

    free(state->vm->gray_stack);
    state->vm->gray_capacity = 0;
}

void lit_mark_object(LitVM* vm, LitObject* object)
{
    if(object == NULL || object->marked)
    {
        return;
    }

    object->marked = true;

#ifdef LIT_LOG_MARKING
    printf("%p mark ", (void*)object);
    lit_print_value(OBJECT_VALUE(object));
    printf("\n");
#endif

    if(vm->gray_capacity < vm->gray_count + 1)
    {
        vm->gray_capacity = LIT_GROW_CAPACITY(vm->gray_capacity);
        vm->gray_stack = (LitObject**)realloc(vm->gray_stack, sizeof(LitObject*) * vm->gray_capacity);
    }

    vm->gray_stack[vm->gray_count++] = object;
}

void lit_mark_value(LitVM* vm, LitValue value)
{
    if(IS_OBJECT(value))
    {
        lit_mark_object(vm, AS_OBJECT(value));
    }
}

static void mark_roots(LitVM* vm)
{
    size_t i;
    LitState* state;
    state = vm->state;
    for(i = 0; i < state->root_count; i++)
    {
        lit_mark_value(vm, state->roots[i]);
    }
    lit_mark_object(vm, (LitObject*)vm->fiber);
    lit_mark_object(vm, (LitObject*)state->classvalue_class);
    lit_mark_object(vm, (LitObject*)state->objectvalue_class);
    lit_mark_object(vm, (LitObject*)state->numbervalue_class);
    lit_mark_object(vm, (LitObject*)state->stringvalue_class);
    lit_mark_object(vm, (LitObject*)state->boolvalue_class);
    lit_mark_object(vm, (LitObject*)state->functionvalue_class);
    lit_mark_object(vm, (LitObject*)state->fibervalue_class);
    lit_mark_object(vm, (LitObject*)state->modulevalue_class);
    lit_mark_object(vm, (LitObject*)state->arrayvalue_class);
    lit_mark_object(vm, (LitObject*)state->mapvalue_class);
    lit_mark_object(vm, (LitObject*)state->rangevalue_class);
    lit_mark_object(vm, (LitObject*)state->api_name);
    lit_mark_object(vm, (LitObject*)state->api_function);
    lit_mark_object(vm, (LitObject*)state->api_fiber);
    lit_mark_table(vm, &state->preprocessor->defined);
    lit_mark_table(vm, &vm->modules->values);
    lit_mark_table(vm, &vm->globals->values);
}

static void mark_array(LitVM* vm, LitValueList* array)
{
    size_t i;
    for(i = 0; i < array->count; i++)
    {
        lit_mark_value(vm, array->values[i]);
    }
}

static void blacken_object(LitVM* vm, LitObject* object)
{
#ifdef LIT_LOG_BLACKING
    printf("%p blacken ", (void*)object);
    lit_print_value(OBJECT_VALUE(object));
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
            {
            }
            break;
        case LITTYPE_USERDATA:
            {
                LitUserdata* data = (LitUserdata*)object;
                if(data->cleanup_fn != NULL)
                {
                    data->cleanup_fn(vm->state, data, true);
                }
            }
            break;
        case LITTYPE_FUNCTION:
            {
                LitFunction* function = (LitFunction*)object;
                lit_mark_object(vm, (LitObject*)function->name);
                mark_array(vm, &function->chunk.constants);
            }
            break;
        case LITTYPE_FIBER:
            {
                LitFiber* fiber = (LitFiber*)object;
                for(LitValue* slot = fiber->stack; slot < fiber->stack_top; slot++)
                {
                    lit_mark_value(vm, *slot);
                }
                for(size_t i = 0; i < fiber->frame_count; i++)
                {
                    LitCallFrame* frame = &fiber->frames[i];
                    if(frame->closure != NULL)
                    {
                        lit_mark_object(vm, (LitObject*)frame->closure);
                    }
                    else
                    {
                        lit_mark_object(vm, (LitObject*)frame->function);
                    }
                }
                for(LitUpvalue* upvalue = fiber->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
                {
                    lit_mark_object(vm, (LitObject*)upvalue);
                }
                lit_mark_value(vm, fiber->error);
                lit_mark_object(vm, (LitObject*)fiber->module);
                lit_mark_object(vm, (LitObject*)fiber->parent);
            }
            break;
        case LITTYPE_MODULE:
            {
                LitModule* module = (LitModule*)object;
                lit_mark_value(vm, module->return_value);
                lit_mark_object(vm, (LitObject*)module->name);
                lit_mark_object(vm, (LitObject*)module->main_function);
                lit_mark_object(vm, (LitObject*)module->main_fiber);
                lit_mark_object(vm, (LitObject*)module->private_names);
                for(size_t i = 0; i < module->private_count; i++)
                {
                    lit_mark_value(vm, module->privates[i]);
                }
            }
            break;
        case LITTYPE_CLOSURE:
            {
                LitClosure* closure = (LitClosure*)object;
                lit_mark_object(vm, (LitObject*)closure->function);
                // Check for NULL is needed for a really specific gc-case
                if(closure->upvalues != NULL)
                {
                    for(size_t i = 0; i < closure->upvalue_count; i++)
                    {
                        lit_mark_object(vm, (LitObject*)closure->upvalues[i]);
                    }
                }
            }
            break;
        case LITTYPE_UPVALUE:
            {
                lit_mark_value(vm, ((LitUpvalue*)object)->closed);
            }
            break;
        case LITTYPE_CLASS:
            {
                LitClass* klass = (LitClass*)object;
                lit_mark_object(vm, (LitObject*)klass->name);
                lit_mark_object(vm, (LitObject*)klass->super);
                lit_mark_table(vm, &klass->methods);
                lit_mark_table(vm, &klass->static_fields);
            }
            break;
        case LITTYPE_INSTANCE:
            {
                LitInstance* instance = (LitInstance*)object;
                lit_mark_object(vm, (LitObject*)instance->klass);
                lit_mark_table(vm, &instance->fields);
            }
            break;
        case LITTYPE_BOUND_METHOD:
            {
                LitBoundMethod* bound_method = (LitBoundMethod*)object;
                lit_mark_value(vm, bound_method->receiver);
                lit_mark_value(vm, bound_method->method);
            }
            break;
        case LITTYPE_ARRAY:
            {
                mark_array(vm, &((LitArray*)object)->values);
            }
            break;
        case LITTYPE_MAP:
            {
                lit_mark_table(vm, &((LitMap*)object)->values);
            }
            break;
        case LITTYPE_FIELD:
            {
                LitField* field = (LitField*)object;
                lit_mark_object(vm, (LitObject*)field->getter);
                lit_mark_object(vm, (LitObject*)field->setter);
            }
            break;
        case LITTYPE_REFERENCE:
            {
                lit_mark_value(vm, *((LitReference*)object)->slot);
            }
            break;
        default:
            {
                UNREACHABLE
            }
            break;
    }
}

static void trace_references(LitVM* vm)
{
    LitObject* object;
    while(vm->gray_count > 0)
    {
        object = vm->gray_stack[--vm->gray_count];
        blacken_object(vm, object);
    }
}

static void sweep(LitVM* vm)
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
            lit_free_object(vm->state, unreached);
        }
    }
}

uint64_t lit_collect_garbage(LitVM* vm)
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

    mark_roots(vm);
    trace_references(vm);
    lit_table_remove_white(&vm->strings);
    sweep(vm);
    vm->state->next_gc = vm->state->bytes_allocated * LIT_GC_HEAP_GROW_FACTOR;
    vm->state->allow_gc = true;
    collected = before - vm->state->bytes_allocated;

#ifdef LIT_LOG_GC
    printf("-- gc end. Collected %imb in %gms\n", ((int)((collected / 1024.0 + 0.5) / 10)) * 10,
           (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
#endif
    return collected;
}

// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
int lit_closest_power_of_two(int n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

