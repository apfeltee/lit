
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include "lit.h"

#ifdef LIT_TRACE_EXECUTION
    #define TRACE_FRAME() lit_trace_frame(fiber);
#else
    #define TRACE_FRAME() \
        do \
        { \
        } while(0);
#endif

#define vm_pushgc(state, allow) \
    bool was_allowed = state->allow_gc; \
    state->allow_gc = allow;

#define vm_popgc(state) \
    state->allow_gc = was_allowed;

#define vm_push(value) \
    (*fiber->stack_top++ = value)

#define vm_pop() \
    (*(--fiber->stack_top))

#define vm_drop() \
    (fiber->stack_top--)

#define vm_dropn(amount) \
    (fiber->stack_top -= amount)

#define vm_readbyte() \
    (*ip++)

#define vm_readshort() \
    (ip += 2u, (uint16_t)((ip[-2] << 8u) | ip[-1]))

#define op_case(name) \
    OP_##name:

#define vm_readconstant() \
    (current_chunk->constants.values[vm_readbyte()])

#define vm_readconstantlong() \
    (current_chunk->constants.values[vm_readshort()])

#define vm_readstring() \
    AS_STRING(vm_readconstant())

#define vm_readstringlong() \
    AS_STRING(vm_readconstantlong())

#define vm_peek(distance) \
    fiber->stack_top[-1 - distance]

#define vm_readframe() \
    frame = &fiber->frames[fiber->frame_count - 1]; \
    current_chunk = &frame->function->chunk; \
    ip = frame->ip; \
    slots = frame->slots; \
    fiber->module = frame->function->module; \
    privates = fiber->module->privates; \
    upvalues = frame->closure == NULL ? NULL : frame->closure->upvalues;

#define vm_writeframe() \
    frame->ip = ip;

#define vm_returnerror() \
    vm_popgc(state); \
    return (LitInterpretResult){ INTERPRET_RUNTIME_ERROR, NULL_VALUE };

#define vm_recoverstate() \
    vm_writeframe(); \
    fiber = vm->fiber; \
    if(fiber == NULL) \
    { \
        return (LitInterpretResult){ INTERPRET_OK, vm_pop() }; \
    } \
    if(fiber->abort) \
    { \
        vm_returnerror(); \
    } \
    vm_readframe(); \
    TRACE_FRAME();

#define vm_callvalue(callee, arg_count) \
    if(call_value(vm, callee, arg_count)) \
    { \
        vm_recoverstate(); \
    }

#define vm_rterror(format) \
    if(lit_runtime_error(vm, format)) \
    { \
        vm_recoverstate(); \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }

#define vm_rterrorvarg(format, ...) \
    if(lit_runtime_error(vm, format, __VA_ARGS__)) \
    { \
        vm_recoverstate();  \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }

#define vm_invoke_from_class_advanced(zklass, method_name, arg_count, error, stat, ignoring, callee) \
    LitValue method; \
    if((IS_INSTANCE(callee) && (lit_table_get(&AS_INSTANCE(callee)->fields, method_name, &method))) \
       || lit_table_get(&zklass->stat, method_name, &method)) \
    { \
        if(ignoring) \
        { \
            if(call_value(vm, method, arg_count)) \
            { \
                vm_recoverstate(); \
                frame->result_ignored = true; \
            } \
            else \
            { \
                fiber->stack_top[-1] = callee; \
            } \
        } \
        else \
        { \
            vm_callvalue(method, arg_count); \
        } \
    } \
    else \
    { \
        if(error) \
        { \
            vm_rterrorvarg("Attempt to call method '%s', that is not defined in class %s", method_name->chars, \
                               zklass->name->chars) \
        } \
    } \
    if(error) \
    { \
        continue; \
    }

#define vm_invoke_from_class(klass, method_name, arg_count, error, stat, ignoring) \
    vm_invoke_from_class_advanced(klass, method_name, arg_count, error, stat, ignoring, vm_peek(arg_count))

#define vm_invokemethod(instance, method_name, arg_count) \
    LitClass* klass = lit_get_class_for(state, instance); \
    if(klass == NULL) \
    { \
        vm_rterror("Only instances and classes have methods"); \
    } \
    vm_writeframe(); \
    vm_invoke_from_class_advanced(klass, CONST_STRING(state, method_name), arg_count, true, methods, false, instance); \
    vm_readframe()

