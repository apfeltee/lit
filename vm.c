
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
        *(fiber->stack_top - 1) = (type(AS_NUMBER(a) op AS_NUMBER(b))); \
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
    *(fiber->stack_top - 1) = (NUMBER_VALUE((int)AS_NUMBER(a) op(int) AS_NUMBER(b)));

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
    LitFiber* fiber = vm->fiber;

    if(fiber->stack_top == fiber->stack || fiber->frame_count == 0)
    {
        return;
    }

    LitValue* top = fiber->frames[fiber->frame_count - 1].slots;
    printf("        | %s", COLOR_GREEN);

    for(LitValue* slot = fiber->stack; slot < top; slot++)
    {
        printf("[ ");
        lit_print_value(*slot);
        printf(" ]");
    }

    printf("%s", COLOR_RESET);

    for(LitValue* slot = top; slot < fiber->stack_top; slot++)
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
            length += snprintf(NULL, 0, "[line %d] in %s()\n", lit_chunk_get_line(chunk, frame->ip - chunk->code - 1), name);
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
            start += sprintf(start, "[line %d] in %s()\n", lit_chunk_get_line(chunk, frame->ip - chunk->code - 1), name);
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
    LitFiber* fiber = vm->fiber;

    if(fiber->frame_count == LIT_CALL_FRAMES_MAX)
    {
        lit_runtime_error(vm, "Stack overflow");
        return true;
    }

    if(fiber->frame_count + 1 > fiber->frame_capacity)
    {
        size_t new_capacity = fmin(LIT_CALL_FRAMES_MAX, fiber->frame_capacity * 2);
        fiber->frames = (LitCallFrame*)lit_reallocate(vm->state, fiber->frames, sizeof(LitCallFrame) * fiber->frame_capacity,
                                                      sizeof(LitCallFrame) * new_capacity);
        fiber->frame_capacity = new_capacity;
    }

    size_t function_arg_count = function->arg_count;
    lit_ensure_fiber_stack(vm->state, fiber, function->max_slots + (int)(fiber->stack_top - fiber->stack));

    LitCallFrame* frame = &fiber->frames[fiber->frame_count++];

    frame->function = function;
    frame->closure = closure;
    frame->ip = function->chunk.code;
    frame->slots = fiber->stack_top - arg_count - 1;
    frame->result_ignored = false;
    frame->return_to_c = false;

    if(arg_count != function_arg_count)
    {
        bool vararg = function->vararg;

        if(arg_count < function_arg_count)
        {
            int amount = (int)function_arg_count - arg_count - (vararg ? 1 : 0);

            for(int i = 0; i < amount; i++)
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
            LitArray* array = lit_create_array(vm->state);
            size_t vararg_count = arg_count - function_arg_count + 1;

            lit_push_root(vm->state, (LitObject*)array);
            lit_values_ensure_size(vm->state, &array->values, vararg_count);
            lit_pop_root(vm->state);

            for(size_t i = 0; i < vararg_count; i++)
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
        LitArray* array = lit_create_array(vm->state);
        size_t vararg_count = arg_count - function_arg_count + 1;

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

LitInterpretResult lit_interpret_fiber(LitState* state, LitFiber* fiber)
{
    LitVm* vm = state->vm;
    vm_pushgc(state, true);

    vm->fiber = fiber;
    fiber->abort = false;

    LitCallFrame* frame = &fiber->frames[fiber->frame_count - 1];
    LitChunk* current_chunk = &frame->function->chunk;

    fiber->module = frame->function->module;

    uint8_t* ip = frame->ip;
    LitValue* slots = frame->slots;
    LitValue* privates = fiber->module->privates;
    LitUpvalue** upvalues = frame->closure == NULL ? NULL : frame->closure->upvalues;

    // Has to be inside of the function in order for goto to work
    static void* dispatch_table[] =
    {
        #define OPCODE(name, effect) &&OP_##name,
        #include "opcodes.inc"
        #undef OPCODE
    };

#ifdef LIT_TRACE_EXECUTION
    TRACE_FRAME();
    uint8_t instruction;
#endif

    while(true)
    {
#ifdef LIT_TRACE_STACK
        lit_trace_vm_stack(vm);
#endif

#ifdef LIT_CHECK_STACK_SIZE
        if((fiber->stack_top - frame->slots) > fiber->stack_capacity)
        {
            vm_rterrorvarg("Fiber stack is not large enough (%i > %i)", (int)(fiber->stack_top - frame->slots),
                               fiber->stack_capacity);
        }
#endif

#ifdef LIT_TRACE_EXECUTION
        instruction = *ip++;

        lit_disassemble_instruction(current_chunk, (size_t)(ip - current_chunk->code - 1), NULL);
        goto* dispatch_table[instruction];
#else
        goto* dispatch_table[*ip++];
#endif

        op_case(POP)
        {
            vm_drop();
            continue;
        }

        op_case(RETURN)
        {
            LitValue result = vm_pop();
            close_upvalues(vm, slots);

            vm_writeframe();
            fiber->frame_count--;

            if(frame->return_to_c)
            {
                frame->return_to_c = false;
                fiber->module->return_value = result;
                fiber->stack_top = frame->slots;

                return (LitInterpretResult){ INTERPRET_OK, result };
            }

            if(fiber->frame_count == 0)
            {
                fiber->module->return_value = result;

                if(fiber->parent == NULL)
                {
                    vm_drop();

                    state->allow_gc = was_allowed;
                    return (LitInterpretResult){ INTERPRET_OK, result };
                }

                size_t arg_count = fiber->arg_count;
                LitFiber* parent = fiber->parent;
                fiber->parent = NULL;

                vm->fiber = fiber = parent;

                vm_readframe();
                TRACE_FRAME();

                fiber->stack_top -= arg_count;
                fiber->stack_top[-1] = result;

                continue;
            }

            fiber->stack_top = frame->slots;

            if(frame->result_ignored)
            {
                fiber->stack_top++;
                frame->result_ignored = false;
            }
            else
            {
                vm_push(result);
            }

            vm_readframe();
            TRACE_FRAME();

            continue;
        }

        op_case(CONSTANT)
        {
            vm_push(vm_readconstant());
            continue;
        }

        op_case(CONSTANT_LONG)
        {
            vm_push(vm_readconstantlong());
            continue;
        }

        op_case(TRUE)
        {
            vm_push(TRUE_VALUE);
            continue;
        }

        op_case(FALSE)
        {
            vm_push(FALSE_VALUE);
            continue;
        }

        op_case(NULL)
        {
            vm_push(NULL_VALUE);
            continue;
        }

        op_case(ARRAY)
        {
            vm_push(OBJECT_VALUE(lit_create_array(state)));
            continue;
        }

        op_case(OBJECT)
        {
            vm_push(OBJECT_VALUE(lit_create_instance(state, state->object_class)));
            continue;
        }

        op_case(RANGE)
        {
            LitValue a = vm_pop();
            LitValue b = vm_pop();

            if(!IS_NUMBER(a) || !IS_NUMBER(b))
            {
                vm_rterror("Range operands must be number");
            }

            vm_push(OBJECT_VALUE(lit_create_range(state, AS_NUMBER(a), AS_NUMBER(b))));
            continue;
        }

        op_case(NEGATE)
        {
            if(!IS_NUMBER(vm_peek(0)))
            {
                LitValue arg = vm_peek(0);

                // Don't even ask me why
                // This doesn't kill our performance, since it's a error anyway
                if(IS_STRING(arg) && strcmp(AS_CSTRING(arg), "muffin") == 0)
                {
                    vm_rterror("Idk, can you negate a muffin?");
                }
                else
                {
                    vm_rterror("Operand must be a number");
                }
            }

            vm_push(NUMBER_VALUE(-AS_NUMBER(vm_pop())));
            continue;
        }

        op_case(NOT)
        {
            if(IS_INSTANCE(vm_peek(0)))
            {
                vm_writeframe();
                vm_invoke_from_class(AS_INSTANCE(vm_peek(0))->klass, CONST_STRING(state, "!"), 0, false, methods, false);
                continue;
            }

            vm_push(BOOL_VALUE(lit_is_falsey(vm_pop())));
            continue;
        }

        op_case(BNOT)
        {
            if(!IS_NUMBER(vm_peek(0)))
            {
                vm_rterror("Operand must be a number");
            }

            vm_push(NUMBER_VALUE(~((int)AS_NUMBER(vm_pop()))));
            continue;
        }

        op_case(ADD)
        {
            vm_binaryop(NUMBER_VALUE, +, "+");
            continue;
        }

        op_case(SUBTRACT)
        {
            vm_binaryop(NUMBER_VALUE, -, "-");
            continue;
        }

        op_case(MULTIPLY)
        {
            vm_binaryop(NUMBER_VALUE, *, "*");
            continue;
        }

        op_case(POWER)
        {
            LitValue a = vm_peek(1);
            LitValue b = vm_peek(0);

            if(IS_NUMBER(a) && IS_NUMBER(b))
            {
                vm_drop();
                *(fiber->stack_top - 1) = (NUMBER_VALUE(pow(AS_NUMBER(a), AS_NUMBER(b))));

                continue;
            }

            vm_invokemethod(a, "**", 1);
            continue;
        }

        op_case(DIVIDE)
        {
            vm_binaryop(NUMBER_VALUE, /, "/");
            continue;
        }

        op_case(FLOOR_DIVIDE)
        {
            LitValue a = vm_peek(1);
            LitValue b = vm_peek(0);

            if(IS_NUMBER(a) && IS_NUMBER(b))
            {
                vm_drop();
                *(fiber->stack_top - 1) = (NUMBER_VALUE(floor(AS_NUMBER(a) / AS_NUMBER(b))));

                continue;
            }

            vm_invokemethod(a, "#", 1);
            continue;
        }

        op_case(MOD)
        {
            LitValue a = vm_peek(1);
            LitValue b = vm_peek(0);

            if(IS_NUMBER(a) && IS_NUMBER(b))
            {
                vm_drop();
                *(fiber->stack_top - 1) = NUMBER_VALUE(fmod(AS_NUMBER(a), AS_NUMBER(b)));
                continue;
            }

            vm_invokemethod(a, "%", 1);
            continue;
        }

        op_case(BAND)
        {
            vm_bitwiseop(&, "&");
            continue;
        }

        op_case(BOR)
        {
            vm_bitwiseop(|, "|");
            continue;
        }

        op_case(BXOR)
        {
            vm_bitwiseop(^, "^");
            continue;
        }

        op_case(LSHIFT)
        {
            vm_bitwiseop(<<, "<<");
            continue;
        }

        op_case(RSHIFT)
        {
            vm_bitwiseop(>>, ">>");
            continue;
        }

        op_case(EQUAL)
        {
            if(IS_INSTANCE(vm_peek(1)))
            {
                vm_writeframe();
                vm_invoke_from_class(AS_INSTANCE(vm_peek(1))->klass, CONST_STRING(state, "=="), 1, false, methods, false);
                continue;
            }

            LitValue a = vm_pop();
            LitValue b = vm_pop();

            vm_push(BOOL_VALUE(a == b));
            continue;
        }

        op_case(GREATER)
        {
            vm_binaryop(BOOL_VALUE, >, ">");
            continue;
        }

        op_case(GREATER_EQUAL)
        {
            vm_binaryop(BOOL_VALUE, >=, ">=");
            continue;
        }

        op_case(LESS)
        {
            vm_binaryop(BOOL_VALUE, <, "<");
            continue;
        }

        op_case(LESS_EQUAL)
        {
            vm_binaryop(BOOL_VALUE, <=, "<=");
            continue;
        }

        op_case(SET_GLOBAL)
        {
            LitString* name = vm_readstringlong();
            lit_table_set(state, &vm->globals->values, name, vm_peek(0));

            continue;
        }

        op_case(GET_GLOBAL)
        {
            LitString* name = vm_readstringlong();
            LitValue value;

            if(!lit_table_get(&vm->globals->values, name, &value))
            {
                vm_push(NULL_VALUE);
            }
            else
            {
                vm_push(value);
            }

            continue;
        }

        op_case(SET_LOCAL)
        {
            uint8_t index = vm_readbyte();
            slots[index] = vm_peek(0);

            continue;
        }

        op_case(GET_LOCAL)
        {
            vm_push(slots[vm_readbyte()]);
            continue;
        }

        op_case(SET_LOCAL_LONG)
        {
            uint8_t index = vm_readshort();
            slots[index] = vm_peek(0);

            continue;
        }

        op_case(GET_LOCAL_LONG)
        {
            vm_push(slots[vm_readshort()]);
            continue;
        }

        op_case(SET_PRIVATE)
        {
            uint8_t index = vm_readbyte();
            privates[index] = vm_peek(0);

            continue;
        }

        op_case(GET_PRIVATE)
        {
            vm_push(privates[vm_readbyte()]);
            continue;
        }

        op_case(SET_PRIVATE_LONG)
        {
            uint8_t index = vm_readshort();
            privates[index] = vm_peek(0);

            continue;
        }

        op_case(GET_PRIVATE_LONG)
        {
            vm_push(privates[vm_readshort()]);
            continue;
        }

        op_case(SET_UPVALUE)
        {
            uint8_t index = vm_readbyte();
            *upvalues[index]->location = vm_peek(0);

            continue;
        }

        op_case(GET_UPVALUE)
        {
            vm_push(*upvalues[vm_readbyte()]->location);
            continue;
        }

        op_case(JUMP_IF_FALSE)
        {
            uint16_t offset = vm_readshort();

            if(lit_is_falsey(vm_pop()))
            {
                ip += offset;
            }

            continue;
        }

        op_case(JUMP_IF_NULL)
        {
            uint16_t offset = vm_readshort();

            if(IS_NULL(vm_peek(0)))
            {
                ip += offset;
            }

            continue;
        }

        op_case(JUMP_IF_NULL_POPPING)
        {
            uint16_t offset = vm_readshort();

            if(IS_NULL(vm_pop()))
            {
                ip += offset;
            }

            continue;
        }

        op_case(JUMP)
        {
            uint16_t offset = vm_readshort();
            ip += offset;

            continue;
        }

        op_case(JUMP_BACK)
        {
            uint16_t offset = vm_readshort();
            ip -= offset;

            continue;
        }

        op_case(AND)
        {
            uint16_t offset = vm_readshort();

            if(lit_is_falsey(vm_peek(0)))
            {
                ip += offset;
            }
            else
            {
                vm_drop();
            }

            continue;
        }

        op_case(OR)
        {
            uint16_t offset = vm_readshort();

            if(lit_is_falsey(vm_peek(0)))
            {
                vm_drop();
            }
            else
            {
                ip += offset;
            }

            continue;
        }

        op_case(NULL_OR)
        {
            uint16_t offset = vm_readshort();

            if(IS_NULL(vm_peek(0)))
            {
                vm_drop();
            }
            else
            {
                ip += offset;
            }

            continue;
        }

        op_case(CALL)
        {
            uint8_t arg_count = vm_readbyte();

            vm_writeframe();
            vm_callvalue(vm_peek(arg_count), arg_count);

            continue;
        }

        op_case(CLOSURE)
        {
            LitFunction* function = AS_FUNCTION(vm_readconstantlong());
            LitClosure* closure = lit_create_closure(state, function);

            vm_push(OBJECT_VALUE(closure));

            for(size_t i = 0; i < closure->upvalue_count; i++)
            {
                uint8_t is_local = vm_readbyte();
                uint8_t index = vm_readbyte();

                if(is_local)
                {
                    closure->upvalues[i] = capture_upvalue(state, frame->slots + index);
                }
                else
                {
                    closure->upvalues[i] = upvalues[index];
                }
            }

            continue;
        }

        op_case(CLOSE_UPVALUE)
        {
            close_upvalues(vm, fiber->stack_top - 1);
            vm_drop();

            continue;
        }

        op_case(CLASS)
        {
            LitString* name = vm_readstringlong();
            LitClass* klass = lit_create_class(state, name);

            vm_push(OBJECT_VALUE(klass));

            klass->super = state->object_class;

            lit_table_add_all(state, &klass->super->methods, &klass->methods);
            lit_table_add_all(state, &klass->super->static_fields, &klass->static_fields);

            lit_table_set(state, &vm->globals->values, name, OBJECT_VALUE(klass));

            continue;
        }

        op_case(GET_FIELD)
        {
            LitValue object = vm_peek(1);

            if(IS_NULL(object))
            {
                vm_rterror("Attempt to index a null value");
            }

            LitValue value;
            LitString* name = AS_STRING(vm_peek(0));

            if(IS_INSTANCE(object))
            {
                LitInstance* instance = AS_INSTANCE(object);

                if(!lit_table_get(&instance->fields, name, &value))
                {
                    if(lit_table_get(&instance->klass->methods, name, &value))
                    {
                        if(IS_FIELD(value))
                        {
                            LitField* field = AS_FIELD(value);

                            if(field->getter == NULL)
                            {
                                vm_rterrorvarg("Class %s does not have a getter for the field %s",
                                                   instance->klass->name->chars, name->chars);
                            }

                            vm_drop();
                            vm_writeframe();
                            vm_callvalue(OBJECT_VALUE(AS_FIELD(value)->getter), 0);
                            vm_readframe();
                            continue;
                        }
                        else
                        {
                            value = OBJECT_VALUE(lit_create_bound_method(state, OBJECT_VALUE(instance), value));
                        }
                    }
                    else
                    {
                        value = NULL_VALUE;
                    }
                }
            }
            else if(IS_CLASS(object))
            {
                LitClass* klass = AS_CLASS(object);

                if(lit_table_get(&klass->static_fields, name, &value))
                {
                    if(IS_NATIVE_METHOD(value) || IS_PRIMITIVE_METHOD(value))
                    {
                        value = OBJECT_VALUE(lit_create_bound_method(state, OBJECT_VALUE(klass), value));
                    }
                    else if(IS_FIELD(value))
                    {
                        LitField* field = AS_FIELD(value);

                        if(field->getter == NULL)
                        {
                            vm_rterrorvarg("Class %s does not have a getter for the field %s", klass->name->chars,
                                               name->chars);
                        }

                        vm_drop();
                        vm_writeframe();
                        vm_callvalue(OBJECT_VALUE(field->getter), 0);
                        vm_readframe();
                        continue;
                    }
                }
                else
                {
                    value = NULL_VALUE;
                }
            }
            else
            {
                LitClass* klass = lit_get_class_for(state, object);

                if(klass == NULL)
                {
                    vm_rterror("Only instances and classes have fields");
                }

                if(lit_table_get(&klass->methods, name, &value))
                {
                    if(IS_FIELD(value))
                    {
                        LitField* field = AS_FIELD(value);

                        if(field->getter == NULL)
                        {
                            vm_rterrorvarg("Class %s does not have a getter for the field %s", klass->name->chars,
                                               name->chars);
                        }

                        vm_drop();
                        vm_writeframe();
                        vm_callvalue(OBJECT_VALUE(AS_FIELD(value)->getter), 0);
                        vm_readframe();
                        continue;
                    }
                    else if(IS_NATIVE_METHOD(value) || IS_PRIMITIVE_METHOD(value))
                    {
                        value = OBJECT_VALUE(lit_create_bound_method(state, object, value));
                    }
                }
                else
                {
                    value = NULL_VALUE;
                }
            }

            vm_drop();// Pop field name
            fiber->stack_top[-1] = value;

            continue;
        }

        op_case(SET_FIELD)
        {
            LitValue instance = vm_peek(2);

            if(IS_NULL(instance))
            {
                vm_rterror("Attempt to index a null value")
            }

            LitValue value = vm_peek(1);
            LitString* field_name = AS_STRING(vm_peek(0));

            if(IS_CLASS(instance))
            {
                LitClass* klass = AS_CLASS(instance);
                LitValue setter;

                if(lit_table_get(&klass->static_fields, field_name, &setter) && IS_FIELD(setter))
                {
                    LitField* field = AS_FIELD(setter);

                    if(field->setter == NULL)
                    {
                        vm_rterrorvarg("Class %s does not have a setter for the field %s", klass->name->chars,
                                           field_name->chars);
                    }

                    vm_dropn(2);
                    vm_push(value);
                    vm_writeframe();
                    vm_callvalue(OBJECT_VALUE(field->setter), 1);
                    vm_readframe();
                    continue;
                }

                if(IS_NULL(value))
                {
                    lit_table_delete(&klass->static_fields, field_name);
                }
                else
                {
                    lit_table_set(state, &klass->static_fields, field_name, value);
                }

                vm_dropn(2);// Pop field name and the value
                fiber->stack_top[-1] = value;
            }
            else if(IS_INSTANCE(instance))
            {
                LitInstance* inst = AS_INSTANCE(instance);
                LitValue setter;

                if(lit_table_get(&inst->klass->methods, field_name, &setter) && IS_FIELD(setter))
                {
                    LitField* field = AS_FIELD(setter);

                    if(field->setter == NULL)
                    {
                        vm_rterrorvarg("Class %s does not have a setter for the field %s", inst->klass->name->chars,
                                           field_name->chars);
                    }

                    vm_dropn(2);
                    vm_push(value);
                    vm_writeframe();
                    vm_callvalue(OBJECT_VALUE(field->setter), 1);
                    vm_readframe();
                    continue;
                }

                if(IS_NULL(value))
                {
                    lit_table_delete(&inst->fields, field_name);
                }
                else
                {
                    lit_table_set(state, &inst->fields, field_name, value);
                }

                vm_dropn(2);// Pop field name and the value
                fiber->stack_top[-1] = value;
            }
            else
            {
                LitClass* klass = lit_get_class_for(state, instance);

                if(klass == NULL)
                {
                    vm_rterror("Only instances and classes have fields");
                }

                LitValue setter;

                if(lit_table_get(&klass->methods, field_name, &setter) && IS_FIELD(setter))
                {
                    LitField* field = AS_FIELD(setter);

                    if(field->setter == NULL)
                    {
                        vm_rterrorvarg("Class %s does not have a setter for the field %s", klass->name->chars,
                                           field_name->chars);
                    }

                    vm_dropn(2);
                    vm_push(value);
                    vm_writeframe();
                    vm_callvalue(OBJECT_VALUE(field->setter), 1);
                    vm_readframe();
                    continue;
                }
                else
                {
                    vm_rterrorvarg("Class %s does not contain field %s", klass->name->chars, field_name->chars);
                }
            }

            continue;
        }

        op_case(SUBSCRIPT_GET)
        {
            vm_invokemethod(vm_peek(1), "[]", 1);
            continue;
        }

        op_case(SUBSCRIPT_SET)
        {
            vm_invokemethod(vm_peek(2), "[]", 2);
            continue;
        }

        op_case(PUSH_ARRAY_ELEMENT)
        {
            LitValues* values = &AS_ARRAY(vm_peek(1))->values;
            int index = values->count;

            lit_values_ensure_size(state, values, index + 1);
            values->values[index] = vm_peek(0);
            vm_drop();

            continue;
        }

        op_case(PUSH_OBJECT_FIELD)
        {
            LitValue operand = vm_peek(2);

            if(IS_MAP(operand))
            {
                lit_table_set(state, &AS_MAP(operand)->values, AS_STRING(vm_peek(1)), vm_peek(0));
            }
            else if(IS_INSTANCE(operand))
            {
                lit_table_set(state, &AS_INSTANCE(operand)->fields, AS_STRING(vm_peek(1)), vm_peek(0));
            }
            else
            {
                vm_rterrorvarg("Expected an object or a map as the operand, got %s", lit_get_value_type(operand));
            }

            vm_dropn(2);
            continue;
        }

        op_case(STATIC_FIELD)
        {
            lit_table_set(state, &AS_CLASS(vm_peek(1))->static_fields, vm_readstringlong(), vm_peek(0));
            vm_drop();

            continue;
        }

        op_case(METHOD)
        {
            LitClass* klass = AS_CLASS(vm_peek(1));
            LitString* name = vm_readstringlong();

            if((klass->init_method == NULL || (klass->super != NULL && klass->init_method == ((LitClass*)klass->super)->init_method))
               && name->length == 11 && memcmp(name->chars, "constructor", 11) == 0)
            {
                klass->init_method = AS_OBJECT(vm_peek(0));
            }

            lit_table_set(state, &klass->methods, name, vm_peek(0));
            vm_drop();

            continue;
        }

        op_case(DEFINE_FIELD)
        {
            lit_table_set(state, &AS_CLASS(vm_peek(1))->methods, vm_readstringlong(), vm_peek(0));
            vm_drop();

            continue;
        }

        op_case(INVOKE)
        {
            vm_invokeoperation(false);
            continue;
        }

        op_case(INVOKE_IGNORING)
        {
            vm_invokeoperation(true);
            continue;
        }

        op_case(INVOKE_SUPER)
        {
            uint8_t arg_count = vm_readbyte();
            LitString* method_name = vm_readstringlong();
            LitClass* klass = AS_CLASS(vm_pop());

            vm_writeframe();
            vm_invoke_from_class(klass, method_name, arg_count, true, methods, false);
            continue;
        }

        op_case(INVOKE_SUPER_IGNORING)
        {
            uint8_t arg_count = vm_readbyte();
            LitString* method_name = vm_readstringlong();
            LitClass* klass = AS_CLASS(vm_pop());

            vm_writeframe();
            vm_invoke_from_class(klass, method_name, arg_count, true, methods, true);
            continue;
        }

        op_case(GET_SUPER_METHOD)
        {
            LitString* method_name = vm_readstringlong();
            LitClass* klass = AS_CLASS(vm_pop());
            LitValue instance = vm_pop();

            LitValue value;

            if(lit_table_get(&klass->methods, method_name, &value))
            {
                value = OBJECT_VALUE(lit_create_bound_method(state, instance, value));
            }
            else
            {
                value = NULL_VALUE;
            }

            vm_push(value);
            continue;
        }

        op_case(INHERIT)
        {
            LitValue super = vm_peek(1);

            if(!IS_CLASS(super))
            {
                vm_rterror("Superclass must be a class");
            }

            LitClass* klass = AS_CLASS(vm_peek(0));
            LitClass* super_klass = AS_CLASS(super);

            klass->super = super_klass;
            klass->init_method = super_klass->init_method;

            lit_table_add_all(state, &super_klass->methods, &klass->methods);
            lit_table_add_all(state, &klass->super->static_fields, &klass->static_fields);

            continue;
        }

        op_case(IS)
        {
            LitValue instance = vm_peek(1);

            if(IS_NULL(instance))
            {
                vm_dropn(2);
                vm_push(FALSE_VALUE);

                continue;
            }

            LitClass* instance_klass = lit_get_class_for(state, instance);
            LitValue klass = vm_peek(0);

            if(instance_klass == NULL || !IS_CLASS(klass))
            {
                vm_rterror("Operands must be an instance and a class");
            }

            LitClass* type = AS_CLASS(klass);
            bool found = false;

            while(instance_klass != NULL)
            {
                if(instance_klass == type)
                {
                    found = true;
                    break;
                }

                instance_klass = (LitClass*)instance_klass->super;
            }

            vm_dropn(2);// Drop the instance and class
            vm_push(BOOL_VALUE(found));

            continue;
        }

        op_case(POP_LOCALS)
        {
            vm_dropn(vm_readshort());
            continue;
        }

        op_case(VARARG)
        {
            LitValue slot = slots[vm_readbyte()];

            if(!IS_ARRAY(slot))
            {
                continue;
            }

            LitValues* values = &AS_ARRAY(slot)->values;
            lit_ensure_fiber_stack(state, fiber,
                                   values->count + frame->function->max_slots + (int)(fiber->stack_top - fiber->stack));

            for(size_t i = 0; i < values->count; i++)
            {
                vm_push(values->values[i]);
            }

            // Hot-bytecode patching, increment the amount of arguments to OP_CALL
            ip[1] = ip[1] + values->count - 1;
            continue;
        }

        op_case(REFERENCE_GLOBAL)
        {
            LitString* name = vm_readstringlong();
            LitValue* value;

            if(lit_table_get_slot(&vm->globals->values, name, &value))
            {
                vm_push(OBJECT_VALUE(lit_create_reference(state, value)));
            }
            else
            {
                vm_rterror("Attempt to reference a null value");
            }

            continue;
        }

        op_case(REFERENCE_PRIVATE)
        {
            vm_push(OBJECT_VALUE(lit_create_reference(state, &privates[vm_readshort()])));
            continue;
        }

        op_case(REFERENCE_LOCAL)
        {
            vm_push(OBJECT_VALUE(lit_create_reference(state, &slots[vm_readshort()])));
            continue;
        }

        op_case(REFERENCE_UPVALUE)
        {
            vm_push(OBJECT_VALUE(lit_create_reference(state, upvalues[vm_readbyte()]->location)));
            continue;
        }

        op_case(REFERENCE_FIELD)
        {
            LitValue object = vm_peek(1);

            if(IS_NULL(object))
            {
                vm_rterror("Attempt to index a null value");
            }

            LitValue* value;
            LitString* name = AS_STRING(vm_peek(0));

            if(IS_INSTANCE(object))
            {
                if(!lit_table_get_slot(&AS_INSTANCE(object)->fields, name, &value))
                {
                    vm_rterror("Attempt to reference a null value");
                }
            }
            else
            {
                lit_print_value(object);
                printf("\n");
                vm_rterror("You can only reference fields of real instances");
            }

            vm_drop();// Pop field name
            fiber->stack_top[-1] = OBJECT_VALUE(lit_create_reference(state, value));

            continue;
        }

        op_case(SET_REFERENCE)
        {
            LitValue reference = vm_pop();

            if(!IS_REFERENCE(reference))
            {
                vm_rterror("Provided value is not a reference");
            }

            *AS_REFERENCE(reference)->slot = vm_peek(0);
            continue;
        }

        vm_rterrorvarg("Unknown op code '%d'", *ip);
        break;
    }

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

    vm_returnerror();

#undef vm_returnerror
}

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