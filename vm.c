
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include "priv.h"

typedef struct LitExecState LitExecState;

enum
{
    RECOVER_RETURNOK,
    RECOVER_RETURNFAIL,
    RECOVER_NOTHING
};

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
#define LIT_USE_COMPUTEDGOTO
#define vm_traceframe
#define vm_returnerror
#define vm_default
#define op_case
#define vm_pushgc
#define vm_popgc
#define vm_recoverstate
#define vm_callvalue
#define vmexec_raiseerror
#define vmexec_raiseerrorfmt
#define vmexec_advinvokefromclass
#define vmexec_invokefromclass
#define vm_invokemethod
#define vm_binaryop
#define vm_bitwiseop
#define vm_invokeoperation
#define OPCODE

*/

static inline uint16_t vmexec_readshort(LitExecState *est);
static inline uint8_t vmexec_readbyte(LitExecState *est);
static inline LitValue vmexec_readconstant(LitExecState *est);
static inline LitValue vmexec_readconstantlong(LitExecState *est);
static inline LitString *vmexec_readstring(LitExecState *est);
static inline LitString *vmexec_readstringlong(LitExecState *est);
static inline void vmexec_push(LitFiber *fiber, LitValue v);
static inline LitValue vmexec_pop(LitFiber *fiber);
static inline void vmexec_drop(LitFiber *fiber);
static inline void vmexec_dropn(LitFiber *fiber, int amount);
static inline LitValue vmexec_peek(LitFiber *fiber, short distance);
static inline void vm_readframe(LitFiber *fiber, LitExecState *est);
static inline void vm_writeframe(LitExecState *est, uint8_t *ip);
static void reset_stack(LitVM *vm);
static void reset_vm(LitState *state, LitVM *vm);
void lit_init_vm(LitState *state, LitVM *vm);
void lit_free_vm(LitVM *vm);
void lit_trace_vm_stack(LitVM *vm, LitWriter *wr);
bool lit_handle_runtime_error(LitVM *vm, LitString *error_string);
bool lit_vruntime_error(LitVM *vm, const char *format, va_list args);
bool lit_runtime_error(LitVM *vm, const char *format, ...);
bool lit_runtime_error_exiting(LitVM *vm, const char *format, ...);
static bool call(LitVM *vm, LitFunction *function, LitClosure *closure, uint8_t argc);
static bool call_value(LitVM *vm, LitFiber* fiber, LitExecState* est, LitValue callee, uint8_t argc);
static LitUpvalue *capture_upvalue(LitState *state, LitValue *local);
static void close_upvalues(LitVM *vm, const LitValue *last);
LitInterpretResult lit_interpret_module(LitState *state, LitModule *module);
LitInterpretResult lit_interpret_fiber(LitState *state, LitFiber *fiber);
void lit_native_exit_jump(void);
bool lit_set_native_exit_jump(void);


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

#ifdef LIT_USE_COMPUTEDGOTO
    #define vm_default()
    #define op_case(name) \
        OP_##name:
#else
    #define vm_default() default:
    #define op_case(name) \
        case OP_##name:
#endif

#define vm_returnerror() \
    vm_popgc(state); \
    return (LitInterpretResult){ LITRESULT_RUNTIME_ERROR, NULL_VALUE };


#define vm_pushgc(state, allow) \
    bool was_allowed = state->allow_gc; \
    state->allow_gc = allow;

#define vm_popgc(state) \
    state->allow_gc = was_allowed;

/*
* can't be turned into a function because it is expected to return in
* lit_interpret_fiber.
* might be possible to wrap this by using an enum to specify
* if (and what) to return, but it'll be quite a bit of work to refactor.
* likewise, any macro that uses vm_recoverstate can't be turned into
* a function.
*/
#define vm_recoverstate(fiber, est) \
    vm_writeframe(&est, est.ip); \
    fiber = vm->fiber; \
    if(fiber == NULL) \
    { \
        return (LitInterpretResult){ LITRESULT_OK, vmexec_pop(fiber) }; \
    } \
    if(fiber->abort) \
    { \
        vm_returnerror(); \
    } \
    vm_readframe(fiber, &est); \
    vm_traceframe(fiber);


#define vm_callvalue(callee, argc) \
    if(call_value(vm, fiber, &est, callee, argc)) \
    { \
        vm_recoverstate(fiber, est); \
    }



#define vmexec_raiseerrorfmt(format, ...) \
    if(lit_runtime_error(vm, format, __VA_ARGS__)) \
    { \
        vm_recoverstate(fiber, est);  \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }

#define vmexec_raiseerror(format) \
    if(lit_runtime_error(vm, format)) \
    { \
        vm_recoverstate(fiber, est); \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }


#define vmexec_advinvokefromclass(zklass, mthname, argc, error, stat, ignoring, callee) \
    LitValue mthval; \
    if((lit_value_isinstance(callee) && (lit_table_get(&lit_value_asinstance(callee)->fields, mthname, &mthval))) \
       || lit_table_get(&zklass->stat, mthname, &mthval)) \
    { \
        if(ignoring) \
        { \
            if(call_value(vm, fiber, &est, mthval, argc)) \
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
            vm_callvalue(mthval, argc); \
        } \
    } \
    else \
    { \
        if(error) \
        { \
            vmexec_raiseerrorfmt("Attempt to call method '%s', that is not defined in class %s", mthname->chars, \
                               zklass->name->chars) \
        } \
    } \
    if(error) \
    { \
        continue; \
    }

// calls vm_recoverstate
#define vmexec_invokefromclass(klass, mthname, argc, error, stat, ignoring) \
    vmexec_advinvokefromclass(klass, mthname, argc, error, stat, ignoring, vmexec_peek(fiber, argc))

// calls vm_recoverstate
#define vm_invokemethod(instance, mthname, argc) \
    if(lit_value_isnull(instance)) \
    { \
        vmexec_raiseerrorfmt("invokemethod('%s'): instance is null", mthname); \
    } \
    LitClass* klass = lit_state_getclassfor(state, instance); \
    if(klass == NULL) \
    { \
        vmexec_raiseerrorfmt("invokemethod('%s'): only instances and classes have methods", mthname); \
    } \
    vm_writeframe(&est, est.ip); \
    vmexec_advinvokefromclass(klass, CONST_STRING(state, mthname), argc, true, methods, false, instance); \
    vm_readframe(fiber, &est)