#define vm_binaryop(type, op, op_string) \
    LitValue a = vm_peek(1); \
    LitValue b = vm_peek(0); \
    if(IS_NUMBER(a)) \
    { \
        if(!IS_NUMBER(b)) \
        { \
            vm_rterrorvarg("Attempt to use op %s with a number and a %s", op_string, lit_get_value_type(b)); \
        } \
        vm_drop(); \
        *(fiber->stack_top - 1) = (type(lit_value_to_number(a) op lit_value_to_number(b))); \
        continue; \
    } \
    if(IS_NULL(a)) \
    { \
        vm_rterrorvarg("Attempt to use op %s on a null value", op_string); \
    } \
    vm_invokemethod(a, op_string, 1)


#define vm_bitwiseop(op, op_string) \
    LitValue a = vm_peek(1); \
    LitValue b = vm_peek(0); \
    if(!IS_NUMBER(a) || !IS_NUMBER(b)) \
    { \
        vm_rterrorvarg("Operands of bitwise op %s must be two numbers, got %s and %s", op_string, \
                           lit_get_value_type(a), lit_get_value_type(b)); \
    } \
    vm_drop(); \
    *(fiber->stack_top - 1) = (lit_number_to_value((int)lit_value_to_number(a) op(int) lit_value_to_number(b)));

#define vm_invokeoperation(ignoring) \
    uint8_t arg_count = vm_readbyte(); \
    LitString* method_name = vm_readstringlong(); \
    LitValue receiver = vm_peek(arg_count); \
    if(IS_NULL(receiver)) \
    { \
        vm_rterror("Attempt to index a null value"); \
    } \
    vm_writeframe(); \
    if(IS_CLASS(receiver)) \
    { \
        vm_invoke_from_class_advanced(AS_CLASS(receiver), method_name, arg_count, true, static_fields, ignoring, receiver); \
        continue; \
    } \
    else if(IS_INSTANCE(receiver)) \
    { \
        LitInstance* instance = AS_INSTANCE(receiver); \
        LitValue value; \
        if(lit_table_get(&instance->fields, method_name, &value)) \
        { \
            fiber->stack_top[-arg_count - 1] = value; \
            vm_callvalue(value, arg_count); \
            vm_readframe(); \
            continue; \
        } \
        vm_invoke_from_class_advanced(instance->klass, method_name, arg_count, true, methods, ignoring, receiver); \
    } \
    else \
    { \
        LitClass* type = lit_get_class_for(state, receiver); \
        if(type == NULL) \
        { \
            vm_rterror("Only instances and classes have methods"); \
        } \
        vm_invoke_from_class_advanced(type, method_name, arg_count, true, methods, ignoring, receiver); \
    }

static jmp_buf jump_buffer;

static void reset_stack(LitVm* vm)
{
    if(vm->fiber != NULL)
    {
        vm->fiber->stack_top = vm->fiber->stack;
    }
}

static void reset_vm(LitState* state, LitVm* vm)
{
    vm->state = state;
    vm->objects = NULL;
    vm->fiber = NULL;
    vm->gray_stack = NULL;
    vm->gray_count = 0;
    vm->gray_capacity = 0;
    lit_init_table(&vm->strings);
    vm->globals = NULL;
    vm->modules = NULL;
}

void lit_init_vm(LitState* state, LitVm* vm)
{
    reset_vm(state, vm);
    vm->globals = lit_create_map(state);
    vm->modules = lit_create_map(state);
}

void lit_free_vm(LitVm* vm)
{
    lit_free_table(vm->state, &vm->strings);
    lit_free_objects(vm->state, vm->objects);
    reset_vm(vm->state, vm);
}

void lit_trace_vm_stack(LitVm* vm)
{
    LitValue* top;
    LitValue* slot;
    LitFiber* fiber;
    fiber = vm->fiber;
    if(fiber->stack_top == fiber->stack || fiber->frame_count == 0)
    {
        return;
    }
    top = fiber->frames[fiber->frame_count - 1].slots;
    printf("        | %s", COLOR_GREEN);
    for(slot = fiber->stack; slot < top; slot++)
    {
        printf("[ ");
        lit_print_value(*slot);
        printf(" ]");
    }
    printf("%s", COLOR_RESET);
    for(slot = top; slot < fiber->stack_top; slot++)
    {
        printf("[ ");
        lit_print_value(*slot);
        printf(" ]");
    }
    printf("\n");
}

