
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "lit.h"
#include "sds.h"

#if 0
    static LitObject g_stackmem[1024 * (1024 * 4)];
    static size_t g_objcount = 0;
#endif

LitObject* lit_allocate_object(LitState* state, size_t size, LitObjectType type, bool islight)
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
        obj = (LitObject*)lit_reallocate(state, NULL, 0, size);
        obj->mustfree = true;
    }
    obj->type = type;
    obj->marked = false;
    obj->next = state->vm->objects;
    state->vm->objects = obj;
    #ifdef LIT_LOG_ALLOCATION
        printf("%p allocate %ld for %s\n", (void*)obj, size, lit_get_value_type(type));
    #endif

    return obj;
}

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
        lit_state_raiseerror(state, RUNTIME_ERROR, "Fatal error:\nOut of memory\nProgram terminated");
        exit(111);
    }
    return ptr;
}

void lit_free_object(LitState* state, LitObject* object)
{
    LitString* string;
    LitFunction* function;
    LitFiber* fiber;
    LitModule* module;
    LitClosure* closure;
#ifdef LIT_LOG_ALLOCATION
    printf("(");
    lit_print_value(lit_value_objectvalue(object));
    printf(") %p free %s\n", (void*)object, lit_get_value_type(object->type));
#endif

    switch(object->type)
    {
        case LITTYPE_NUMBER:
            {
                LitNumber* n = (LitNumber*)object;
                if(object->mustfree)
                {
                    LIT_FREE(state, sizeof(LitNumber), object);
                }
            }
            break;
        case LITTYPE_STRING:
            {
                string = (LitString*)object;
                //LIT_FREE_ARRAY(state, sizeof(char), string->chars, string->length + 1);
                sdsfree(string->chars);
                string->chars = NULL;
                LIT_FREE(state, sizeof(LitString), object);
            }
            break;

        case LITTYPE_FUNCTION:
            {
                function = (LitFunction*)object;
                lit_free_chunk(state, &function->chunk);
                LIT_FREE(state, sizeof(LitFunction), object);
            }
            break;
        case LITTYPE_NATIVE_FUNCTION:
            {
                LIT_FREE(state, sizeof(LitNativeFunction), object);
            }
            break;
        case LITTYPE_NATIVE_PRIMITIVE:
            {
                LIT_FREE(state, sizeof(LitNativePrimFunction), object);
            }
            break;
        case LITTYPE_NATIVE_METHOD:
            {
                LIT_FREE(state, sizeof(LitNativeMethod), object);
            }
            break;
        case LITTYPE_PRIMITIVE_METHOD:
            {
                LIT_FREE(state, sizeof(LitPrimitiveMethod), object);
            }
            break;
        case LITTYPE_FIBER:
            {
                fiber = (LitFiber*)object;
                LIT_FREE_ARRAY(state, sizeof(LitCallFrame), fiber->frames, fiber->frame_capacity);
                LIT_FREE_ARRAY(state, sizeof(LitValue), fiber->stack, fiber->stack_capacity);
                LIT_FREE(state, sizeof(LitFiber), object);
            }
            break;
        case LITTYPE_MODULE:
            {
                module = (LitModule*)object;
                LIT_FREE_ARRAY(state, sizeof(LitValue), module->privates, module->private_count);
                LIT_FREE(state, sizeof(LitModule), object);
            }
            break;
        case LITTYPE_CLOSURE:
            {
                closure = (LitClosure*)object;
                LIT_FREE_ARRAY(state, sizeof(LitUpvalue*), closure->upvalues, closure->upvalue_count);
                LIT_FREE(state, sizeof(LitClosure), object);
            }
            break;
        case LITTYPE_UPVALUE:
            {
                LIT_FREE(state, sizeof(LitUpvalue), object);
            }
            break;
        case LITTYPE_CLASS:
            {
                LitClass* klass = (LitClass*)object;
                lit_table_destroy(state, &klass->methods);
                lit_table_destroy(state, &klass->static_fields);
                LIT_FREE(state, sizeof(LitClass), object);
            }
            break;

        case LITTYPE_INSTANCE:
            {
                lit_table_destroy(state, &((LitInstance*)object)->fields);
                LIT_FREE(state, sizeof(LitInstance), object);
            }
            break;
        case LITTYPE_BOUND_METHOD:
            {
                LIT_FREE(state, sizeof(LitBoundMethod), object);
            }
            break;
        case LITTYPE_ARRAY:
            {
                lit_vallist_destroy(state, &((LitArray*)object)->list);
                LIT_FREE(state, sizeof(LitArray), object);
            }
            break;
        case LITTYPE_MAP:
            {
                lit_table_destroy(state, &((LitMap*)object)->values);
                LIT_FREE(state, sizeof(LitMap), object);
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
                    if(data->canfree)
                    {
                        lit_reallocate(state, data->data, data->size, 0);
                    }
                }
                LIT_FREE(state, sizeof(LitUserdata), data);
                //free(data);
            }
            break;
        case LITTYPE_RANGE:
            {
                LIT_FREE(state, sizeof(LitRange), object);
            }
            break;
        case LITTYPE_FIELD:
            {
                LIT_FREE(state, sizeof(LitField), object);
            }
            break;
        case LITTYPE_REFERENCE:
            {
                LIT_FREE(state, sizeof(LitReference), object);
            }
            break;
        default:
            {
                fprintf(stderr, "internal error: trying to free something else!\n");
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

void lit_mark_value(LitVM* vm, LitValue value)
{
    if(lit_value_isobject(value))
    {
        lit_mark_object(vm, lit_value_asobject(value));
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
    for(i = 0; i < lit_vallist_count(array); i++)
    {
        lit_mark_value(vm, lit_vallist_get(array, i));
    }
}

static void blacken_object(LitVM* vm, LitObject* object)
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
                lit_mark_object(vm, (LitObject*)function->name);
                mark_array(vm, &function->chunk.constants);
            }
            break;
        case LITTYPE_FIBER:
            {
                fiber = (LitFiber*)object;
                for(LitValue* slot = fiber->stack; slot < fiber->stack_top; slot++)
                {
                    lit_mark_value(vm, *slot);
                }
                for(i = 0; i < fiber->frame_count; i++)
                {
                    frame = &fiber->frames[i];
                    if(frame->closure != NULL)
                    {
                        lit_mark_object(vm, (LitObject*)frame->closure);
                    }
                    else
                    {
                        lit_mark_object(vm, (LitObject*)frame->function);
                    }
                }
                for(upvalue = fiber->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
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
                module = (LitModule*)object;
                lit_mark_value(vm, module->return_value);
                lit_mark_object(vm, (LitObject*)module->name);
                lit_mark_object(vm, (LitObject*)module->main_function);
                lit_mark_object(vm, (LitObject*)module->main_fiber);
                lit_mark_object(vm, (LitObject*)module->private_names);
                for(i = 0; i < module->private_count; i++)
                {
                    lit_mark_value(vm, module->privates[i]);
                }
            }
            break;
        case LITTYPE_CLOSURE:
            {
                closure = (LitClosure*)object;
                lit_mark_object(vm, (LitObject*)closure->function);
                // Check for NULL is needed for a really specific gc-case
                if(closure->upvalues != NULL)
                {
                    for(i = 0; i < closure->upvalue_count; i++)
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
                klass = (LitClass*)object;
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
                bound_method = (LitBoundMethod*)object;
                lit_mark_value(vm, bound_method->receiver);
                lit_mark_value(vm, bound_method->method);
            }
            break;
        case LITTYPE_ARRAY:
            {
                mark_array(vm, &((LitArray*)object)->list);
            }
            break;
        case LITTYPE_MAP:
            {
                lit_mark_table(vm, &((LitMap*)object)->values);
            }
            break;
        case LITTYPE_FIELD:
            {
                field = (LitField*)object;
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
                fprintf(stderr, "internal error: trying to blacken something else!\n");
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