#define vm_binaryop(type, op, op_string) \
    LitValue a = vmexec_peek(fiber, 1); \
    LitValue b = vmexec_peek(fiber, 0); \
    if(lit_value_isnumber(a)) \
    { \
        if(!lit_value_isnumber(b)) \
        { \
            if(!lit_value_isnull(b)) \
            { \
                vmexec_raiseerrorfmt("Attempt to use op %s with a number and a %s", op_string, lit_tostring_typename(b)); \
            } \
        } \
        vmexec_drop(fiber); \
        *(fiber->stack_top - 1) = (type(vm->state, lit_value_asnumber(a) op lit_value_asnumber(b))); \
        continue; \
    } \
    if(lit_value_isnull(a)) \
    { \
    /* vmexec_raiseerrorfmt("Attempt to use op %s on a null value", op_string); */ \
        vmexec_drop(fiber); \
        *(fiber->stack_top - 1) = TRUE_VALUE; \
    } \
    else \
    { \
        vm_invokemethod(a, op_string, 1); \
    }

#define vm_bitwiseop(op, op_string) \
    LitValue a = vmexec_peek(fiber, 1); \
    LitValue b = vmexec_peek(fiber, 0); \
    /* if((!lit_value_isnumber(a) && !lit_value_isnull(a)) || (!lit_value_isnumber(b) && !lit_value_isnull(b))) */ \
    if((!lit_value_isnumber(a) ) || (!lit_value_isnumber(b))) \
    { \
        vmexec_raiseerrorfmt("Operands of bitwise op %s must be two numbers, got %s and %s", op_string, \
                           lit_tostring_typename(a), lit_tostring_typename(b)); \
    } \
    vmexec_drop(fiber); \
    *(fiber->stack_top - 1) = (lit_value_numbertovalue(vm->state, (int)lit_value_asnumber(a) op(int) lit_value_asnumber(b)));

#define vm_invokeoperation(ignoring) \
    uint8_t argc = vmexec_readbyte(&est); \
    LitString* mthname = vmexec_readstringlong(&est); \
    LitValue receiver = vmexec_peek(fiber, argc); \
    if(lit_value_isnull(receiver)) \
    { \
        vmexec_raiseerror("Attempt to index a null value"); \
    } \
    vm_writeframe(&est, est.ip); \
    if(lit_value_isclass(receiver)) \
    { \
        vmexec_advinvokefromclass(lit_value_asclass(receiver), mthname, argc, true, static_fields, ignoring, receiver); \
        continue; \
    } \
    else if(lit_value_isinstance(receiver)) \
    { \
        LitInstance* instance = lit_value_asinstance(receiver); \
        LitValue value; \
        if(lit_table_get(&instance->fields, mthname, &value)) \
        { \
            fiber->stack_top[-argc - 1] = value; \
            vm_callvalue(value, argc); \
            vm_readframe(fiber, &est); \
            continue; \
        } \
        vmexec_advinvokefromclass(instance->klass, mthname, argc, true, methods, ignoring, receiver); \
    } \
    else \
    { \
        LitClass* type = lit_state_getclassfor(state, receiver); \
        if(type == NULL) \
        { \
            vmexec_raiseerror("invokeoperation: only instances and classes have methods"); \
        } \
        vmexec_advinvokefromclass(type, mthname, argc, true, methods, ignoring, receiver); \
    }

static jmp_buf jump_buffer;

static inline uint16_t vmexec_readshort(LitExecState* est)
{
    est->ip += 2u;
    return (uint16_t)((est->ip[-2] << 8u) | est->ip[-1]);
}

static inline uint8_t vmexec_readbyte(LitExecState* est)
{
    return (*est->ip++);
}

static inline LitValue vmexec_readconstant(LitExecState* est)
{
    return lit_vallist_get(&est->current_chunk->constants, vmexec_readbyte(est));
}

static inline LitValue vmexec_readconstantlong(LitExecState* est)
{
    return lit_vallist_get(&est->current_chunk->constants, vmexec_readshort(est));
}

static inline LitString* vmexec_readstring(LitExecState* est)
{
    return lit_value_asstring(vmexec_readconstant(est));
}

static inline LitString* vmexec_readstringlong(LitExecState* est)
{
    return lit_value_asstring(vmexec_readconstantlong(est));
}


static inline void vmexec_push(LitFiber* fiber, LitValue v)
{
    *fiber->stack_top++ = v;
}

static inline LitValue vmexec_pop(LitFiber* fiber)
{
    return *(--fiber->stack_top);
}

static inline void vmexec_drop(LitFiber* fiber)
{
    fiber->stack_top--;
}

static inline void vmexec_dropn(LitFiber* fiber, int amount)
{
    fiber->stack_top -= amount;
}

static inline LitValue vmexec_peek(LitFiber* fiber, short distance)
{
    int ofs;
    ofs = ((-1) - distance);
    if(distance < 0)
    {
        return NULL_VALUE;
    }
    return fiber->stack_top[ofs];
}