bool lit_handle_runtime_error(LitVm* vm, LitString* error_string)
{
    LitValue error = OBJECT_VALUE(error_string);
    LitFiber* fiber = vm->fiber;

    while(fiber != NULL)
    {
        fiber->error = error;

        if(fiber->catcher)
        {
            vm->fiber = fiber->parent;
            vm->fiber->stack_top -= fiber->arg_count;
            vm->fiber->stack_top[-1] = error;

            return true;
        }

        LitFiber* caller = fiber->parent;
        fiber->parent = NULL;
        fiber = caller;
    }

    fiber = vm->fiber;
    fiber->abort = true;
    fiber->error = error;

    if(fiber->parent != NULL)
    {
        fiber->parent->abort = true;
    }

    // Maan, formatting c strings is hard...
    int count = (int)fiber->frame_count - 1;
    size_t length = snprintf(NULL, 0, "%s%s\n", COLOR_RED, error_string->chars);

    for(int i = count; i >= 0; i--)
    {
        LitCallFrame* frame = &fiber->frames[i];
        LitFunction* function = frame->function;
        LitChunk* chunk = &function->chunk;
        const char* name = function->name == NULL ? "unknown" : function->name->chars;

        if(chunk->has_line_info)
        {
            length += snprintf(NULL, 0, "[line %d] in %s()\n", (int)lit_chunk_get_line(chunk, frame->ip - chunk->code - 1), name);
        }
        else
        {
            length += snprintf(NULL, 0, "\tin %s()\n", name);
        }
    }

    length += snprintf(NULL, 0, "%s", COLOR_RESET);
    char buffer[length + 1];
    buffer[length] = '\0';

    char* start = buffer + sprintf(buffer, "%s%s\n", COLOR_RED, error_string->chars);

    for(int i = count; i >= 0; i--)
    {
        LitCallFrame* frame = &fiber->frames[i];
        LitFunction* function = frame->function;
        LitChunk* chunk = &function->chunk;
        const char* name = function->name == NULL ? "unknown" : function->name->chars;

        if(chunk->has_line_info)
        {
            start += sprintf(start, "[line %d] in %s()\n", (int)lit_chunk_get_line(chunk, frame->ip - chunk->code - 1), name);
        }
        else
        {
            start += sprintf(start, "\tin %s()\n", name);
        }
    }

    start += sprintf(start, "%s", COLOR_RESET);
    lit_error(vm->state, RUNTIME_ERROR, buffer);
    reset_stack(vm);

    return false;
}

bool lit_vruntime_error(LitVm* vm, const char* format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    size_t buffer_size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);

    char buffer[buffer_size];
    vsnprintf(buffer, buffer_size, format, args);

    return lit_handle_runtime_error(vm, lit_copy_string(vm->state, buffer, buffer_size));
}

bool lit_runtime_error(LitVm* vm, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    bool result = lit_vruntime_error(vm, format, args);
    va_end(args);

    return result;
}

bool lit_runtime_error_exiting(LitVm* vm, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    bool result = lit_vruntime_error(vm, format, args);
    va_end(args);

    lit_native_exit_jump();
    return result;
}

