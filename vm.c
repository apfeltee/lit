
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include "lit.h"

typedef struct LitExecState LitExecState;

struct LitExecState
{
    LitValue* slots;
    LitValue* privates;
    LitUpvalue** upvalues;
    uint8_t* ip;
    LitCallFrame* frame;
    LitChunk* current_chunk;

};


//#define LIT_TRACE_EXECUTION

/*
* visual studio doesn't support computed gotos, so
* instead a switch-case is used. 
*/
#if !defined(_MSC_VER)
    #define LIT_USE_COMPUTEDGOTO
#endif

#ifdef LIT_TRACE_EXECUTION
    #define vm_traceframe(fiber)\
        lit_trace_frame(fiber);
#else
    #define vm_traceframe(fiber) \
        do \
        { \
        } while(0);
#endif

// the following macros cannot be turned into a function without
// breaking everything
#define vm_returnerror() \
    vm_popgc(state); \
    return (LitInterpretResult){ LITRESULT_RUNTIME_ERROR, NULL_VALUE };



/*
* ===========
*/


static inline uint16_t vm_readshort(LitExecState* est)
{
    est->ip += 2u;
    return (uint16_t)((est->ip[-2] << 8u) | est->ip[-1]);
}


static inline uint8_t vm_readbyte(LitExecState* est)
{
    return (*est->ip++);
}

static inline LitValue vm_readconstant(LitExecState* est)
{
    return lit_vallist_get(&est->current_chunk->constants, vm_readbyte(est));
}

static inline LitValue vm_readconstantlong(LitExecState* est)
{
    return lit_vallist_get(&est->current_chunk->constants, vm_readshort(est));
}

static inline LitString* vm_readstring(LitExecState* est)
{
    return lit_as_string(vm_readconstant(est));
}

static inline LitString* vm_readstringlong(LitExecState* est)
{
    return lit_as_string(vm_readconstantlong(est));
}

#ifdef LIT_USE_COMPUTEDGOTO
    #define vm_default()
    #define op_case(name) \
        OP_##name:
#else
    #define vm_default() default:
    #define op_case(name) \
        case OP_##name:
#endif

#define vm_pushgc(state, allow) \
    bool was_allowed = state->allow_gc; \
    state->allow_gc = allow;

#define vm_popgc(state) \
    state->allow_gc = was_allowed;

static inline void vm_push(LitFiber* fiber, LitValue v)
{
    *fiber->stack_top++ = v;
}

static inline LitValue vm_pop(LitFiber* fiber)
{
    return *(--fiber->stack_top);
}

static inline void vm_drop(LitFiber* fiber)
{
    fiber->stack_top--;
}

static inline void vm_dropn(LitFiber* fiber, int amount)
{
    fiber->stack_top -= amount;
}

static inline LitValue vm_peek(LitFiber* fiber, short distance)
{
    return fiber->stack_top[(-1) - distance];
}

void vm_readframe(LitFiber* fiber, LitExecState* est)
{
    est->frame = &fiber->frames[fiber->frame_count - 1];
    est->current_chunk = &est->frame->function->chunk;
    est->ip = est->frame->ip;
    est->slots = est->frame->slots;
    fiber->module = est->frame->function->module;
    est->privates = fiber->module->privates;
    est->upvalues = est->frame->closure == NULL ? NULL : est->frame->closure->upvalues;
}


static inline void vm_writeframe(LitExecState* est, uint8_t* ip)
{
    est->frame->ip = ip;
}
enum
{
    RECOVER_RETURNOK,
    RECOVER_RETURNFAIL,
    RECOVER_NOTHING
};

    #define vm_recoverstate(fiber, est) \
        vm_writeframe(&est, est.ip); \
        fiber = vm->fiber; \
        if(fiber == NULL) \
        { \
            return (LitInterpretResult){ LITRESULT_OK, vm_pop(fiber) }; \
        } \
        if(fiber->abort) \
        { \
            vm_returnerror(); \
        } \
        vm_readframe(fiber, &est); \
        vm_traceframe(fiber);

#define vm_callvalue(callee, arg_count) \
    if(call_value(vm, callee, arg_count)) \
    { \
        vm_recoverstate(fiber, est); \
    }

#define vm_rterror(format) \
    if(lit_runtime_error(vm, format)) \
    { \
        vm_recoverstate(fiber, est); \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }

#define vm_rterrorvarg(format, ...) \
    if(lit_runtime_error(vm, format, __VA_ARGS__)) \
    { \
        vm_recoverstate(fiber, est);  \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }

#define vm_invoke_from_class_advanced(zklass, method_name, arg_count, error, stat, ignoring, callee) \
    LitValue mthval; \
    if((IS_INSTANCE(callee) && (lit_table_get(&AS_INSTANCE(callee)->fields, method_name, &mthval))) \
       || lit_table_get(&zklass->stat, method_name, &mthval)) \
    { \
        if(ignoring) \
        { \
            if(call_value(vm, mthval, arg_count)) \
            { \
                vm_recoverstate(fiber, est); \
                est.frame->result_ignored = true; \
            } \
            else \
            { \
                fiber->stack_top[-1] = callee; \
            } \
        } \
        else \
        { \
            vm_callvalue(mthval, arg_count); \
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
    vm_invoke_from_class_advanced(klass, method_name, arg_count, error, stat, ignoring, vm_peek(fiber, arg_count))

#define vm_invokemethod(instance, method_name, arg_count) \
    LitClass* klass = lit_state_getclassfor(state, instance); \
    if(klass == NULL) \
    { \
        vm_rterror("invokemethod: only instances and classes have methods"); \
    } \
    vm_writeframe(&est, est.ip); \
    vm_invoke_from_class_advanced(klass, CONST_STRING(state, method_name), arg_count, true, methods, false, instance); \
    vm_readframe(fiber, &est)

#define vm_binaryop(type, op, op_string) \
    LitValue a = vm_peek(fiber, 1); \
    LitValue b = vm_peek(fiber, 0); \
    if(IS_NUMBER(a)) \
    { \
        if(!IS_NUMBER(b)) \
        { \
            if(!IS_NULL(b)) \
            { \
                vm_rterrorvarg("Attempt to use op %s with a number and a %s", op_string, lit_get_value_type(b)); \
            } \
        } \
        vm_drop(fiber); \
        *(fiber->stack_top - 1) = (type(lit_value_to_number(a) op lit_value_to_number(b))); \
        continue; \
    } \
    if(IS_NULL(a)) \
    { \
    /* vm_rterrorvarg("Attempt to use op %s on a null value", op_string); */ \
        vm_drop(fiber); \
        *(fiber->stack_top - 1) = TRUE_VALUE; \
    } \
    else \
    { \
        vm_invokemethod(a, op_string, 1); \
    }

#define vm_bitwiseop(op, op_string) \
    LitValue a = vm_peek(fiber, 1); \
    LitValue b = vm_peek(fiber, 0); \
    /* if((!IS_NUMBER(a) && !IS_NULL(a)) || (!IS_NUMBER(b) && !IS_NULL(b))) */ \
    if((!IS_NUMBER(a) ) || (!IS_NUMBER(b))) \
    { \
        vm_rterrorvarg("Operands of bitwise op %s must be two numbers, got %s and %s", op_string, \
                           lit_get_value_type(a), lit_get_value_type(b)); \
    } \
    vm_drop(fiber); \
    *(fiber->stack_top - 1) = (lit_number_to_value((int)lit_value_to_number(a) op(int) lit_value_to_number(b)));

#define vm_invokeoperation(ignoring) \
    uint8_t arg_count = vm_readbyte(&est); \
    LitString* method_name = vm_readstringlong(&est); \
    LitValue receiver = vm_peek(fiber, arg_count); \
    if(IS_NULL(receiver)) \
    { \
        vm_rterror("Attempt to index a null value"); \
    } \
    vm_writeframe(&est, est.ip); \
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
            vm_readframe(fiber, &est); \
            continue; \
        } \
        vm_invoke_from_class_advanced(instance->klass, method_name, arg_count, true, methods, ignoring, receiver); \
    } \
    else \
    { \
        LitClass* type = lit_state_getclassfor(state, receiver); \
        if(type == NULL) \
        { \
            vm_rterror("invokeoperation: only instances and classes have methods"); \
        } \
        vm_invoke_from_class_advanced(type, method_name, arg_count, true, methods, ignoring, receiver); \
    }

static jmp_buf jump_buffer;

static void reset_stack(LitVM* vm)
{
    if(vm->fiber != NULL)
    {
        vm->fiber->stack_top = vm->fiber->stack;
    }
}

static void reset_vm(LitState* state, LitVM* vm)
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

void lit_init_vm(LitState* state, LitVM* vm)
{
    reset_vm(state, vm);
    vm->globals = lit_create_map(state);
    vm->modules = lit_create_map(state);
}

void lit_free_vm(LitVM* vm)
{
    lit_free_table(vm->state, &vm->strings);
    lit_free_objects(vm->state, vm->objects);
    reset_vm(vm->state, vm);
}

void lit_trace_vm_stack(LitVM* vm, LitWriter* wr)
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
    lit_writer_writeformat(wr, "        | %s", COLOR_GREEN);
    for(slot = fiber->stack; slot < top; slot++)
    {
        lit_writer_writeformat(wr, "[ ");
        lit_print_value(vm->state, wr, *slot);
        lit_writer_writeformat(wr, " ]");
    }
    lit_writer_writeformat(wr, "%s", COLOR_RESET);
    for(slot = top; slot < fiber->stack_top; slot++)
    {
        lit_writer_writeformat(wr, "[ ");
        lit_print_value(vm->state, wr, *slot);
        lit_writer_writeformat(wr, " ]");
    }
    lit_writer_writeformat(wr, "\n");
}

bool lit_handle_runtime_error(LitVM* vm, LitString* error_string)
{
    int i;
    int count;
    size_t length;
    char* start;
    char* buffer;
    const char* name;
    LitCallFrame* frame;
    LitFunction* function;
    LitChunk* chunk;
    LitValue error;
    LitFiber* fiber;
    LitFiber* caller;
    error = OBJECT_VALUE(error_string);
    fiber = vm->fiber;
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
        caller = fiber->parent;
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
    count = (int)fiber->frame_count - 1;
    length = snprintf(NULL, 0, "%s%s\n", COLOR_RED, error_string->chars);
    for(i = count; i >= 0; i--)
    {
        frame = &fiber->frames[i];
        function = frame->function;
        chunk = &function->chunk;
        name = function->name == NULL ? "unknown" : function->name->chars;

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
    buffer = (char*)malloc(length + 1);
    buffer[length] = '\0';
    start = buffer + sprintf(buffer, "%s%s\n", COLOR_RED, error_string->chars);
    for(i = count; i >= 0; i--)
    {
        frame = &fiber->frames[i];
        function = frame->function;
        chunk = &function->chunk;
        name = function->name == NULL ? "unknown" : function->name->chars;
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
    lit_state_raiseerror(vm->state, RUNTIME_ERROR, buffer);
    free(buffer);
    reset_stack(vm);
    return false;
}

bool lit_vruntime_error(LitVM* vm, const char* format, va_list args)
{
    size_t buffer_size;
    char* buffer;
    va_list args_copy;
    va_copy(args_copy, args);
    buffer_size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);
    buffer = (char*)malloc(buffer_size+1);
    vsnprintf(buffer, buffer_size, format, args);
    return lit_handle_runtime_error(vm, lit_string_take(vm->state, buffer, buffer_size, false));
}

bool lit_runtime_error(LitVM* vm, const char* format, ...)
{
    bool result;
    va_list args;
    va_start(args, format);
    result = lit_vruntime_error(vm, format, args);
    va_end(args);
    return result;
}

bool lit_runtime_error_exiting(LitVM* vm, const char* format, ...)
{
    bool result;
    va_list args;
    va_start(args, format);
    result = lit_vruntime_error(vm, format, args);
    va_end(args);
    lit_native_exit_jump();
    return result;
}

static bool call(LitVM* vm, LitFunction* function, LitClosure* closure, uint8_t arg_count)
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
            lit_state_pushroot(vm->state, (LitObject*)array);
            lit_vallist_ensuresize(vm->state, &array->list, vararg_count);
            lit_state_poproot(vm->state);
            for(i = 0; i < vararg_count; i++)
            {
                lit_vallist_set(&array->list, i, vm->fiber->stack_top[(int)i - (int)vararg_count]);
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
        lit_state_pushroot(vm->state, (LitObject*)array);
        lit_vallist_push(vm->state, &array->list, *(fiber->stack_top - 1));
        *(fiber->stack_top - 1) = OBJECT_VALUE(array);
        lit_state_poproot(vm->state);
    }
    return true;
}