static inline void vm_readframe(LitFiber* fiber, LitExecState* est)
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
    lit_table_init(vm->state, &vm->strings);
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
    lit_table_destroy(vm->state, &vm->strings);
    lit_object_destroylistof(vm->state, vm->objects);
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
        lit_tostring_value(vm->state, wr, *slot);
        lit_writer_writeformat(wr, " ]");
    }
    lit_writer_writeformat(wr, "%s", COLOR_RESET);
    for(slot = top; slot < fiber->stack_top; slot++)
    {
        lit_writer_writeformat(wr, "[ ");
        lit_tostring_value(vm->state, wr, *slot);
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
    error = lit_value_objectvalue(error_string);
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

static bool call(LitVM* vm, LitFunction* function, LitClosure* closure, uint8_t argc)
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
        fiber->frames = (LitCallFrame*)lit_gcmem_memrealloc(vm->state, fiber->frames, osize, newsize);
        fiber->frame_capacity = newcapacity;
    }

    function_arg_count = function->arg_count;
    lit_ensure_fiber_stack(vm->state, fiber, function->max_slots + (int)(fiber->stack_top - fiber->stack));
    frame = &fiber->frames[fiber->frame_count++];
    frame->function = function;
    frame->closure = closure;
    frame->ip = function->chunk.code;
    frame->slots = fiber->stack_top - argc - 1;
    frame->result_ignored = false;
    frame->return_to_c = false;
    if(argc != function_arg_count)
    {
        vararg = function->vararg;
        if(argc < function_arg_count)
        {
            amount = (int)function_arg_count - argc - (vararg ? 1 : 0);
            for(i = 0; i < amount; i++)
            {
                lit_vm_push(vm, NULL_VALUE);
            }
            if(vararg)
            {
                lit_vm_push(vm, lit_value_objectvalue(lit_create_array(vm->state)));
            }
        }
        else if(function->vararg)
        {
            array = lit_create_array(vm->state);
            vararg_count = argc - function_arg_count + 1;
            lit_state_pushroot(vm->state, (LitObject*)array);
            lit_vallist_ensuresize(vm->state, &array->list, vararg_count);
            lit_state_poproot(vm->state);
            for(i = 0; i < vararg_count; i++)
            {
                lit_vallist_set(&array->list, i, vm->fiber->stack_top[(int)i - (int)vararg_count]);
            }
            vm->fiber->stack_top -= vararg_count;
            lit_vm_push(vm, lit_value_objectvalue(array));
        }
        else
        {
            vm->fiber->stack_top -= (argc - function_arg_count);
        }
    }
    else if(function->vararg)
    {
        array = lit_create_array(vm->state);
        vararg_count = argc - function_arg_count + 1;
        lit_state_pushroot(vm->state, (LitObject*)array);
        lit_vallist_push(vm->state, &array->list, *(fiber->stack_top - 1));
        *(fiber->stack_top - 1) = lit_value_objectvalue(array);
        lit_state_poproot(vm->state);
    }
    return true;
}

const char* find_funcname_fromvalue(LitVM* vm, LitExecState* est, LitValue v)
{
    LitValue vn;
    vn = lit_get_function_name(vm, v);
    if(!lit_value_isnull(vn))
    {
        return lit_value_ascstring(vn);
    }
    return "unknown";
}