static bool call(LitVm* vm, LitFunction* function, LitClosure* closure, uint8_t arg_count)
{
    bool vararg;
    size_t amount;
    size_t i;
    size_t osize;
    size_t newcapacity;
    size_t newsize;
    size_t vararg_count;
    size_t function_arg_count;
    LitCallFrame* frame;
    LitFiber* fiber;
    LitArray* array;
    fiber = vm->fiber;

    #if 0
    //if(fiber->frame_count == LIT_CALL_FRAMES_MAX)
    //{
        //lit_runtime_error(vm, "call stack overflow");
        //return true;
    //}
    #endif
    if(fiber->frame_count + 1 > fiber->frame_capacity)
    {
        //newcapacity = fmin(LIT_CALL_FRAMES_MAX, fiber->frame_capacity * 2);
        newcapacity = (fiber->frame_capacity * 2);
        newsize = (sizeof(LitCallFrame) * newcapacity);
        osize = (sizeof(LitCallFrame) * fiber->frame_capacity);
        fiber->frames = (LitCallFrame*)lit_reallocate(vm->state, fiber->frames, osize, newsize);
        fiber->frame_capacity = newcapacity;
    }

    function_arg_count = function->arg_count;
    lit_ensure_fiber_stack(vm->state, fiber, function->max_slots + (int)(fiber->stack_top - fiber->stack));

    frame = &fiber->frames[fiber->frame_count++];

    frame->function = function;
    frame->closure = closure;
    frame->ip = function->chunk.code;
    frame->slots = fiber->stack_top - arg_count - 1;
    frame->result_ignored = false;
    frame->return_to_c = false;

    if(arg_count != function_arg_count)
    {
        vararg = function->vararg;

        if(arg_count < function_arg_count)
        {
            amount = (int)function_arg_count - arg_count - (vararg ? 1 : 0);

            for(i = 0; i < amount; i++)
            {
                lit_push(vm, NULL_VALUE);
            }
            if(vararg)
            {
                lit_push(vm, OBJECT_VALUE(lit_create_array(vm->state)));
            }
        }
        else if(function->vararg)
        {
            array = lit_create_array(vm->state);
            vararg_count = arg_count - function_arg_count + 1;

            lit_push_root(vm->state, (LitObject*)array);
            lit_values_ensure_size(vm->state, &array->values, vararg_count);
            lit_pop_root(vm->state);

            for(i = 0; i < vararg_count; i++)
            {
                array->values.values[i] = vm->fiber->stack_top[(int)i - (int)vararg_count];
            }

            vm->fiber->stack_top -= vararg_count;
            lit_push(vm, OBJECT_VALUE(array));
        }
        else
        {
            vm->fiber->stack_top -= (arg_count - function_arg_count);
        }
    }
    else if(function->vararg)
    {
        array = lit_create_array(vm->state);
        vararg_count = arg_count - function_arg_count + 1;
        lit_push_root(vm->state, (LitObject*)array);
        lit_values_write(vm->state, &array->values, *(fiber->stack_top - 1));
        *(fiber->stack_top - 1) = OBJECT_VALUE(array);
        lit_pop_root(vm->state);
    }

    return true;
}

static bool call_value(LitVm* vm, LitValue callee, uint8_t arg_count)
{
    if(IS_OBJECT(callee))
    {
        if(lit_set_native_exit_jump())
        {
            return true;
        }

        switch(OBJECT_TYPE(callee))
        {
            case OBJECT_FUNCTION:
            {
                return call(vm, AS_FUNCTION(callee), NULL, arg_count);
            }

            case OBJECT_CLOSURE:
            {
                LitClosure* closure = AS_CLOSURE(callee);
                return call(vm, closure->function, closure, arg_count);
            }

            case OBJECT_NATIVE_FUNCTION:
            {
                vm_pushgc(vm->state, false)

                LitValue result = AS_NATIVE_FUNCTION(callee)->function(vm, arg_count, vm->fiber->stack_top - arg_count);
                vm->fiber->stack_top -= arg_count + 1;
                lit_push(vm, result);

                vm_popgc(vm->state);
                return false;
            }

            case OBJECT_NATIVE_PRIMITIVE:
            {
                vm_pushgc(vm->state, false)

                LitFiber* fiber = vm->fiber;
                bool result = AS_NATIVE_PRIMITIVE(callee)->function(vm, arg_count, fiber->stack_top - arg_count);

                if(result)
                {
                    fiber->stack_top -= arg_count;
                }

                vm_popgc(vm->state);
                return result;
            }

            case OBJECT_NATIVE_METHOD:
            {
                vm_pushgc(vm->state, false);

                LitNativeMethod* method = AS_NATIVE_METHOD(callee);
                LitFiber* fiber = vm->fiber;
                LitValue result
                = method->method(vm, *(vm->fiber->stack_top - arg_count - 1), arg_count, vm->fiber->stack_top - arg_count);

                vm->fiber->stack_top -= arg_count + 1;
                lit_push(vm, result);

                vm_popgc(vm->state);
                return false;
            }

            case OBJECT_PRIMITIVE_METHOD:
            {
                vm_pushgc(vm->state, false);

                LitFiber* fiber = vm->fiber;
                bool result = AS_PRIMITIVE_METHOD(callee)->method(vm, *(fiber->stack_top - arg_count - 1), arg_count,
                                                                  fiber->stack_top - arg_count);

                if(result)
                {
                    fiber->stack_top -= arg_count;
                }

                vm_popgc(vm->state);
                return result;
            }

            case OBJECT_CLASS:
            {
                LitClass* klass = AS_CLASS(callee);
                LitInstance* instance = lit_create_instance(vm->state, klass);

                vm->fiber->stack_top[-arg_count - 1] = OBJECT_VALUE(instance);

                if(klass->init_method != NULL)
                {
                    return call_value(vm, OBJECT_VALUE(klass->init_method), arg_count);
                }

                // Remove the arguments, so that they don't mess up the stack
                // (default constructor has no arguments)
                for(size_t i = 0; i < arg_count; i++)
                {
                    lit_pop(vm);
                }

                return false;
            }

            case OBJECT_BOUND_METHOD:
            {
                LitBoundMethod* bound_method = AS_BOUND_METHOD(callee);
                LitValue method = bound_method->method;

                if(IS_NATIVE_METHOD(method))
                {
                    vm_pushgc(vm->state, false);

                    LitValue result = AS_NATIVE_METHOD(method)->method(vm, bound_method->receiver, arg_count,
                                                                       vm->fiber->stack_top - arg_count);
                    vm->fiber->stack_top -= arg_count + 1;
                    lit_push(vm, result);

                    vm_popgc(vm->state);
                    return false;
                }
                else if(IS_PRIMITIVE_METHOD(method))
                {
                    LitFiber* fiber = vm->fiber;
                    vm_pushgc(vm->state, false);

                    if(AS_PRIMITIVE_METHOD(method)->method(vm, bound_method->receiver, arg_count, fiber->stack_top - arg_count))
                    {
                        fiber->stack_top -= arg_count;
                        return true;
                    }

                    vm_popgc(vm->state);
                    return false;
                }
                else
                {
                    vm->fiber->stack_top[-arg_count - 1] = bound_method->receiver;
                    return call(vm, AS_FUNCTION(method), NULL, arg_count);
                }
            }

            default:
            {
                break;
            }
        }
    }

    if(IS_NULL(callee))
    {
        lit_runtime_error(vm, "Attempt to call a null value");
    }
    else
    {
        lit_runtime_error(vm, "Can only call functions and classes, got %s", lit_get_value_type(callee));
    }

    return true;
}