static bool call_value(LitVM* vm, LitValue callee, uint8_t arg_count)
{
    size_t i;
    bool bres;
    LitNativeMethod* mthobj;
    LitFiber* fiber;
    LitClosure* closure;
    LitBoundMethod* bound_method;
    LitValue mthval;
    LitValue result;
    LitInstance* instance;
    LitClass* klass;
    (void)fiber;
    if(IS_OBJECT(callee))
    {
        if(lit_set_native_exit_jump())
        {
            return true;
        }
        switch(OBJECT_TYPE(callee))
        {
            case LITTYPE_FUNCTION:
                {
                    return call(vm, AS_FUNCTION(callee), NULL, arg_count);
                }
                break;
            case LITTYPE_CLOSURE:
                {
                    closure = AS_CLOSURE(callee);
                    return call(vm, closure->function, closure, arg_count);
                }
                break;
            case LITTYPE_NATIVE_FUNCTION:
                {
                    vm_pushgc(vm->state, false)
                    result = AS_NATIVE_FUNCTION(callee)->function(vm, arg_count, vm->fiber->stack_top - arg_count);
                    vm->fiber->stack_top -= arg_count + 1;
                    lit_push(vm, result);
                    vm_popgc(vm->state);
                    return false;
                }
                break;
            case LITTYPE_NATIVE_PRIMITIVE:
                {
                    vm_pushgc(vm->state, false)
                    fiber = vm->fiber;
                    bres = AS_NATIVE_PRIMITIVE(callee)->function(vm, arg_count, fiber->stack_top - arg_count);
                    if(bres)
                    {
                        fiber->stack_top -= arg_count;
                    }
                    vm_popgc(vm->state);
                    return bres;
                }
                break;
            case LITTYPE_NATIVE_METHOD:
                {
                    vm_pushgc(vm->state, false);
                    mthobj = AS_NATIVE_METHOD(callee);
                    fiber = vm->fiber;
                    result = mthobj->method(vm, *(vm->fiber->stack_top - arg_count - 1), arg_count, vm->fiber->stack_top - arg_count);
                    vm->fiber->stack_top -= arg_count + 1;
                    //if(!IS_NULL(result))
                    {
                        if(!vm->fiber->abort)
                        {
                            lit_push(vm, result);
                        }
                    }
                    vm_popgc(vm->state);
                    return false;
                }
                break;
            case LITTYPE_PRIMITIVE_METHOD:
                {
                    vm_pushgc(vm->state, false);
                    fiber = vm->fiber;
                    bres = AS_PRIMITIVE_METHOD(callee)->method(vm, *(fiber->stack_top - arg_count - 1), arg_count, fiber->stack_top - arg_count);
                    if(bres)
                    {
                        fiber->stack_top -= arg_count;
                    }
                    vm_popgc(vm->state);
                    return bres;
                }
                break;
            case LITTYPE_CLASS:
                {
                    klass = AS_CLASS(callee);
                    instance = lit_create_instance(vm->state, klass);
                    vm->fiber->stack_top[-arg_count - 1] = OBJECT_VALUE(instance);
                    if(klass->init_method != NULL)
                    {
                        return call_value(vm, OBJECT_VALUE(klass->init_method), arg_count);
                    }
                    // Remove the arguments, so that they don't mess up the stack
                    // (default constructor has no arguments)
                    for(i = 0; i < arg_count; i++)
                    {
                        lit_pop(vm);
                    }
                    return false;
                }
                break;
            case LITTYPE_BOUND_METHOD:
                {
                    bound_method = AS_BOUND_METHOD(callee);
                    mthval = bound_method->method;
                    if(IS_NATIVE_METHOD(mthval))
                    {
                        vm_pushgc(vm->state, false);
                        result = AS_NATIVE_METHOD(mthval)->method(vm, bound_method->receiver, arg_count, vm->fiber->stack_top - arg_count);
                        vm->fiber->stack_top -= arg_count + 1;
                        lit_push(vm, result);
                        vm_popgc(vm->state);
                        return false;
                    }
                    else if(IS_PRIMITIVE_METHOD(mthval))
                    {
                        fiber = vm->fiber;
                        vm_pushgc(vm->state, false);
                        if(AS_PRIMITIVE_METHOD(mthval)->method(vm, bound_method->receiver, arg_count, fiber->stack_top - arg_count))
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
                        return call(vm, AS_FUNCTION(mthval), NULL, arg_count);
                    }
                }
                break;
            default:
                {
                }
                break;

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
    LitUpvalue* upvalue;
    LitUpvalue* created_upvalue;
    LitUpvalue* previous_upvalue;
    previous_upvalue = NULL;
    upvalue = state->vm->fiber->open_upvalues;
    while(upvalue != NULL && upvalue->location > local)
    {
        previous_upvalue = upvalue;
        upvalue = upvalue->next;
    }
    if(upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }
    created_upvalue = lit_create_upvalue(state, local);
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

static void close_upvalues(LitVM* vm, const LitValue* last)
{
    LitFiber* fiber;
    LitUpvalue* upvalue;
    fiber = vm->fiber;
    while(fiber->open_upvalues != NULL && fiber->open_upvalues->location >= last)
    {
        upvalue = fiber->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        fiber->open_upvalues = upvalue->next;
    }
}

LitInterpretResult lit_interpret_module(LitState* state, LitModule* module)
{
    LitVM* vm;
    LitFiber* fiber;
    LitInterpretResult result;
    vm = state->vm;
    fiber = lit_create_fiber(state, module, module->main_function);
    vm->fiber = fiber;
    lit_push(vm, OBJECT_VALUE(module->main_function));
    result = lit_interpret_fiber(state, fiber);
    return result;
}


double lit_util_uinttofloat(unsigned int val);
unsigned int lit_util_floattouint(double val);
int lit_util_numbertoint32(double n);
int lit_util_doubletoint(double n);
unsigned int lit_util_numbertouint32(double n);

LitInterpretResult lit_interpret_fiber(LitState* state, LitFiber* fiber)
{
    bool found;
    size_t arg_count;
    size_t arindex;
    size_t i;
    uint16_t offset;
    uint8_t index;
    uint8_t is_local;
    uint8_t instruction;
    LitClass* instance_klass;
    LitClass* klassobj;
    LitClass* super_klass;
    LitClass* type;
    LitClosure* closure;
    LitFiber* parent;
    LitField* field;
    LitFunction* function;
    LitInstance* instobj;
    LitString* field_name;
    LitString* method_name;
    LitString* name;
    LitValue a;
    LitValue arg;
    LitValue b;
    LitValue getval;
    LitValue instval;
    LitValue klassval;
    LitValue object;
    LitValue operand;
    LitValue reference;
    LitValue result;
    LitValue setter;
    LitValue setval;
    LitValue slot;
    LitValue super;
    LitValue tmpval;
    LitValue value;
    LitValue* pval;
    LitValueList* values;
    LitExecState est;
    LitVM* vm;

    (void)instruction;
    vm = state->vm;
    vm_pushgc(state, true);
    vm->fiber = fiber;
    fiber->abort = false;
    est.frame = &fiber->frames[fiber->frame_count - 1];
    est.current_chunk = &est.frame->function->chunk;
    fiber->module = est.frame->function->module;
    est.ip = est.frame->ip;
    est.slots = est.frame->slots;
    est.privates = fiber->module->privates;
    est.upvalues = est.frame->closure == NULL ? NULL : est.frame->closure->upvalues;

    // Has to be inside of the function in order for goto to work
    #ifdef LIT_USE_COMPUTEDGOTO
        static void* dispatch_table[] =
        {
            #define OPCODE(name, effect) &&OP_##name,
            #include "opcodes.inc"
            #undef OPCODE
        };
    #endif
#ifdef LIT_TRACE_EXECUTION
    vm_traceframe(fiber);
#endif

    while(true)
    {
#ifdef LIT_TRACE_STACK
        lit_trace_vm_stack(vm);
#endif

#ifdef LIT_CHECK_STACK_SIZE
        if((fiber->stack_top - est.frame->slots) > fiber->stack_capacity)
        {
            vm_rterrorvarg("Fiber stack is not large enough (%i > %i)", (int)(fiber->stack_top - est.frame->slots),
                               fiber->stack_capacity);
        }
#endif

        #ifdef LIT_USE_COMPUTEDGOTO
            #ifdef LIT_TRACE_EXECUTION
                instruction = *est.ip++;
                lit_disassemble_instruction(state, est.current_chunk, (size_t)(est.ip - est.current_chunk->code - 1), NULL);
                goto* dispatch_table[instruction];
            #else
                goto* dispatch_table[*est.ip++];
            #endif
        #else
            instruction = *est.ip++;
            #ifdef LIT_TRACE_EXECUTION
                lit_disassemble_instruction(state, est.current_chunk, (size_t)(est.ip - est.current_chunk->code - 1), NULL);
            #endif
            switch(instruction)
        #endif
        /*
        * each op_case(...){...} *MUST* end with either break, return, or continue.
        * computationally, fall-throughs differ wildly between computed gotos or switch/case statements.
        * in computed gotos, a "fall-through" just executes the next block (since it's just a labelled block),
        * which may invalidate the stack, and while the same is technically true for switch/case, they
        * could end up executing completely unrelated instructions.
        * think, declaring a block for OP_BUILDHOUSE, and the next block is OP_SETHOUSEONFIRE.
        * an easy mistake to make, but crucial to check.
        */
        {
            op_case(POP)
            {
                vm_drop(fiber);
                continue;
            }
            op_case(RETURN)
            {
                result = vm_pop(fiber);
                close_upvalues(vm, est.slots);
                vm_writeframe(&est, est.ip);
                fiber->frame_count--;
                if(est.frame->return_to_c)
                {
                    est.frame->return_to_c = false;
                    fiber->module->return_value = result;
                    fiber->stack_top = est.frame->slots;
                    return (LitInterpretResult){ LITRESULT_OK, result };
                }
                if(fiber->frame_count == 0)
                {
                    fiber->module->return_value = result;
                    if(fiber->parent == NULL)
                    {
                        vm_drop(fiber);
                        state->allow_gc = was_allowed;
                        return (LitInterpretResult){ LITRESULT_OK, result };
                    }
                    arg_count = fiber->arg_count;
                    parent = fiber->parent;
                    fiber->parent = NULL;
                    vm->fiber = fiber = parent;
                    vm_readframe(fiber, &est);
                    vm_traceframe(fiber);
                    fiber->stack_top -= arg_count;
                    fiber->stack_top[-1] = result;
                    continue;
                }
                fiber->stack_top = est.frame->slots;
                if(est.frame->result_ignored)
                {
                    fiber->stack_top++;
                    est.frame->result_ignored = false;
                }
                else
                {
                    vm_push(fiber, result);
                }
                vm_readframe(fiber, &est);
                vm_traceframe(fiber);
                continue;
            }
            op_case(CONSTANT)
            {
                vm_push(fiber, vm_readconstant(&est));
                continue;
            }
            op_case(CONSTANT_LONG)
            {
                vm_push(fiber, vm_readconstantlong(&est));
                continue;
            }
            op_case(TRUE)
            {
                vm_push(fiber, TRUE_VALUE);
                continue;
            }
            op_case(FALSE)
            {
                vm_push(fiber, FALSE_VALUE);
                continue;
            }
            op_case(NULL)
            {
                vm_push(fiber, NULL_VALUE);
                continue;
            }
            op_case(ARRAY)
            {
                vm_push(fiber, OBJECT_VALUE(lit_create_array(state)));
                continue;
            }
            op_case(OBJECT)
            {
                // TODO: use object, or map for literal '{...}' constructs?
                // objects would be more general-purpose, but don't implement anything map-like.
                //vm_push(fiber, OBJECT_VALUE(lit_create_instance(state, state->objectvalue_class)));
                vm_push(fiber, OBJECT_VALUE(lit_create_map(state)));

                continue;
            }
            op_case(RANGE)
            {
                a = vm_pop(fiber);
                b = vm_pop(fiber);
                if(!IS_NUMBER(a) || !IS_NUMBER(b))
                {
                    vm_rterror("Range operands must be number");
                }
                vm_push(fiber, OBJECT_VALUE(lit_create_range(state, lit_value_to_number(a), lit_value_to_number(b))));
                continue;
            }
            op_case(NEGATE)
            {
                if(!IS_NUMBER(vm_peek(fiber, 0)))
                {
                    arg = vm_peek(fiber, 0);
                    // Don't even ask me why
                    // This doesn't kill our performance, since it's a error anyway
                    if(IS_STRING(arg) && strcmp(lit_as_cstring(arg), "muffin") == 0)
                    {
                        vm_rterror("Idk, can you negate a muffin?");
                    }
                    else
                    {
                        vm_rterror("Operand must be a number");
                    }
                }
                tmpval = lit_number_to_value(-lit_value_to_number(vm_pop(fiber)));
                vm_push(fiber, tmpval);
                continue;
            }
            op_case(NOT)
            {
                if(IS_INSTANCE(vm_peek(fiber, 0)))
                {
                    vm_writeframe(&est, est.ip);
                    vm_invoke_from_class(AS_INSTANCE(vm_peek(fiber, 0))->klass, CONST_STRING(state, "!"), 0, false, methods, false);
                    continue;
                }
                tmpval = BOOL_VALUE(lit_is_falsey(vm_pop(fiber)));
                vm_push(fiber, tmpval);
                continue;
            }
            op_case(BNOT)
            {
                if(!IS_NUMBER(vm_peek(fiber, 0)))
                {
                    vm_rterror("Operand must be a number");
                }
                tmpval = lit_number_to_value(~((int)lit_value_to_number(vm_pop(fiber))));
                vm_push(fiber, tmpval);
                continue;
            }
            op_case(ADD)
            {
                vm_binaryop(lit_number_to_value, +, "+");
                continue;
            }
            op_case(SUBTRACT)
            {
                vm_binaryop(lit_number_to_value, -, "-");
                continue;
            }
            op_case(MULTIPLY)
            {
                vm_binaryop(lit_number_to_value, *, "*");
                continue;
            }
            op_case(POWER)
            {
                a = vm_peek(fiber, 1);
                b = vm_peek(fiber, 0);
                if(IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_drop(fiber);
                    *(fiber->stack_top - 1) = (lit_number_to_value(pow(lit_value_to_number(a), lit_value_to_number(b))));
                    continue;
                }
                vm_invokemethod(a, "**", 1);
                continue;
            }
            op_case(DIVIDE)
            {
                vm_binaryop(lit_number_to_value, /, "/");
                continue;
            }
            op_case(FLOOR_DIVIDE)
            {
                a = vm_peek(fiber, 1);
                b = vm_peek(fiber, 0);
                if(IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_drop(fiber);
                    *(fiber->stack_top - 1) = (lit_number_to_value(floor(lit_value_to_number(a) / lit_value_to_number(b))));

                    continue;
                }

                vm_invokemethod(a, "#", 1);
                continue;
            }
            op_case(MOD)
            {
                a = vm_peek(fiber, 1);
                b = vm_peek(fiber, 0);
                if(IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_drop(fiber);
                    *(fiber->stack_top - 1) = lit_number_to_value(fmod(lit_value_to_number(a), lit_value_to_number(b)));
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
                
                LitValue a = vm_peek(fiber, 1);
                LitValue b = vm_peek(fiber, 0);
                if((!IS_NUMBER(a) && !IS_NULL(a)) || (!IS_NUMBER(b) && !IS_NULL(b)))
                {
                    vm_rterrorvarg("Operands of bitwise op %s must be two numbers, got %s and %s", "<<", lit_get_value_type(a), lit_get_value_type(b));
                }
                vm_drop(fiber);
                #if 0
                    *(fiber->stack_top - 1) = lit_number_to_value((int)lit_value_to_number(a) << (int)lit_value_to_number(b));

                #else
                    /*
                        double lit_util_uinttofloat(unsigned int val);
                        unsigned int lit_util_floattouint(double val);
                        int lit_util_numbertoint32(double n);
                        int lit_util_doubletoint(double n);
                        unsigned int lit_util_numbertouint32(double n);
                    */
                    
                    int uleft = lit_util_numbertoint32(lit_value_to_number(a));
                    unsigned int uright = lit_util_numbertouint32(lit_value_to_number(b));

                    *(fiber->stack_top - 1) = lit_number_to_value(uleft << (uright & 0x1F));
 

                #endif
                continue;
            }
            op_case(RSHIFT)
            {
                LitValue a = vm_peek(fiber, 1);
                LitValue b = vm_peek(fiber, 0);
                if((!IS_NUMBER(a) && !IS_NULL(a)) || (!IS_NUMBER(b) && !IS_NULL(b)))
                {
                    vm_rterrorvarg("Operands of bitwise op %s must be two numbers, got %s and %s", ">>", lit_get_value_type(a), lit_get_value_type(b));
                }
                vm_drop(fiber);

                /*
                    double lit_util_uinttofloat(unsigned int val);
                    unsigned int lit_util_floattouint(double val);
                    int lit_util_numbertoint32(double n);
                    int lit_util_doubletoint(double n);
                    unsigned int lit_util_numbertouint32(double n);
                */
                int uleft = lit_util_numbertoint32(lit_value_to_number(a));
                unsigned int uright = lit_util_numbertouint32(lit_value_to_number(b));

                *(fiber->stack_top - 1) = lit_number_to_value(uleft >> (uright & 0x1F));
                

                continue;
            }

            op_case(EQUAL)
            {
                /*
                if(IS_INSTANCE(vm_peek(fiber, 1)))
                {
                    vm_writeframe(&est, est.ip);
                    fprintf(stderr, "OP_EQUAL: trying to invoke '==' ...\n");
                    vm_invoke_from_class(AS_INSTANCE(vm_peek(fiber, 1))->klass, CONST_STRING(state, "=="), 1, false, methods, false);
                    continue;
                }
                a = vm_pop(fiber);
                b = vm_pop(fiber);
                vm_push(fiber, BOOL_VALUE(a == b));
                */
                vm_binaryop(lit_number_to_value, ==, "==");
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
                name = vm_readstringlong(&est);
                lit_table_set(state, &vm->globals->values, name, vm_peek(fiber, 0));
                continue;
            }

            op_case(GET_GLOBAL)
            {
                name = vm_readstringlong(&est);
                if(!lit_table_get(&vm->globals->values, name, &setval))
                {
                    vm_push(fiber, NULL_VALUE);
                }
                else
                {
                    vm_push(fiber, setval);
                }
                continue;
            }
            op_case(SET_LOCAL)
            {
                index = vm_readbyte(&est);
                est.slots[index] = vm_peek(fiber, 0);
                continue;
            }
            op_case(GET_LOCAL)
            {
                vm_push(fiber, est.slots[vm_readbyte(&est)]);
                continue;
            }
            op_case(SET_LOCAL_LONG)
            {
                index = vm_readshort(&est);
                est.slots[index] = vm_peek(fiber, 0);
                continue;
            }
            op_case(GET_LOCAL_LONG)
            {
                vm_push(fiber, est.slots[vm_readshort(&est)]);
                continue;
            }
            op_case(SET_PRIVATE)
            {
                index = vm_readbyte(&est);
                est.privates[index] = vm_peek(fiber, 0);
                continue;
            }
            op_case(GET_PRIVATE)
            {
                vm_push(fiber, est.privates[vm_readbyte(&est)]);
                continue;
            }
            op_case(SET_PRIVATE_LONG)
            {
                index = vm_readshort(&est);
                est.privates[index] = vm_peek(fiber, 0);
                continue;
            }
            op_case(GET_PRIVATE_LONG)
            {
                vm_push(fiber, est.privates[vm_readshort(&est)]);
                continue;
            }
            op_case(SET_UPVALUE)
            {
                index = vm_readbyte(&est);
                *est.upvalues[index]->location = vm_peek(fiber, 0);
                continue;
            }
            op_case(GET_UPVALUE)
            {
                vm_push(fiber, *est.upvalues[vm_readbyte(&est)]->location);
                continue;
            }

            op_case(JUMP_IF_FALSE)
            {
                offset = vm_readshort(&est);
                if(lit_is_falsey(vm_pop(fiber)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(JUMP_IF_NULL)
            {
                offset = vm_readshort(&est);
                if(IS_NULL(vm_peek(fiber, 0)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(JUMP_IF_NULL_POPPING)
            {
                offset = vm_readshort(&est);
                if(IS_NULL(vm_pop(fiber)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(JUMP)
            {
                offset = vm_readshort(&est);
                est.ip += offset;
                continue;
            }
            op_case(JUMP_BACK)
            {
                offset = vm_readshort(&est);
                est.ip -= offset;
                continue;
            }
            op_case(AND)
            {
                offset = vm_readshort(&est);
                if(lit_is_falsey(vm_peek(fiber, 0)))
                {
                    est.ip += offset;
                }
                else
                {
                    vm_drop(fiber);
                }
                continue;
            }
            op_case(OR)
            {
                offset = vm_readshort(&est);
                if(lit_is_falsey(vm_peek(fiber, 0)))
                {
                    vm_drop(fiber);
                }
                else
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(NULL_OR)
            {
                offset = vm_readshort(&est);
                if(IS_NULL(vm_peek(fiber, 0)))
                {
                    vm_drop(fiber);
                }
                else
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(CALL)
            {
                arg_count = vm_readbyte(&est);
                vm_writeframe(&est, est.ip);
                vm_callvalue(vm_peek(fiber, arg_count), arg_count);
                continue;
            }
            op_case(CLOSURE)
            {
                function = AS_FUNCTION(vm_readconstantlong(&est));
                closure = lit_create_closure(state, function);
                vm_push(fiber, OBJECT_VALUE(closure));
                for(i = 0; i < closure->upvalue_count; i++)
                {
                    is_local = vm_readbyte(&est);
                    index = vm_readbyte(&est);
                    if(is_local)
                    {
                        closure->upvalues[i] = capture_upvalue(state, est.frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = est.upvalues[index];
                    }
                }
                continue;
            }
            op_case(CLOSE_UPVALUE)
            {
                close_upvalues(vm, fiber->stack_top - 1);
                vm_drop(fiber);
                continue;
            }
            op_case(CLASS)
            {
                name = vm_readstringlong(&est);
                klassobj = lit_create_class(state, name);
                vm_push(fiber, OBJECT_VALUE(klassobj));
                klassobj->super = state->objectvalue_class;
                lit_table_add_all(state, &klassobj->super->methods, &klassobj->methods);
                lit_table_add_all(state, &klassobj->super->static_fields, &klassobj->static_fields);
                lit_table_set(state, &vm->globals->values, name, OBJECT_VALUE(klassobj));
                continue;
            }
            op_case(GET_FIELD)
            {
                object = vm_peek(fiber, 1);
                if(IS_NULL(object))
                {
                    vm_rterror("Attempt to index a null value");
                }
                name = lit_as_string(vm_peek(fiber, 0));
                if(IS_INSTANCE(object))
                {
                    instobj = AS_INSTANCE(object);

                    if(!lit_table_get(&instobj->fields, name, &getval))
                    {
                        if(lit_table_get(&instobj->klass->methods, name, &getval))
                        {
                            if(IS_FIELD(getval))
                            {
                                field = AS_FIELD(getval);
                                if(field->getter == NULL)
                                {
                                    vm_rterrorvarg("Class %s does not have a getter for the field %s",
                                                       instobj->klass->name->chars, name->chars);
                                }
                                vm_drop(fiber);
                                vm_writeframe(&est, est.ip);
                                vm_callvalue(OBJECT_VALUE(AS_FIELD(getval)->getter), 0);
                                vm_readframe(fiber, &est);
                                continue;
                            }
                            else
                            {
                                getval = OBJECT_VALUE(lit_create_bound_method(state, OBJECT_VALUE(instobj), getval));
                            }
                        }
                        else
                        {
                            getval = NULL_VALUE;
                        }
                    }
                }
                else if(IS_CLASS(object))
                {
                    klassobj = AS_CLASS(object);
                    if(lit_table_get(&klassobj->static_fields, name, &getval))
                    {
                        if(IS_NATIVE_METHOD(getval) || IS_PRIMITIVE_METHOD(getval))
                        {
                            getval = OBJECT_VALUE(lit_create_bound_method(state, OBJECT_VALUE(klassobj), getval));
                        }
                        else if(IS_FIELD(getval))
                        {
                            field = AS_FIELD(getval);
                            if(field->getter == NULL)
                            {
                                vm_rterrorvarg("Class %s does not have a getter for the field %s", klassobj->name->chars,
                                                   name->chars);
                            }
                            vm_drop(fiber);
                            vm_writeframe(&est, est.ip);
                            vm_callvalue(OBJECT_VALUE(field->getter), 0);
                            vm_readframe(fiber, &est);
                            continue;
                        }
                    }
                    else
                    {
                        getval = NULL_VALUE;
                    }
                }
                else
                {
                    klassobj = lit_state_getclassfor(state, object);
                    if(klassobj == NULL)
                    {
                        vm_rterror("GET_FIELD: only instances and classes have fields");
                    }
                    if(lit_table_get(&klassobj->methods, name, &getval))
                    {
                        if(IS_FIELD(getval))
                        {
                            field = AS_FIELD(getval);
                            if(field->getter == NULL)
                            {
                                vm_rterrorvarg("Class %s does not have a getter for the field %s", klassobj->name->chars,
                                                   name->chars);
                            }
                            vm_drop(fiber);
                            vm_writeframe(&est, est.ip);
                            vm_callvalue(OBJECT_VALUE(AS_FIELD(getval)->getter), 0);
                            vm_readframe(fiber, &est);
                            continue;
                        }
                        else if(IS_NATIVE_METHOD(getval) || IS_PRIMITIVE_METHOD(getval))
                        {
                            getval = OBJECT_VALUE(lit_create_bound_method(state, object, getval));
                        }
                    }
                    else
                    {
                        getval = NULL_VALUE;
                    }
                }
                vm_drop(fiber);// Pop field name
                fiber->stack_top[-1] = getval;
                continue;
            }
            op_case(SET_FIELD)
            {
                instval = vm_peek(fiber, 2);
                if(IS_NULL(instval))
                {
                    vm_rterror("Attempt to index a null value")
                }
                value = vm_peek(fiber, 1);
                field_name = lit_as_string(vm_peek(fiber, 0));
                if(IS_CLASS(instval))
                {
                    klassobj = AS_CLASS(instval);
                    if(lit_table_get(&klassobj->static_fields, field_name, &setter) && IS_FIELD(setter))
                    {
                        field = AS_FIELD(setter);
                        if(field->setter == NULL)
                        {
                            vm_rterrorvarg("Class %s does not have a setter for the field %s", klassobj->name->chars,
                                               field_name->chars);
                        }

                        vm_dropn(fiber, 2);
                        vm_push(fiber, value);
                        vm_writeframe(&est, est.ip);
                        vm_callvalue(OBJECT_VALUE(field->setter), 1);
                        vm_readframe(fiber, &est);
                        continue;
                    }
                    if(IS_NULL(value))
                    {
                        lit_table_delete(&klassobj->static_fields, field_name);
                    }
                    else
                    {
                        lit_table_set(state, &klassobj->static_fields, field_name, value);
                    }
                    vm_dropn(fiber, 2);// Pop field name and the value
                    fiber->stack_top[-1] = value;
                }
                else if(IS_INSTANCE(instval))
                {
                    instobj = AS_INSTANCE(instval);
                    if(lit_table_get(&instobj->klass->methods, field_name, &setter) && IS_FIELD(setter))
                    {
                        field = AS_FIELD(setter);
                        if(field->setter == NULL)
                        {
                            vm_rterrorvarg("Class %s does not have a setter for the field %s", instobj->klass->name->chars,
                                               field_name->chars);
                        }
                        vm_dropn(fiber, 2);
                        vm_push(fiber, value);
                        vm_writeframe(&est, est.ip);
                        vm_callvalue(OBJECT_VALUE(field->setter), 1);
                        vm_readframe(fiber, &est);
                        continue;
                    }
                    if(IS_NULL(value))
                    {
                        lit_table_delete(&instobj->fields, field_name);
                    }
                    else
                    {
                        lit_table_set(state, &instobj->fields, field_name, value);
                    }
                    vm_dropn(fiber, 2);// Pop field name and the value
                    fiber->stack_top[-1] = value;
                }
                else
                {
                    klassobj = lit_state_getclassfor(state, instval);
                    if(klassobj == NULL)
                    {
                        vm_rterror("SET_FIELD: only instances and classes have fields");
                    }
                    if(lit_table_get(&klassobj->methods, field_name, &setter) && IS_FIELD(setter))
                    {
                        field = AS_FIELD(setter);
                        if(field->setter == NULL)
                        {
                            vm_rterrorvarg("Class %s does not have a setter for the field %s", klassobj->name->chars,
                                               field_name->chars);
                        }
                        vm_dropn(fiber, 2);
                        vm_push(fiber, value);
                        vm_writeframe(&est, est.ip);
                        vm_callvalue(OBJECT_VALUE(field->setter), 1);
                        vm_readframe(fiber, &est);
                        continue;
                    }
                    else
                    {
                        vm_rterrorvarg("Class %s does not contain field %s", klassobj->name->chars, field_name->chars);
                    }
                }
                continue;
            }
            op_case(SUBSCRIPT_GET)
            {
                vm_invokemethod(vm_peek(fiber, 1), "[]", 1);
                continue;
            }
            op_case(SUBSCRIPT_SET)
            {
                vm_invokemethod(vm_peek(fiber, 2), "[]", 2);
                continue;
            }
            op_case(PUSH_ARRAY_ELEMENT)
            {
                values = &AS_ARRAY(vm_peek(fiber, 1))->list;
                arindex = lit_vallist_count(values);
                lit_vallist_ensuresize(state, values, arindex + 1);
                lit_vallist_set(values, arindex, vm_peek(fiber, 0));
                vm_drop(fiber);
                continue;
            }
            op_case(PUSH_OBJECT_FIELD)
            {
                operand = vm_peek(fiber, 2);
                if(IS_MAP(operand))
                {
                    lit_table_set(state, &AS_MAP(operand)->values, lit_as_string(vm_peek(fiber, 1)), vm_peek(fiber, 0));
                }
                else if(IS_INSTANCE(operand))
                {
                    lit_table_set(state, &AS_INSTANCE(operand)->fields, lit_as_string(vm_peek(fiber, 1)), vm_peek(fiber, 0));
                }
                else
                {
                    vm_rterrorvarg("Expected an object or a map as the operand, got %s", lit_get_value_type(operand));
                }
                vm_dropn(fiber, 2);
                continue;
            }
            op_case(STATIC_FIELD)
            {
                lit_table_set(state, &AS_CLASS(vm_peek(fiber, 1))->static_fields, vm_readstringlong(&est), vm_peek(fiber, 0));
                vm_drop(fiber);
                continue;
            }
            op_case(METHOD)
            {
                klassobj = AS_CLASS(vm_peek(fiber, 1));
                name = vm_readstringlong(&est);
                if((klassobj->init_method == NULL || (klassobj->super != NULL && klassobj->init_method == ((LitClass*)klassobj->super)->init_method))
                   && lit_string_getlength(name) == 11 && memcmp(name->chars, "constructor", 11) == 0)
                {
                    klassobj->init_method = lit_as_object(vm_peek(fiber, 0));
                }
                lit_table_set(state, &klassobj->methods, name, vm_peek(fiber, 0));
                vm_drop(fiber);
                continue;
            }
            op_case(DEFINE_FIELD)
            {
                lit_table_set(state, &AS_CLASS(vm_peek(fiber, 1))->methods, vm_readstringlong(&est), vm_peek(fiber, 0));
                vm_drop(fiber);
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
                arg_count = vm_readbyte(&est);
                method_name = vm_readstringlong(&est);
                klassobj = AS_CLASS(vm_pop(fiber));
                vm_writeframe(&est, est.ip);
                vm_invoke_from_class(klassobj, method_name, arg_count, true, methods, false);
                continue;
            }
            op_case(INVOKE_SUPER_IGNORING)
            {
                arg_count = vm_readbyte(&est);
                method_name = vm_readstringlong(&est);
                klassobj = AS_CLASS(vm_pop(fiber));
                vm_writeframe(&est, est.ip);
                vm_invoke_from_class(klassobj, method_name, arg_count, true, methods, true);
                continue;
            }
            op_case(GET_SUPER_METHOD)
            {
                method_name = vm_readstringlong(&est);
                klassobj = AS_CLASS(vm_pop(fiber));
                instval = vm_pop(fiber);
                if(lit_table_get(&klassobj->methods, method_name, &value))
                {
                    value = OBJECT_VALUE(lit_create_bound_method(state, instval, value));
                }
                else
                {
                    value = NULL_VALUE;
                }
                vm_push(fiber, value);
                continue;
            }
            op_case(INHERIT)
            {
                super = vm_peek(fiber, 1);
                if(!IS_CLASS(super))
                {
                    vm_rterror("Superclass must be a class");
                }
                klassobj = AS_CLASS(vm_peek(fiber, 0));
                super_klass = AS_CLASS(super);
                klassobj->super = super_klass;
                klassobj->init_method = super_klass->init_method;
                lit_table_add_all(state, &super_klass->methods, &klassobj->methods);
                lit_table_add_all(state, &klassobj->super->static_fields, &klassobj->static_fields);
                continue;
            }
            op_case(IS)
            {
                instval = vm_peek(fiber, 1);
                if(IS_NULL(instval))
                {
                    vm_dropn(fiber, 2);
                    vm_push(fiber, FALSE_VALUE);

                    continue;
                }
                instance_klass = lit_state_getclassfor(state, instval);
                klassval = vm_peek(fiber, 0);
                if(instance_klass == NULL || !IS_CLASS(klassval))
                {
                    vm_rterror("operands must be an instance and a class");
                }            
                type = AS_CLASS(klassval);
                found = false;
                while(instance_klass != NULL)
                {
                    if(instance_klass == type)
                    {
                        found = true;
                        break;
                    }
                    instance_klass = (LitClass*)instance_klass->super;
                }
                vm_dropn(fiber, 2);// Drop the instance and class
                vm_push(fiber, BOOL_VALUE(found));
                continue;
            }
            op_case(POP_LOCALS)
            {
                vm_dropn(fiber, vm_readshort(&est));
                continue;
            }
            op_case(VARARG)
            {
                slot = est.slots[vm_readbyte(&est)];
                if(!IS_ARRAY(slot))
                {
                    continue;
                }
                values = &AS_ARRAY(slot)->list;
                lit_ensure_fiber_stack(state, fiber, lit_vallist_count(values) + est.frame->function->max_slots + (int)(fiber->stack_top - fiber->stack));
                for(i = 0; i < lit_vallist_count(values); i++)
                {
                    vm_push(fiber, lit_vallist_get(values, i));
                }
                // Hot-bytecode patching, increment the amount of arguments to OP_CALL
                est.ip[1] = est.ip[1] + lit_vallist_count(values) - 1;
                continue;
            }

            op_case(REFERENCE_GLOBAL)
            {
                name = vm_readstringlong(&est);
                if(lit_table_get_slot(&vm->globals->values, name, &pval))
                {
                    vm_push(fiber, OBJECT_VALUE(lit_create_reference(state, pval)));
                }
                else
                {
                    vm_rterror("Attempt to reference a null value");
                }
                continue;
            }
            op_case(REFERENCE_PRIVATE)
            {
                vm_push(fiber, OBJECT_VALUE(lit_create_reference(state, &est.privates[vm_readshort(&est)])));
                continue;
            }
            op_case(REFERENCE_LOCAL)
            {
                vm_push(fiber, OBJECT_VALUE(lit_create_reference(state, &est.slots[vm_readshort(&est)])));
                continue;
            }
            op_case(REFERENCE_UPVALUE)
            {
                vm_push(fiber, OBJECT_VALUE(lit_create_reference(state, est.upvalues[vm_readbyte(&est)]->location)));
                continue;
            }
            op_case(REFERENCE_FIELD)
            {
                object = vm_peek(fiber, 1);
                if(IS_NULL(object))
                {
                    vm_rterror("Attempt to index a null value");
                }
                name = lit_as_string(vm_peek(fiber, 0));
                if(IS_INSTANCE(object))
                {
                    if(!lit_table_get_slot(&AS_INSTANCE(object)->fields, name, &pval))
                    {
                        vm_rterror("Attempt to reference a null value");
                    }
                }
                else
                {
                    lit_print_value(state, &state->debugwriter, object);
                    printf("\n");
                    vm_rterror("You can only reference fields of real instances");
                }
                vm_drop(fiber);// Pop field name
                fiber->stack_top[-1] = OBJECT_VALUE(lit_create_reference(state, pval));
                continue;
            }
            op_case(SET_REFERENCE)
            {
                reference = vm_pop(fiber);
                if(!IS_REFERENCE(reference))
                {
                    vm_rterror("Provided value is not a reference");
                }
                *AS_REFERENCE(reference)->slot = vm_peek(fiber, 0);
                continue;
            }
            vm_default()
            {
                vm_rterrorvarg("Unknown op code '%d'", *est.ip);
                break;
            }
        }
    }

    vm_returnerror();
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