static bool call_value(LitVM* vm, LitFiber* fiber, LitExecState* est, LitValue callee, uint8_t argc)
{
    size_t i;
    bool bres;
    const char* fname;
    LitValue mthval;
    LitValue result;
    LitValue fromval;
    LitNativeMethod* mthobj;
    LitFiber* valfiber;
    LitClosure* closure;
    LitBoundMethod* bound_method;
    LitInstance* instance;
    LitClass* klass;
    (void)valfiber;
    if(lit_value_isobject(callee))
    {
        if(lit_set_native_exit_jump())
        {
            return true;
        }
        switch(lit_value_type(callee))
        {
            case LITTYPE_FUNCTION:
                {
                    return call(vm, lit_value_asfunction(callee), NULL, argc);
                }
                break;
            case LITTYPE_CLOSURE:
                {
                    closure = lit_value_asclosure(callee);
                    return call(vm, closure->function, closure, argc);
                }
                break;
            case LITTYPE_NATIVE_FUNCTION:
                {
                    vm_pushgc(vm->state, false)
                    result = lit_value_asnativefunction(callee)->function(vm, argc, vm->fiber->stack_top - argc);
                    vm->fiber->stack_top -= argc + 1;
                    lit_vm_push(vm, result);
                    vm_popgc(vm->state);
                    return false;
                }
                break;
            case LITTYPE_NATIVE_PRIMITIVE:
                {
                    vm_pushgc(vm->state, false)
                    fiber = vm->fiber;
                    bres = lit_value_asnativeprimitive(callee)->function(vm, argc, fiber->stack_top - argc);
                    if(bres)
                    {
                        fiber->stack_top -= argc;
                    }
                    vm_popgc(vm->state);
                    return bres;
                }
                break;
            case LITTYPE_NATIVE_METHOD:
                {
                    vm_pushgc(vm->state, false);
                    mthobj = lit_value_asnativemethod(callee);
                    fiber = vm->fiber;
                    result = mthobj->method(vm, *(vm->fiber->stack_top - argc - 1), argc, vm->fiber->stack_top - argc);
                    vm->fiber->stack_top -= argc + 1;
                    //if(!lit_value_isnull(result))
                    {
                        if(!vm->fiber->abort)
                        {
                            lit_vm_push(vm, result);
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
                    bres = lit_value_asprimitivemethod(callee)->method(vm, *(fiber->stack_top - argc - 1), argc, fiber->stack_top - argc);
                    if(bres)
                    {
                        fiber->stack_top -= argc;
                    }
                    vm_popgc(vm->state);
                    return bres;
                }
                break;
            case LITTYPE_CLASS:
                {
                    klass = lit_value_asclass(callee);
                    instance = lit_create_instance(vm->state, klass);
                    vm->fiber->stack_top[-argc - 1] = lit_value_objectvalue(instance);
                    if(klass->init_method != NULL)
                    {
                        return call_value(vm, fiber, est, lit_value_objectvalue(klass->init_method), argc);
                    }
                    // Remove the arguments, so that they don't mess up the stack
                    // (default constructor has no arguments)
                    for(i = 0; i < argc; i++)
                    {
                        lit_vm_pop(vm);
                    }
                    return false;
                }
                break;
            case LITTYPE_BOUND_METHOD:
                {
                    bound_method = lit_value_asboundmethod(callee);
                    mthval = bound_method->method;
                    if(lit_value_isnatmethod(mthval))
                    {
                        vm_pushgc(vm->state, false);
                        result = lit_value_asnativemethod(mthval)->method(vm, bound_method->receiver, argc, vm->fiber->stack_top - argc);
                        vm->fiber->stack_top -= argc + 1;
                        lit_vm_push(vm, result);
                        vm_popgc(vm->state);
                        return false;
                    }
                    else if(lit_value_isprimmethod(mthval))
                    {
                        fiber = vm->fiber;
                        vm_pushgc(vm->state, false);
                        if(lit_value_asprimitivemethod(mthval)->method(vm, bound_method->receiver, argc, fiber->stack_top - argc))
                        {
                            fiber->stack_top -= argc;
                            return true;
                        }
                        vm_popgc(vm->state);
                        return false;
                    }
                    else
                    {
                        vm->fiber->stack_top[-argc - 1] = bound_method->receiver;
                        return call(vm, lit_value_asfunction(mthval), NULL, argc);
                    }
                }
                break;
            default:
                {
                }
                break;

        }
    }
    fromval = callee;
    /*
    LitValue* slots;
    LitValue* privates;
    LitUpvalue** upvalues;
    uint8_t* ip;
    LitCallFrame* frame;
    LitChunk* current_chunk;
    */
    if(lit_value_isnull(fromval))
    {
        /*
        if(est->frame->function != NULL)
        {
            fromval =  lit_value_objectvalue(est->frame->function);
        }
        */
        //fromval = est->slots[0];
        fromval = vmexec_peek(fiber, 0);
    }
    fname = "unknown";
    fprintf(stderr, "fromval type=%d %s\n", lit_value_type(fromval), lit_tostring_typename(fromval));
    if(lit_value_isfunction(fromval))
    {
        fname = find_funcname_fromvalue(vm, est, fromval);
    }
    else if(lit_value_isstring(fromval))
    {
        fname = lit_value_ascstring(fromval);
    }
    if(lit_value_isnull(callee))
    {
        lit_runtime_error(vm, "attempt to call '%s' which is null", fname);
    }
    else
    {
        lit_runtime_error(vm, "attempt to call '%s' which is neither function nor class, but is %s", fname, lit_tostring_typename(callee));
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
    lit_vm_push(vm, lit_value_objectvalue(module->main_function));
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
    size_t argc;
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
    LitString* mthname;
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
    LitValue peeked;
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
            vmexec_raiseerrorfmt("Fiber stack is not large enough (%i > %i)", (int)(fiber->stack_top - est.frame->slots),
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
                vmexec_drop(fiber);
                continue;
            }
            op_case(RETURN)
            {
                result = vmexec_pop(fiber);
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
                        vmexec_drop(fiber);
                        state->allow_gc = was_allowed;
                        return (LitInterpretResult){ LITRESULT_OK, result };
                    }
                    argc = fiber->arg_count;
                    parent = fiber->parent;
                    fiber->parent = NULL;
                    vm->fiber = fiber = parent;
                    vm_readframe(fiber, &est);
                    vm_traceframe(fiber);
                    fiber->stack_top -= argc;
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
                    vmexec_push(fiber, result);
                }
                vm_readframe(fiber, &est);
                vm_traceframe(fiber);
                continue;
            }
            op_case(CONSTANT)
            {
                vmexec_push(fiber, vmexec_readconstant(&est));
                continue;
            }
            op_case(CONSTANT_LONG)
            {
                vmexec_push(fiber, vmexec_readconstantlong(&est));
                continue;
            }
            op_case(TRUE)
            {
                vmexec_push(fiber, TRUE_VALUE);
                continue;
            }
            op_case(FALSE)
            {
                vmexec_push(fiber, FALSE_VALUE);
                continue;
            }
            op_case(NULL)
            {
                vmexec_push(fiber, NULL_VALUE);
                continue;
            }
            op_case(ARRAY)
            {
                vmexec_push(fiber, lit_value_objectvalue(lit_create_array(state)));
                continue;
            }
            op_case(OBJECT)
            {
                // TODO: use object, or map for literal '{...}' constructs?
                // objects would be more general-purpose, but don't implement anything map-like.
                //vmexec_push(fiber, lit_value_objectvalue(lit_create_instance(state, state->objectvalue_class)));
                vmexec_push(fiber, lit_value_objectvalue(lit_create_map(state)));

                continue;
            }
            op_case(RANGE)
            {
                a = vmexec_pop(fiber);
                b = vmexec_pop(fiber);
                if(!lit_value_isnumber(a) || !lit_value_isnumber(b))
                {
                    vmexec_raiseerror("Range operands must be number");
                }
                vmexec_push(fiber, lit_value_objectvalue(lit_create_range(state, lit_value_asnumber(a), lit_value_asnumber(b))));
                continue;
            }
            op_case(NEGATE)
            {
                if(!lit_value_isnumber(vmexec_peek(fiber, 0)))
                {
                    arg = vmexec_peek(fiber, 0);
                    // Don't even ask me why
                    // This doesn't kill our performance, since it's a error anyway
                    if(lit_value_isstring(arg) && strcmp(lit_value_ascstring(arg), "muffin") == 0)
                    {
                        vmexec_raiseerror("Idk, can you negate a muffin?");
                    }
                    else
                    {
                        vmexec_raiseerror("Operand must be a number");
                    }
                }
                tmpval = lit_value_numbertovalue(vm->state, -lit_value_asnumber(vmexec_pop(fiber)));
                vmexec_push(fiber, tmpval);
                continue;
            }
            op_case(NOT)
            {
                if(lit_value_isinstance(vmexec_peek(fiber, 0)))
                {
                    vm_writeframe(&est, est.ip);
                    vmexec_invokefromclass(lit_value_asinstance(vmexec_peek(fiber, 0))->klass, CONST_STRING(state, "!"), 0, false, methods, false);
                    continue;
                }
                tmpval = lit_bool_to_value(vm->state, lit_value_isfalsey(vmexec_pop(fiber)));
                vmexec_push(fiber, tmpval);
                continue;
            }
            op_case(BNOT)
            {
                if(!lit_value_isnumber(vmexec_peek(fiber, 0)))
                {
                    vmexec_raiseerror("Operand must be a number");
                }
                tmpval = lit_value_numbertovalue(vm->state, ~((int)lit_value_asnumber(vmexec_pop(fiber))));
                vmexec_push(fiber, tmpval);
                continue;
            }
            op_case(ADD)
            {
                vm_binaryop(lit_value_numbertovalue, +, "+");
                continue;
            }
            op_case(SUBTRACT)
            {
                vm_binaryop(lit_value_numbertovalue, -, "-");
                continue;
            }
            op_case(MULTIPLY)
            {
                vm_binaryop(lit_value_numbertovalue, *, "*");
                continue;
            }
            op_case(POWER)
            {
                a = vmexec_peek(fiber, 1);
                b = vmexec_peek(fiber, 0);
                if(lit_value_isnumber(a) && lit_value_isnumber(b))
                {
                    vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = (lit_value_numbertovalue(vm->state, pow(lit_value_asnumber(a), lit_value_asnumber(b))));
                    continue;
                }
                vm_invokemethod(a, "**", 1);
                continue;
            }
            op_case(DIVIDE)
            {
                vm_binaryop(lit_value_numbertovalue, /, "/");
                continue;
            }
            op_case(FLOOR_DIVIDE)
            {
                a = vmexec_peek(fiber, 1);
                b = vmexec_peek(fiber, 0);
                if(lit_value_isnumber(a) && lit_value_isnumber(b))
                {
                    vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = (lit_value_numbertovalue(vm->state, floor(lit_value_asnumber(a) / lit_value_asnumber(b))));

                    continue;
                }

                vm_invokemethod(a, "#", 1);
                continue;
            }
            op_case(MOD)
            {
                a = vmexec_peek(fiber, 1);
                b = vmexec_peek(fiber, 0);
                if(lit_value_isnumber(a) && lit_value_isnumber(b))
                {
                    vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = lit_value_numbertovalue(vm->state, fmod(lit_value_asnumber(a), lit_value_asnumber(b)));
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
                
                LitValue a = vmexec_peek(fiber, 1);
                LitValue b = vmexec_peek(fiber, 0);
                if((!lit_value_isnumber(a) && !lit_value_isnull(a)) || (!lit_value_isnumber(b) && !lit_value_isnull(b)))
                {
                    vmexec_raiseerrorfmt("Operands of bitwise op %s must be two numbers, got %s and %s", "<<", lit_tostring_typename(a), lit_tostring_typename(b));
                }
                vmexec_drop(fiber);
                #if 0
                    *(fiber->stack_top - 1) = lit_value_numbertovalue(vm->state, (int)lit_value_asnumber(a) << (int)lit_value_asnumber(b));

                #else
                    /*
                        double lit_util_uinttofloat(unsigned int val);
                        unsigned int lit_util_floattouint(double val);
                        int lit_util_numbertoint32(double n);
                        int lit_util_doubletoint(double n);
                        unsigned int lit_util_numbertouint32(double n);
                    */
                    
                    int uleft = lit_util_numbertoint32(lit_value_asnumber(a));
                    unsigned int uright = lit_util_numbertouint32(lit_value_asnumber(b));

                    *(fiber->stack_top - 1) = lit_value_numbertovalue(vm->state, uleft << (uright & 0x1F));
 

                #endif
                continue;
            }
            op_case(RSHIFT)
            {
                LitValue a = vmexec_peek(fiber, 1);
                LitValue b = vmexec_peek(fiber, 0);
                if((!lit_value_isnumber(a) && !lit_value_isnull(a)) || (!lit_value_isnumber(b) && !lit_value_isnull(b)))
                {
                    vmexec_raiseerrorfmt("Operands of bitwise op %s must be two numbers, got %s and %s", ">>", lit_tostring_typename(a), lit_tostring_typename(b));
                }
                vmexec_drop(fiber);

                /*
                    double lit_util_uinttofloat(unsigned int val);
                    unsigned int lit_util_floattouint(double val);
                    int lit_util_numbertoint32(double n);
                    int lit_util_doubletoint(double n);
                    unsigned int lit_util_numbertouint32(double n);
                */
                int uleft = lit_util_numbertoint32(lit_value_asnumber(a));
                unsigned int uright = lit_util_numbertouint32(lit_value_asnumber(b));

                *(fiber->stack_top - 1) = lit_value_numbertovalue(vm->state, uleft >> (uright & 0x1F));
                

                continue;
            }

            op_case(EQUAL)
            {
                /*
                if(lit_value_isinstance(vmexec_peek(fiber, 1)))
                {
                    vm_writeframe(&est, est.ip);
                    fprintf(stderr, "OP_EQUAL: trying to invoke '==' ...\n");
                    vmexec_invokefromclass(lit_value_asinstance(vmexec_peek(fiber, 1))->klass, CONST_STRING(state, "=="), 1, false, methods, false);
                    continue;
                }
                a = vmexec_pop(fiber);
                b = vmexec_pop(fiber);
                vmexec_push(fiber, lit_bool_to_value(vm->state, a == b));
                */
                vm_binaryop(lit_value_numbertovalue, ==, "==");
                continue;
            }

            op_case(GREATER)
            {
                vm_binaryop(lit_bool_to_value, >, ">");
                continue;
            }
            op_case(GREATER_EQUAL)
            {
                vm_binaryop(lit_bool_to_value, >=, ">=");
                continue;
            }
            op_case(LESS)
            {
                vm_binaryop(lit_bool_to_value, <, "<");
                continue;
            }
            op_case(LESS_EQUAL)
            {
                vm_binaryop(lit_bool_to_value, <=, "<=");
                continue;
            }

            op_case(SET_GLOBAL)
            {
                name = vmexec_readstringlong(&est);
                lit_table_set(state, &vm->globals->values, name, vmexec_peek(fiber, 0));
                continue;
            }

            op_case(GET_GLOBAL)
            {
                name = vmexec_readstringlong(&est);
                if(!lit_table_get(&vm->globals->values, name, &setval))
                {
                    vmexec_push(fiber, NULL_VALUE);
                }
                else
                {
                    vmexec_push(fiber, setval);
                }
                continue;
            }
            op_case(SET_LOCAL)
            {
                index = vmexec_readbyte(&est);
                est.slots[index] = vmexec_peek(fiber, 0);
                continue;
            }
            op_case(GET_LOCAL)
            {
                vmexec_push(fiber, est.slots[vmexec_readbyte(&est)]);
                continue;
            }
            op_case(SET_LOCAL_LONG)
            {
                index = vmexec_readshort(&est);
                est.slots[index] = vmexec_peek(fiber, 0);
                continue;
            }
            op_case(GET_LOCAL_LONG)
            {
                vmexec_push(fiber, est.slots[vmexec_readshort(&est)]);
                continue;
            }
            op_case(SET_PRIVATE)
            {
                index = vmexec_readbyte(&est);
                est.privates[index] = vmexec_peek(fiber, 0);
                continue;
            }
            op_case(GET_PRIVATE)
            {
                vmexec_push(fiber, est.privates[vmexec_readbyte(&est)]);
                continue;
            }
            op_case(SET_PRIVATE_LONG)
            {
                index = vmexec_readshort(&est);
                est.privates[index] = vmexec_peek(fiber, 0);
                continue;
            }
            op_case(GET_PRIVATE_LONG)
            {
                vmexec_push(fiber, est.privates[vmexec_readshort(&est)]);
                continue;
            }
            op_case(SET_UPVALUE)
            {
                index = vmexec_readbyte(&est);
                *est.upvalues[index]->location = vmexec_peek(fiber, 0);
                continue;
            }
            op_case(GET_UPVALUE)
            {
                vmexec_push(fiber, *est.upvalues[vmexec_readbyte(&est)]->location);
                continue;
            }

            op_case(JUMP_IF_FALSE)
            {
                offset = vmexec_readshort(&est);
                if(lit_value_isfalsey(vmexec_pop(fiber)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(JUMP_IF_NULL)
            {
                offset = vmexec_readshort(&est);
                if(lit_value_isnull(vmexec_peek(fiber, 0)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(JUMP_IF_NULL_POPPING)
            {
                offset = vmexec_readshort(&est);
                if(lit_value_isnull(vmexec_pop(fiber)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(JUMP)
            {
                offset = vmexec_readshort(&est);
                est.ip += offset;
                continue;
            }
            op_case(JUMP_BACK)
            {
                offset = vmexec_readshort(&est);
                est.ip -= offset;
                continue;
            }
            op_case(AND)
            {
                offset = vmexec_readshort(&est);
                if(lit_value_isfalsey(vmexec_peek(fiber, 0)))
                {
                    est.ip += offset;
                }
                else
                {
                    vmexec_drop(fiber);
                }
                continue;
            }
            op_case(OR)
            {
                offset = vmexec_readshort(&est);
                if(lit_value_isfalsey(vmexec_peek(fiber, 0)))
                {
                    vmexec_drop(fiber);
                }
                else
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(NULL_OR)
            {
                offset = vmexec_readshort(&est);
                if(lit_value_isnull(vmexec_peek(fiber, 0)))
                {
                    vmexec_drop(fiber);
                }
                else
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(CALL)
            {
                argc = vmexec_readbyte(&est);
                vm_writeframe(&est, est.ip);
                peeked = vmexec_peek(fiber, argc);
                vm_callvalue(peeked, argc);
                continue;
            }
            op_case(CLOSURE)
            {
                function = lit_value_asfunction(vmexec_readconstantlong(&est));
                closure = lit_create_closure(state, function);
                vmexec_push(fiber, lit_value_objectvalue(closure));
                for(i = 0; i < closure->upvalue_count; i++)
                {
                    is_local = vmexec_readbyte(&est);
                    index = vmexec_readbyte(&est);
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
                vmexec_drop(fiber);
                continue;
            }
            op_case(CLASS)
            {
                name = vmexec_readstringlong(&est);
                klassobj = lit_create_class(state, name);
                vmexec_push(fiber, lit_value_objectvalue(klassobj));
                klassobj->super = state->objectvalue_class;
                lit_table_add_all(state, &klassobj->super->methods, &klassobj->methods);
                lit_table_add_all(state, &klassobj->super->static_fields, &klassobj->static_fields);
                lit_table_set(state, &vm->globals->values, name, lit_value_objectvalue(klassobj));
                continue;
            }
            op_case(GET_FIELD)
            {
                object = vmexec_peek(fiber, 1);
                if(lit_value_isnull(object))
                {
                    vmexec_raiseerror("Attempt to index a null value");
                }
                name = lit_value_asstring(vmexec_peek(fiber, 0));
                if(lit_value_isinstance(object))
                {
                    instobj = lit_value_asinstance(object);

                    if(!lit_table_get(&instobj->fields, name, &getval))
                    {
                        if(lit_table_get(&instobj->klass->methods, name, &getval))
                        {
                            if(lit_value_isfield(getval))
                            {
                                field = lit_value_asfield(getval);
                                if(field->getter == NULL)
                                {
                                    vmexec_raiseerrorfmt("Class %s does not have a getter for the field %s",
                                                       instobj->klass->name->chars, name->chars);
                                }
                                vmexec_drop(fiber);
                                vm_writeframe(&est, est.ip);
                                LitField* field = lit_value_asfield(getval);
                                tmpval =lit_value_objectvalue(field->getter);
                                vm_callvalue(tmpval, 0);
                                vm_readframe(fiber, &est);
                                continue;
                            }
                            else
                            {
                                getval = lit_value_objectvalue(lit_create_bound_method(state, lit_value_objectvalue(instobj), getval));
                            }
                        }
                        else
                        {
                            getval = NULL_VALUE;
                        }
                    }
                }
                else if(lit_value_isclass(object))
                {
                    klassobj = lit_value_asclass(object);
                    if(lit_table_get(&klassobj->static_fields, name, &getval))
                    {
                        if(lit_value_isnatmethod(getval) || lit_value_isprimmethod(getval))
                        {
                            getval = lit_value_objectvalue(lit_create_bound_method(state, lit_value_objectvalue(klassobj), getval));
                        }
                        else if(lit_value_isfield(getval))
                        {
                            field = lit_value_asfield(getval);
                            if(field->getter == NULL)
                            {
                                vmexec_raiseerrorfmt("Class %s does not have a getter for the field %s", klassobj->name->chars,
                                                   name->chars);
                            }
                            vmexec_drop(fiber);
                            vm_writeframe(&est, est.ip);
                            tmpval = lit_value_objectvalue(field->getter);
                            vm_callvalue(tmpval, 0);
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
                        vmexec_raiseerror("GET_FIELD: only instances and classes have fields");
                    }
                    if(lit_table_get(&klassobj->methods, name, &getval))
                    {
                        if(lit_value_isfield(getval))
                        {
                            field = lit_value_asfield(getval);
                            if(field->getter == NULL)
                            {
                                vmexec_raiseerrorfmt("Class %s does not have a getter for the field %s", klassobj->name->chars,
                                                   name->chars);
                            }
                            vmexec_drop(fiber);
                            vm_writeframe(&est, est.ip);
                            tmpval = lit_value_objectvalue(lit_value_asfield(getval)->getter);
                            vm_callvalue(tmpval, 0);
                            vm_readframe(fiber, &est);
                            continue;
                        }
                        else if(lit_value_isnatmethod(getval) || lit_value_isprimmethod(getval))
                        {
                            getval = lit_value_objectvalue(lit_create_bound_method(state, object, getval));
                        }
                    }
                    else
                    {
                        getval = NULL_VALUE;
                    }
                }
                vmexec_drop(fiber);// Pop field name
                fiber->stack_top[-1] = getval;
                continue;
            }
            op_case(SET_FIELD)
            {
                instval = vmexec_peek(fiber, 2);
                if(lit_value_isnull(instval))
                {
                    vmexec_raiseerror("Attempt to index a null value")
                }
                value = vmexec_peek(fiber, 1);
                field_name = lit_value_asstring(vmexec_peek(fiber, 0));
                if(lit_value_isclass(instval))
                {
                    klassobj = lit_value_asclass(instval);
                    if(lit_table_get(&klassobj->static_fields, field_name, &setter) && lit_value_isfield(setter))
                    {
                        field = lit_value_asfield(setter);
                        if(field->setter == NULL)
                        {
                            vmexec_raiseerrorfmt("Class %s does not have a setter for the field %s", klassobj->name->chars,
                                               field_name->chars);
                        }

                        vmexec_dropn(fiber, 2);
                        vmexec_push(fiber, value);
                        vm_writeframe(&est, est.ip);
                        tmpval = lit_value_objectvalue(field->setter);
                        vm_callvalue(tmpval, 1);
                        vm_readframe(fiber, &est);
                        continue;
                    }
                    if(lit_value_isnull(value))
                    {
                        lit_table_delete(&klassobj->static_fields, field_name);
                    }
                    else
                    {
                        lit_table_set(state, &klassobj->static_fields, field_name, value);
                    }
                    vmexec_dropn(fiber, 2);// Pop field name and the value
                    fiber->stack_top[-1] = value;
                }
                else if(lit_value_isinstance(instval))
                {
                    instobj = lit_value_asinstance(instval);
                    if(lit_table_get(&instobj->klass->methods, field_name, &setter) && lit_value_isfield(setter))
                    {
                        field = lit_value_asfield(setter);
                        if(field->setter == NULL)
                        {
                            vmexec_raiseerrorfmt("Class %s does not have a setter for the field %s", instobj->klass->name->chars,
                                               field_name->chars);
                        }
                        vmexec_dropn(fiber, 2);
                        vmexec_push(fiber, value);
                        vm_writeframe(&est, est.ip);
                        tmpval = lit_value_objectvalue(field->setter);
                        vm_callvalue(tmpval, 1);
                        vm_readframe(fiber, &est);
                        continue;
                    }
                    if(lit_value_isnull(value))
                    {
                        lit_table_delete(&instobj->fields, field_name);
                    }
                    else
                    {
                        lit_table_set(state, &instobj->fields, field_name, value);
                    }
                    vmexec_dropn(fiber, 2);// Pop field name and the value
                    fiber->stack_top[-1] = value;
                }
                else
                {
                    klassobj = lit_state_getclassfor(state, instval);
                    if(klassobj == NULL)
                    {
                        vmexec_raiseerror("SET_FIELD: only instances and classes have fields");
                    }
                    if(lit_table_get(&klassobj->methods, field_name, &setter) && lit_value_isfield(setter))
                    {
                        field = lit_value_asfield(setter);
                        if(field->setter == NULL)
                        {
                            vmexec_raiseerrorfmt("Class %s does not have a setter for the field %s", klassobj->name->chars,
                                               field_name->chars);
                        }
                        vmexec_dropn(fiber, 2);
                        vmexec_push(fiber, value);
                        vm_writeframe(&est, est.ip);
                        tmpval = lit_value_objectvalue(field->setter);
                        vm_callvalue(tmpval, 1);
                        vm_readframe(fiber, &est);
                        continue;
                    }
                    else
                    {
                        vmexec_raiseerrorfmt("Class %s does not contain field %s", klassobj->name->chars, field_name->chars);
                    }
                }
                continue;
            }
            op_case(SUBSCRIPT_GET)
            {
                vm_invokemethod(vmexec_peek(fiber, 1), "[]", 1);
                continue;
            }
            op_case(SUBSCRIPT_SET)
            {
                vm_invokemethod(vmexec_peek(fiber, 2), "[]", 2);
                continue;
            }
            op_case(PUSH_ARRAY_ELEMENT)
            {
                values = &lit_value_asarray(vmexec_peek(fiber, 1))->list;
                arindex = lit_vallist_count(values);
                lit_vallist_ensuresize(state, values, arindex + 1);
                lit_vallist_set(values, arindex, vmexec_peek(fiber, 0));
                vmexec_drop(fiber);
                continue;
            }
            op_case(PUSH_OBJECT_FIELD)
            {
                operand = vmexec_peek(fiber, 2);
                if(lit_value_ismap(operand))
                {
                    lit_table_set(state, &lit_value_asmap(operand)->values, lit_value_asstring(vmexec_peek(fiber, 1)), vmexec_peek(fiber, 0));
                }
                else if(lit_value_isinstance(operand))
                {
                    lit_table_set(state, &lit_value_asinstance(operand)->fields, lit_value_asstring(vmexec_peek(fiber, 1)), vmexec_peek(fiber, 0));
                }
                else
                {
                    vmexec_raiseerrorfmt("Expected an object or a map as the operand, got %s", lit_tostring_typename(operand));
                }
                vmexec_dropn(fiber, 2);
                continue;
            }
            op_case(STATIC_FIELD)
            {
                lit_table_set(state, &lit_value_asclass(vmexec_peek(fiber, 1))->static_fields, vmexec_readstringlong(&est), vmexec_peek(fiber, 0));
                vmexec_drop(fiber);
                continue;
            }
            op_case(METHOD)
            {
                klassobj = lit_value_asclass(vmexec_peek(fiber, 1));
                name = vmexec_readstringlong(&est);
                if((klassobj->init_method == NULL || (klassobj->super != NULL && klassobj->init_method == ((LitClass*)klassobj->super)->init_method))
                   && lit_string_getlength(name) == 11 && memcmp(name->chars, "constructor", 11) == 0)
                {
                    klassobj->init_method = lit_value_asobject(vmexec_peek(fiber, 0));
                }
                lit_table_set(state, &klassobj->methods, name, vmexec_peek(fiber, 0));
                vmexec_drop(fiber);
                continue;
            }
            op_case(DEFINE_FIELD)
            {
                lit_table_set(state, &lit_value_asclass(vmexec_peek(fiber, 1))->methods, vmexec_readstringlong(&est), vmexec_peek(fiber, 0));
                vmexec_drop(fiber);
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
                argc = vmexec_readbyte(&est);
                mthname = vmexec_readstringlong(&est);
                klassobj = lit_value_asclass(vmexec_pop(fiber));
                vm_writeframe(&est, est.ip);
                vmexec_invokefromclass(klassobj, mthname, argc, true, methods, false);
                continue;
            }
            op_case(INVOKE_SUPER_IGNORING)
            {
                argc = vmexec_readbyte(&est);
                mthname = vmexec_readstringlong(&est);
                klassobj = lit_value_asclass(vmexec_pop(fiber));
                vm_writeframe(&est, est.ip);
                vmexec_invokefromclass(klassobj, mthname, argc, true, methods, true);
                continue;
            }
            op_case(GET_SUPER_METHOD)
            {
                mthname = vmexec_readstringlong(&est);
                klassobj = lit_value_asclass(vmexec_pop(fiber));
                instval = vmexec_pop(fiber);
                if(lit_table_get(&klassobj->methods, mthname, &value))
                {
                    value = lit_value_objectvalue(lit_create_bound_method(state, instval, value));
                }
                else
                {
                    value = NULL_VALUE;
                }
                vmexec_push(fiber, value);
                continue;
            }
            op_case(INHERIT)
            {
                super = vmexec_peek(fiber, 1);
                if(!lit_value_isclass(super))
                {
                    vmexec_raiseerror("Superclass must be a class");
                }
                klassobj = lit_value_asclass(vmexec_peek(fiber, 0));
                super_klass = lit_value_asclass(super);
                klassobj->super = super_klass;
                klassobj->init_method = super_klass->init_method;
                lit_table_add_all(state, &super_klass->methods, &klassobj->methods);
                lit_table_add_all(state, &klassobj->super->static_fields, &klassobj->static_fields);
                continue;
            }
            op_case(IS)
            {
                instval = vmexec_peek(fiber, 1);
                if(lit_value_isnull(instval))
                {
                    vmexec_dropn(fiber, 2);
                    vmexec_push(fiber, FALSE_VALUE);

                    continue;
                }
                instance_klass = lit_state_getclassfor(state, instval);
                klassval = vmexec_peek(fiber, 0);
                if(instance_klass == NULL || !lit_value_isclass(klassval))
                {
                    vmexec_raiseerror("operands must be an instance and a class");
                }            
                type = lit_value_asclass(klassval);
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
                vmexec_dropn(fiber, 2);// Drop the instance and class
                vmexec_push(fiber, lit_bool_to_value(vm->state, found));
                continue;
            }
            op_case(POP_LOCALS)
            {
                vmexec_dropn(fiber, vmexec_readshort(&est));
                continue;
            }
            op_case(VARARG)
            {
                slot = est.slots[vmexec_readbyte(&est)];
                if(!lit_value_isarray(slot))
                {
                    continue;
                }
                values = &lit_value_asarray(slot)->list;
                lit_ensure_fiber_stack(state, fiber, lit_vallist_count(values) + est.frame->function->max_slots + (int)(fiber->stack_top - fiber->stack));
                for(i = 0; i < lit_vallist_count(values); i++)
                {
                    vmexec_push(fiber, lit_vallist_get(values, i));
                }
                // Hot-bytecode patching, increment the amount of arguments to OP_CALL
                est.ip[1] = est.ip[1] + lit_vallist_count(values) - 1;
                continue;
            }

            op_case(REFERENCE_GLOBAL)
            {
                name = vmexec_readstringlong(&est);
                if(lit_table_get_slot(&vm->globals->values, name, &pval))
                {
                    vmexec_push(fiber, lit_value_objectvalue(lit_create_reference(state, pval)));
                }
                else
                {
                    vmexec_raiseerror("Attempt to reference a null value");
                }
                continue;
            }
            op_case(REFERENCE_PRIVATE)
            {
                vmexec_push(fiber, lit_value_objectvalue(lit_create_reference(state, &est.privates[vmexec_readshort(&est)])));
                continue;
            }
            op_case(REFERENCE_LOCAL)
            {
                vmexec_push(fiber, lit_value_objectvalue(lit_create_reference(state, &est.slots[vmexec_readshort(&est)])));
                continue;
            }
            op_case(REFERENCE_UPVALUE)
            {
                vmexec_push(fiber, lit_value_objectvalue(lit_create_reference(state, est.upvalues[vmexec_readbyte(&est)]->location)));
                continue;
            }
            op_case(REFERENCE_FIELD)
            {
                object = vmexec_peek(fiber, 1);
                if(lit_value_isnull(object))
                {
                    vmexec_raiseerror("Attempt to index a null value");
                }
                name = lit_value_asstring(vmexec_peek(fiber, 0));
                if(lit_value_isinstance(object))
                {
                    if(!lit_table_get_slot(&lit_value_asinstance(object)->fields, name, &pval))
                    {
                        vmexec_raiseerror("Attempt to reference a null value");
                    }
                }
                else
                {
                    lit_tostring_value(state, &state->debugwriter, object);
                    printf("\n");
                    vmexec_raiseerror("You can only reference fields of real instances");
                }
                vmexec_drop(fiber);// Pop field name
                fiber->stack_top[-1] = lit_value_objectvalue(lit_create_reference(state, pval));
                continue;
            }
            op_case(SET_REFERENCE)
            {
                reference = vmexec_pop(fiber);
                if(!lit_value_isreference(reference))
                {
                    vmexec_raiseerror("Provided value is not a reference");
                }
                *lit_value_asreference(reference)->slot = vmexec_peek(fiber, 0);
                continue;
            }
            vm_default()
            {
                vmexec_raiseerrorfmt("Unknown op code '%d'", *est.ip);
                break;
            }
        }
    }

    vm_returnerror();
}


void lit_native_exit_jump()
{
    longjmp(jump_buffer, 1);
}

bool lit_set_native_exit_jump()
{
    return setjmp(jump_buffer);
}