static LitUpvalue* capture_upvalue(LitState* state, LitValue* local)
{
    LitUpvalue* previous_upvalue = NULL;
    LitUpvalue* upvalue = state->vm->fiber->open_upvalues;

    while(upvalue != NULL && upvalue->location > local)
    {
        previous_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    LitUpvalue* created_upvalue = lit_create_upvalue(state, local);
    created_upvalue->next = upvalue;

    if(previous_upvalue == NULL)
    {
        state->vm->fiber->open_upvalues = created_upvalue;
    }
    else
    {
        previous_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

static void close_upvalues(LitVm* vm, const LitValue* last)
{
    LitFiber* fiber = vm->fiber;

    while(fiber->open_upvalues != NULL && fiber->open_upvalues->location >= last)
    {
        LitUpvalue* upvalue = fiber->open_upvalues;

        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;

        fiber->open_upvalues = upvalue->next;
    }
}

LitInterpretResult lit_interpret_module(LitState* state, LitModule* module)
{
    LitVm* vm = state->vm;

    LitFiber* fiber = lit_create_fiber(state, module, module->main_function);
    vm->fiber = fiber;

    lit_push(vm, OBJECT_VALUE(module->main_function));
    LitInterpretResult result = lit_interpret_fiber(state, fiber);

    return result;
}

#include "vmfiber.inc"


#undef vm_rterrorvarg
#undef vm_rterror
#undef vm_invokemethod
#undef vm_invoke_from_class
#undef vm_invoke_from_class_advanced
#undef vm_dropn
#undef vm_push
#undef vm_drop
#undef vm_pop
#undef vm_callvalue
#undef vm_recoverstate
#undef vm_writeframe
#undef vm_readframe
#undef vm_peek
#undef vm_bitwiseop
#undef vm_binaryop
#undef vm_readconstantlong
#undef vm_readconstant
#undef op_case
#undef vm_readstring
#undef vm_readshort
#undef vm_readbyte
#undef vm_returnerror

void lit_native_exit_jump()
{
    longjmp(jump_buffer, 1);
}

bool lit_set_native_exit_jump()
{
    return setjmp(jump_buffer);
}

#undef vm_pushgc
#undef vm_popgc
#undef LIT_TRACE_FRAME