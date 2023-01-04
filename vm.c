
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include "lit.h"

#define LIT_VM_INLINE

enum
{
    RECOVER_RETURNOK,
    RECOVER_RETURNFAIL,
    RECOVER_NOTHING
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

LIT_VM_INLINE uint16_t lit_vmexec_readshort(LitExecState *est);
LIT_VM_INLINE uint8_t lit_vmexec_readbyte(LitExecState *est);
LIT_VM_INLINE LitValue lit_vmexec_readconstant(LitExecState *est);
LIT_VM_INLINE LitValue lit_vmexec_readconstantlong(LitExecState *est);
LIT_VM_INLINE LitString *lit_vmexec_readstring(LitExecState *est);
LIT_VM_INLINE LitString *lit_vmexec_readstringlong(LitExecState *est);
LIT_VM_INLINE void lit_vmexec_push(LitFiber *fiber, LitValue v);
LIT_VM_INLINE LitValue lit_vmexec_pop(LitFiber *fiber);
LIT_VM_INLINE void lit_vmexec_drop(LitFiber *fiber);
LIT_VM_INLINE void lit_vmexec_dropn(LitFiber *fiber, int amount);
LIT_VM_INLINE LitValue lit_vmexec_peek(LitFiber *fiber, short distance);
LIT_VM_INLINE void lit_vmexec_readframe(LitFiber *fiber, LitExecState *est);
LIT_VM_INLINE void lit_vmexec_writeframe(LitExecState *est, uint8_t *ip);
void lit_vmexec_resetstack(LitVM *vm);
void lit_vmexec_resetvm(LitState *state, LitVM *vm);
void lit_vm_init(LitState *state, LitVM *vm);
void lit_vm_destroy(LitVM *vm);
void lit_vm_tracestack(LitVM *vm, LitWriter *wr);
bool lit_vm_handleruntimeerror(LitVM *vm, LitString *error_string);
bool lit_vm_vraiseerror(LitVM *vm, const char *format, va_list args);
bool lit_vm_raiseerror(LitVM *vm, const char *format, ...);
bool lit_vm_raiseexitingerror(LitVM *vm, const char *format, ...);
bool lit_vm_callcallable(LitVM *vm, LitFunction *function, LitClosure *closure, uint8_t argc);
bool lit_vm_callvalue(LitVM *vm, LitFiber* fiber, LitExecState* est, LitValue callee, uint8_t argc);
LitUpvalue *lit_execvm_captureupvalue(LitState *state, LitValue *local);
void lit_vm_closeupvalues(LitVM *vm, const LitValue *last);
LitInterpretResult lit_vm_execmodule(LitState *state, LitModule *module);
LitInterpretResult lit_vm_execfiber(LitState *state, LitFiber *fiber);
void lit_vmutil_callexitjump(void);
bool lit_vmutil_setexitjump(void);


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
        name:
#else
    #define vm_default() default:
    #define op_case(name) \
        case name:
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
* lit_vm_execfiber.
* might be possible to wrap this by using an enum to specify
* if (and what) to return, but it'll be quite a bit of work to refactor.
* likewise, any macro that uses vm_recoverstate can't be turned into
* a function.
*/
#define vm_recoverstate(fiber, est) \
    lit_vmexec_writeframe(&est, est.ip); \
    fiber = vm->fiber; \
    if(fiber == NULL) \
    { \
        return (LitInterpretResult){ LITRESULT_OK, lit_vmexec_pop(fiber) }; \
    } \
    if(fiber->abort) \
    { \
        vm_returnerror(); \
    } \
    lit_vmexec_readframe(fiber, &est); \
    vm_traceframe(fiber);

#define vm_callvalue(callee, argc) \
    if(lit_vm_callvalue(vm, fiber, &est, callee, argc)) \
    { \
        vm_recoverstate(fiber, est); \
    }

#define vmexec_raiseerrorfmt(format, ...) \
    if(lit_vm_raiseerror(vm, format, __VA_ARGS__)) \
    { \
        vm_recoverstate(fiber, est);  \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }

#define vmexec_raiseerror(format) \
    if(lit_vm_raiseerror(vm, format)) \
    { \
        vm_recoverstate(fiber, est); \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }

#define vmexec_advinvokefromclass(zklass, mthname, argc, lit_emitter_raiseerror, stat, ignoring, callee) \
    LitValue mthval; \
    if((lit_value_isinstance(callee) && (lit_table_get(&lit_value_asinstance(callee)->fields, mthname, &mthval))) \
       || lit_table_get(&zklass->stat, mthname, &mthval)) \
    { \
        if(ignoring) \
        { \
            if(lit_vm_callvalue(vm, fiber, &est, mthval, argc)) \
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
        if(lit_emitter_raiseerror) \
        { \
            vmexec_raiseerrorfmt("cannot lit_vm_callcallable undefined method '%s' of class '%s'", mthname->chars, \
                               zklass->name->chars) \
        } \
    } \
    if(lit_emitter_raiseerror) \
    { \
        continue; \
    }

// calls vm_recoverstate
#define vmexec_invokefromclass(klass, mthname, argc, lit_emitter_raiseerror, stat, ignoring) \
    vmexec_advinvokefromclass(klass, mthname, argc, lit_emitter_raiseerror, stat, ignoring, lit_vmexec_peek(fiber, argc))

// calls vm_recoverstate
#define vm_invokemethod(instance, mthname, argc) \
    if(lit_value_isnull(instance)) \
    { \
        vmexec_raiseerrorfmt("cannot lit_vm_callcallable method '%s' of null-instance", mthname); \
    } \
    LitClass* klass = lit_state_getclassfor(state, instance); \
    if(klass == NULL) \
    { \
        vmexec_raiseerrorfmt("cannot lit_vm_callcallable method '%s' of a non-class", mthname); \
    } \
    lit_vmexec_writeframe(&est, est.ip); \
    vmexec_advinvokefromclass(klass, CONST_STRING(state, mthname), argc, true, methods, false, instance); \
    lit_vmexec_readframe(fiber, &est)

#define vm_binaryop(type, op, op_string) \
    LitValue a = lit_vmexec_peek(fiber, 1); \
    LitValue b = lit_vmexec_peek(fiber, 0); \
    if(lit_value_isnumber(a)) \
    { \
        if(!lit_value_isnumber(b)) \
        { \
            if(!lit_value_isnull(b)) \
            { \
                vmexec_raiseerrorfmt("cannot use op '%s' with a 'number' and a '%s'", op_string, lit_tostring_typename(b)); \
            } \
        } \
        lit_vmexec_drop(fiber); \
        *(fiber->stack_top - 1) = (type(vm->state, lit_value_asnumber(a) op lit_value_asnumber(b))); \
        continue; \
    } \
    if(lit_value_isnull(a)) \
    { \
    /* vmexec_raiseerrorfmt("Attempt to use op %s on a null value", op_string); */ \
        lit_vmexec_drop(fiber); \
        *(fiber->stack_top - 1) = TRUE_VALUE; \
    } \
    else \
    { \
        vm_invokemethod(a, op_string, 1); \
    }

#define vm_bitwiseop(op, op_string) \
    LitValue a = lit_vmexec_peek(fiber, 1); \
    LitValue b = lit_vmexec_peek(fiber, 0); \
    /* if((!lit_value_isnumber(a) && !lit_value_isnull(a)) || (!lit_value_isnumber(b) && !lit_value_isnull(b))) */ \
    if((!lit_value_isnumber(a) ) || (!lit_value_isnumber(b))) \
    { \
        vmexec_raiseerrorfmt("cannot use bitwise op '%s' with types '%s' and '%s'", op_string, \
                           lit_tostring_typename(a), lit_tostring_typename(b)); \
    } \
    lit_vmexec_drop(fiber); \
    *(fiber->stack_top - 1) = (lit_value_numbertovalue(vm->state, (int)lit_value_asnumber(a) op(int) lit_value_asnumber(b)));

#define vm_invokeoperation(ignoring) \
    uint8_t argc = lit_vmexec_readbyte(&est); \
    LitString* mthname = lit_vmexec_readstringlong(&est); \
    LitValue receiver = lit_vmexec_peek(fiber, argc); \
    if(lit_value_isnull(receiver)) \
    { \
        vmexec_raiseerrorfmt("cannot index a null value with '%s'", mthname->chars); \
    } \
    lit_vmexec_writeframe(&est, est.ip); \
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
            lit_vmexec_readframe(fiber, &est); \
            continue; \
        } \
        vmexec_advinvokefromclass(instance->klass, mthname, argc, true, methods, ignoring, receiver); \
    } \
    else \
    { \
        LitClass* type = lit_state_getclassfor(state, receiver); \
        if(type == NULL) \
        { \
            vmexec_raiseerror("cannot get class"); \
        } \
        vmexec_advinvokefromclass(type, mthname, argc, true, methods, ignoring, receiver); \
    }

static jmp_buf jump_buffer;

LIT_VM_INLINE uint16_t lit_vmexec_readshort(LitExecState* est)
{
    est->ip += 2u;
    return (uint16_t)((est->ip[-2] << 8u) | est->ip[-1]);
}

LIT_VM_INLINE uint8_t lit_vmexec_readbyte(LitExecState* est)
{
    return (*est->ip++);
}

LIT_VM_INLINE LitValue lit_vmexec_readconstant(LitExecState* est)
{
    return lit_vallist_get(&est->current_chunk->constants, lit_vmexec_readbyte(est));
}

LIT_VM_INLINE LitValue lit_vmexec_readconstantlong(LitExecState* est)
{
    return lit_vallist_get(&est->current_chunk->constants, lit_vmexec_readshort(est));
}

LIT_VM_INLINE LitString* lit_vmexec_readstring(LitExecState* est)
{
    return lit_value_asstring(lit_vmexec_readconstant(est));
}

LIT_VM_INLINE LitString* lit_vmexec_readstringlong(LitExecState* est)
{
    return lit_value_asstring(lit_vmexec_readconstantlong(est));
}


LIT_VM_INLINE void lit_vmexec_push(LitFiber* fiber, LitValue v)
{
    *fiber->stack_top++ = v;
}

LIT_VM_INLINE LitValue lit_vmexec_pop(LitFiber* fiber)
{
    return *(--fiber->stack_top);
}

LIT_VM_INLINE void lit_vmexec_drop(LitFiber* fiber)
{
    fiber->stack_top--;
}

LIT_VM_INLINE void lit_vmexec_dropn(LitFiber* fiber, int amount)
{
    fiber->stack_top -= amount;
}

LIT_VM_INLINE LitValue lit_vmexec_peek(LitFiber* fiber, short distance)
{
    int ofs;
    ofs = ((-1) - distance);
    if(ofs < 0)
    {
        //return NULL_VALUE;
    }
    return fiber->stack_top[ofs];
}

LIT_VM_INLINE void lit_vmexec_readframe(LitFiber* fiber, LitExecState* est)
{
    est->frame = &fiber->frames[fiber->frame_count - 1];
    est->current_chunk = &est->frame->function->chunk;
    est->ip = est->frame->ip;
    est->slots = est->frame->slots;
    fiber->module = est->frame->function->module;
    est->privates = fiber->module->privates;
    est->upvalues = est->frame->closure == NULL ? NULL : est->frame->closure->upvalues;
}

LIT_VM_INLINE void lit_vmexec_writeframe(LitExecState* est, uint8_t* ip)
{
    est->frame->ip = ip;
}

void lit_vmexec_resetstack(LitVM* vm)
{
    if(vm->fiber != NULL)
    {
        vm->fiber->stack_top = vm->fiber->stack;
    }
}

void lit_vmexec_resetvm(LitState* state, LitVM* vm)
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

void lit_vm_init(LitState* state, LitVM* vm)
{
    lit_vmexec_resetvm(state, vm);
    vm->globals = lit_create_map(state);
    vm->modules = lit_create_map(state);
}

void lit_vm_destroy(LitVM* vm)
{
    lit_table_destroy(vm->state, &vm->strings);
    lit_object_destroylistof(vm->state, vm->objects);
    lit_vmexec_resetvm(vm->state, vm);
}

void lit_vm_tracestack(LitVM* vm, LitWriter* wr)
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
        lit_towriter_value(vm->state, wr, *slot);
        lit_writer_writeformat(wr, " ]");
    }
    lit_writer_writeformat(wr, "%s", COLOR_RESET);
    for(slot = top; slot < fiber->stack_top; slot++)
    {
        lit_writer_writeformat(wr, "[ ");
        lit_towriter_value(vm->state, wr, *slot);
        lit_writer_writeformat(wr, " ]");
    }
    lit_writer_writeformat(wr, "\n");
}

bool lit_vm_handleruntimeerror(LitVM* vm, LitString* error_string)
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
    LitValue lit_emitter_raiseerror;
    LitFiber* fiber;
    LitFiber* caller;
    lit_emitter_raiseerror = lit_value_objectvalue(error_string);
    fiber = vm->fiber;
    while(fiber != NULL)
    {
        fiber->lit_emitter_raiseerror = lit_emitter_raiseerror;
        if(fiber->catcher)
        {
            vm->fiber = fiber->parent;
            vm->fiber->stack_top -= fiber->arg_count;
            vm->fiber->stack_top[-1] = lit_emitter_raiseerror;
            return true;
        }
        caller = fiber->parent;
        fiber->parent = NULL;
        fiber = caller;
    }
    fiber = vm->fiber;
    fiber->abort = true;
    fiber->lit_emitter_raiseerror = lit_emitter_raiseerror;
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
    lit_vmexec_resetstack(vm);
    return false;
}

bool lit_vm_vraiseerror(LitVM* vm, const char* format, va_list args)
{
    size_t buffer_size;
    char* buffer;
    va_list args_copy;
    va_copy(args_copy, args);
    buffer_size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);
    buffer = (char*)malloc(buffer_size+1);
    vsnprintf(buffer, buffer_size, format, args);
    return lit_vm_handleruntimeerror(vm, lit_string_take(vm->state, buffer, buffer_size, false));
}

bool lit_vm_raiseerror(LitVM* vm, const char* format, ...)
{
    bool result;
    va_list args;
    va_start(args, format);
    result = lit_vm_vraiseerror(vm, format, args);
    va_end(args);
    return result;
}

bool lit_vm_raiseexitingerror(LitVM* vm, const char* format, ...)
{
    bool result;
    va_list args;
    va_start(args, format);
    result = lit_vm_vraiseerror(vm, format, args);
    va_end(args);
    lit_vmutil_callexitjump();
    return result;
}

bool lit_vm_callcallable(LitVM* vm, LitFunction* function, LitClosure* closure, uint8_t argc)
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
        //lit_vm_raiseerror(vm, "lit_vm_callcallable stack overflow");
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

const char* lit_vmexec_funcnamefromvalue(LitVM* vm, LitExecState* est, LitValue v)
{
    LitValue vn;
    vn = lit_get_function_name(vm, v);
    if(!lit_value_isnull(vn))
    {
        return lit_value_ascstring(vn);
    }
    return "unknown";
}

bool lit_vm_callvalue(LitVM* vm, LitFiber* fiber, LitExecState* est, LitValue callee, uint8_t argc)
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
        if(lit_vmutil_setexitjump())
        {
            return true;
        }
        switch(lit_value_type(callee))
        {
            case LITTYPE_FUNCTION:
                {
                    return lit_vm_callcallable(vm, lit_value_asfunction(callee), NULL, argc);
                }
                break;
            case LITTYPE_CLOSURE:
                {
                    closure = lit_value_asclosure(callee);
                    return lit_vm_callcallable(vm, closure->function, closure, argc);
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
                        return lit_vm_callvalue(vm, fiber, est, lit_value_objectvalue(klass->init_method), argc);
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
                        return lit_vm_callcallable(vm, lit_value_asfunction(mthval), NULL, argc);
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
        fromval = lit_vmexec_peek(fiber, 0);
    }
    fname = "unknown";
    fprintf(stderr, "fromval type=%d %s\n", lit_value_type(fromval), lit_tostring_typename(fromval));
    if(lit_value_isfunction(fromval))
    {
        fname = lit_vmexec_funcnamefromvalue(vm, est, fromval);
    }
    else if(lit_value_isstring(fromval))
    {
        fname = lit_value_ascstring(fromval);
    }
    if(lit_value_isnull(callee))
    {
        lit_vm_raiseerror(vm, "attempt to lit_vm_callcallable '%s' which is null", fname);
    }
    else
    {
        lit_vm_raiseerror(vm, "attempt to lit_vm_callcallable '%s' which is neither function nor class, but is %s", fname, lit_tostring_typename(callee));
    }
    return true;
}

LitUpvalue* lit_execvm_captureupvalue(LitState* state, LitValue* local)
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

void lit_vm_closeupvalues(LitVM* vm, const LitValue* last)
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

LitInterpretResult lit_vm_execmodule(LitState* state, LitModule* module)
{
    LitVM* vm;
    LitFiber* fiber;
    LitInterpretResult result;
    vm = state->vm;
    fiber = lit_create_fiber(state, module, module->main_function);
    vm->fiber = fiber;
    lit_vm_push(vm, lit_value_objectvalue(module->main_function));
    result = lit_vm_execfiber(state, fiber);
    return result;
}


double lit_util_uinttofloat(unsigned int val);
unsigned int lit_util_floattouint(double val);
int lit_util_numbertoint32(double n);
int lit_util_doubletoint(double n);
unsigned int lit_util_numbertouint32(double n);

LitInterpretResult lit_vm_execfiber(LitState* state, LitFiber* fiber)
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
        lit_vm_tracestack(vm);
#endif

#ifdef LIT_CHECK_STACK_SIZE
        if((fiber->stack_top - est.frame->slots) > fiber->stack_capacity)
        {
            vmexec_raiseerrorfmt("fiber stack too small (%i > %i)", (int)(fiber->stack_top - est.frame->slots),
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
        * an easy mistake to make, but crucial to lit_parser_check.
        */
        {
            op_case(OP_POP)
            {
                lit_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_RETURN)
            {
                result = lit_vmexec_pop(fiber);
                lit_vm_closeupvalues(vm, est.slots);
                lit_vmexec_writeframe(&est, est.ip);
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
                        lit_vmexec_drop(fiber);
                        state->allow_gc = was_allowed;
                        return (LitInterpretResult){ LITRESULT_OK, result };
                    }
                    argc = fiber->arg_count;
                    parent = fiber->parent;
                    fiber->parent = NULL;
                    vm->fiber = fiber = parent;
                    lit_vmexec_readframe(fiber, &est);
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
                    lit_vmexec_push(fiber, result);
                }
                lit_vmexec_readframe(fiber, &est);
                vm_traceframe(fiber);
                continue;
            }
            op_case(OP_CONSTANT)
            {
                lit_vmexec_push(fiber, lit_vmexec_readconstant(&est));
                continue;
            }
            op_case(OP_CONSTANT_LONG)
            {
                lit_vmexec_push(fiber, lit_vmexec_readconstantlong(&est));
                continue;
            }
            op_case(OP_TRUE)
            {
                lit_vmexec_push(fiber, TRUE_VALUE);
                continue;
            }
            op_case(OP_FALSE)
            {
                lit_vmexec_push(fiber, FALSE_VALUE);
                continue;
            }
            op_case(OP_NULL)
            {
                lit_vmexec_push(fiber, NULL_VALUE);
                continue;
            }
            op_case(OP_ARRAY)
            {
                lit_vmexec_push(fiber, lit_value_objectvalue(lit_create_array(state)));
                continue;
            }
            op_case(OP_OBJECT)
            {
                // TODO: use object, or map for literal '{...}' constructs?
                // objects would be more general-purpose, but don't implement anything map-like.
                //lit_vmexec_push(fiber, lit_value_objectvalue(lit_create_instance(state, state->objectvalue_class)));
                lit_vmexec_push(fiber, lit_value_objectvalue(lit_create_map(state)));

                continue;
            }
            op_case(OP_RANGE)
            {
                a = lit_vmexec_pop(fiber);
                b = lit_vmexec_pop(fiber);
                if(!lit_value_isnumber(a) || !lit_value_isnumber(b))
                {
                    vmexec_raiseerror("range fields must be numbers");
                }
                lit_vmexec_push(fiber, lit_value_objectvalue(lit_create_range(state, lit_value_asnumber(a), lit_value_asnumber(b))));
                continue;
            }
            op_case(OP_NEGATE)
            {
                if(!lit_value_isnumber(lit_vmexec_peek(fiber, 0)))
                {
                    arg = lit_vmexec_peek(fiber, 0);
                    vmexec_raiseerror("operand must be a number");
                }
                tmpval = lit_value_numbertovalue(vm->state, -lit_value_asnumber(lit_vmexec_pop(fiber)));
                lit_vmexec_push(fiber, tmpval);
                continue;
            }
            op_case(OP_NOT)
            {
                if(lit_value_isinstance(lit_vmexec_peek(fiber, 0)))
                {
                    lit_vmexec_writeframe(&est, est.ip);
                    vmexec_invokefromclass(lit_value_asinstance(lit_vmexec_peek(fiber, 0))->klass, CONST_STRING(state, "!"), 0, false, methods, false);
                    continue;
                }
                tmpval = lit_bool_to_value(vm->state, lit_value_isfalsey(lit_vmexec_pop(fiber)));
                lit_vmexec_push(fiber, tmpval);
                continue;
            }
            op_case(OP_BNOT)
            {
                if(!lit_value_isnumber(lit_vmexec_peek(fiber, 0)))
                {
                    vmexec_raiseerror("Operand must be a number");
                }
                tmpval = lit_value_numbertovalue(vm->state, ~((int)lit_value_asnumber(lit_vmexec_pop(fiber))));
                lit_vmexec_push(fiber, tmpval);
                continue;
            }
            op_case(OP_ADD)
            {
                vm_binaryop(lit_value_numbertovalue, +, "+");
                continue;
            }
            op_case(OP_SUBTRACT)
            {
                vm_binaryop(lit_value_numbertovalue, -, "-");
                continue;
            }
            op_case(OP_MULTIPLY)
            {
                vm_binaryop(lit_value_numbertovalue, *, "*");
                continue;
            }
            op_case(OP_POWER)
            {
                a = lit_vmexec_peek(fiber, 1);
                b = lit_vmexec_peek(fiber, 0);
                if(lit_value_isnumber(a) && lit_value_isnumber(b))
                {
                    lit_vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = (lit_value_numbertovalue(vm->state, pow(lit_value_asnumber(a), lit_value_asnumber(b))));
                    continue;
                }
                vm_invokemethod(a, "**", 1);
                continue;
            }
            op_case(OP_DIVIDE)
            {
                vm_binaryop(lit_value_numbertovalue, /, "/");
                continue;
            }
            op_case(OP_FLOOR_DIVIDE)
            {
                a = lit_vmexec_peek(fiber, 1);
                b = lit_vmexec_peek(fiber, 0);
                if(lit_value_isnumber(a) && lit_value_isnumber(b))
                {
                    lit_vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = (lit_value_numbertovalue(vm->state, floor(lit_value_asnumber(a) / lit_value_asnumber(b))));

                    continue;
                }

                vm_invokemethod(a, "#", 1);
                continue;
            }
            op_case(OP_MOD)
            {
                a = lit_vmexec_peek(fiber, 1);
                b = lit_vmexec_peek(fiber, 0);
                if(lit_value_isnumber(a) && lit_value_isnumber(b))
                {
                    lit_vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = lit_value_numbertovalue(vm->state, fmod(lit_value_asnumber(a), lit_value_asnumber(b)));
                    continue;
                }
                vm_invokemethod(a, "%", 1);
                continue;
            }
            op_case(OP_BAND)
            {
                vm_bitwiseop(&, "&");
                continue;
            }
            op_case(OP_BOR)
            {
                vm_bitwiseop(|, "|");
                continue;
            }
            op_case(OP_BXOR)
            {
                vm_bitwiseop(^, "^");
                continue;
            }

            op_case(OP_LSHIFT)
            {
                
                LitValue a = lit_vmexec_peek(fiber, 1);
                LitValue b = lit_vmexec_peek(fiber, 0);
                if((!lit_value_isnumber(a) && !lit_value_isnull(a)) || (!lit_value_isnumber(b) && !lit_value_isnull(b)))
                {
                    vmexec_raiseerrorfmt("cannot use bitwise op '%s' with types '%s' and '%s'", "<<", lit_tostring_typename(a), lit_tostring_typename(b));
                }
                lit_vmexec_drop(fiber);
                /*
                    ApeFloat_t rightval = ape_object_value_asnumber(right);
                    ApeFloat_t leftval = ape_object_value_asnumber(left);
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    resfixed = (uleft << (uright & 0x1F));
                    isfixed = true;
                */                
                int uleft = lit_util_numbertoint32(lit_value_asnumber(a));
                unsigned int uright = lit_util_numbertouint32(lit_value_asnumber(b));
                int res = uleft << (uright & 0x1F);
                *(fiber->stack_top - 1) = lit_value_numbertovalue(vm->state, (res));
                continue;
            }
            op_case(OP_RSHIFT)
            {
                LitValue a = lit_vmexec_peek(fiber, 1);
                LitValue b = lit_vmexec_peek(fiber, 0);
                if((!lit_value_isnumber(a) && !lit_value_isnull(a)) || (!lit_value_isnumber(b) && !lit_value_isnull(b)))
                {
                    vmexec_raiseerrorfmt("cannot use bitwise op '%s' with types '%s' and '%s'", ">>", lit_tostring_typename(a), lit_tostring_typename(b));
                }
                lit_vmexec_drop(fiber);
                /*
                    ApeFloat_t rightval = ape_object_value_asnumber(right);
                    ApeFloat_t leftval = ape_object_value_asnumber(left);
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    resfixed = (uleft >> (uright & 0x1F));
                    isfixed = true;
                */
                int uleft = lit_util_numbertoint32(lit_value_asnumber(a));
                unsigned int uright = lit_util_numbertouint32(lit_value_asnumber(b));
                int res = uleft >> (uright & 0x1F);
                *(fiber->stack_top - 1) = lit_value_numbertovalue(vm->state, (res));
                continue;
            }
            op_case(OP_EQUAL)
            {
                vm_binaryop(lit_value_numbertovalue, ==, "==");
                continue;
            }
            op_case(OP_GREATER)
            {
                vm_binaryop(lit_bool_to_value, >, ">");
                continue;
            }
            op_case(OP_GREATER_EQUAL)
            {
                vm_binaryop(lit_bool_to_value, >=, ">=");
                continue;
            }
            op_case(OP_LESS)
            {
                vm_binaryop(lit_bool_to_value, <, "<");
                continue;
            }
            op_case(OP_LESS_EQUAL)
            {
                vm_binaryop(lit_bool_to_value, <=, "<=");
                continue;
            }
            op_case(OP_SET_GLOBAL)
            {
                name = lit_vmexec_readstringlong(&est);
                lit_table_set(state, &vm->globals->values, name, lit_vmexec_peek(fiber, 0));
                continue;
            }

            op_case(OP_GET_GLOBAL)
            {
                name = lit_vmexec_readstringlong(&est);
                if(!lit_table_get(&vm->globals->values, name, &setval))
                {
                    lit_vmexec_push(fiber, NULL_VALUE);
                }
                else
                {
                    lit_vmexec_push(fiber, setval);
                }
                continue;
            }
            op_case(OP_SET_LOCAL)
            {
                index = lit_vmexec_readbyte(&est);
                est.slots[index] = lit_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_LOCAL)
            {
                lit_vmexec_push(fiber, est.slots[lit_vmexec_readbyte(&est)]);
                continue;
            }
            op_case(OP_SET_LOCAL_LONG)
            {
                index = lit_vmexec_readshort(&est);
                est.slots[index] = lit_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_LOCAL_LONG)
            {
                lit_vmexec_push(fiber, est.slots[lit_vmexec_readshort(&est)]);
                continue;
            }
            op_case(OP_SET_PRIVATE)
            {
                index = lit_vmexec_readbyte(&est);
                est.privates[index] = lit_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_PRIVATE)
            {
                lit_vmexec_push(fiber, est.privates[lit_vmexec_readbyte(&est)]);
                continue;
            }
            op_case(OP_SET_PRIVATE_LONG)
            {
                index = lit_vmexec_readshort(&est);
                est.privates[index] = lit_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_PRIVATE_LONG)
            {
                lit_vmexec_push(fiber, est.privates[lit_vmexec_readshort(&est)]);
                continue;
            }
            op_case(OP_SET_UPVALUE)
            {
                index = lit_vmexec_readbyte(&est);
                *est.upvalues[index]->location = lit_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_UPVALUE)
            {
                lit_vmexec_push(fiber, *est.upvalues[lit_vmexec_readbyte(&est)]->location);
                continue;
            }

            op_case(OP_JUMP_IF_FALSE)
            {
                offset = lit_vmexec_readshort(&est);
                if(lit_value_isfalsey(lit_vmexec_pop(fiber)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP_IF_NULL)
            {
                offset = lit_vmexec_readshort(&est);
                if(lit_value_isnull(lit_vmexec_peek(fiber, 0)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP_IF_NULL_POPPING)
            {
                offset = lit_vmexec_readshort(&est);
                if(lit_value_isnull(lit_vmexec_pop(fiber)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP)
            {
                offset = lit_vmexec_readshort(&est);
                est.ip += offset;
                continue;
            }
            op_case(OP_JUMP_BACK)
            {
                offset = lit_vmexec_readshort(&est);
                est.ip -= offset;
                continue;
            }
            op_case(OP_AND)
            {
                offset = lit_vmexec_readshort(&est);
                if(lit_value_isfalsey(lit_vmexec_peek(fiber, 0)))
                {
                    est.ip += offset;
                }
                else
                {
                    lit_vmexec_drop(fiber);
                }
                continue;
            }
            op_case(OP_OR)
            {
                offset = lit_vmexec_readshort(&est);
                if(lit_value_isfalsey(lit_vmexec_peek(fiber, 0)))
                {
                    lit_vmexec_drop(fiber);
                }
                else
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_NULL_OR)
            {
                offset = lit_vmexec_readshort(&est);
                if(lit_value_isnull(lit_vmexec_peek(fiber, 0)))
                {
                    lit_vmexec_drop(fiber);
                }
                else
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_CALL)
            {
                argc = lit_vmexec_readbyte(&est);
                lit_vmexec_writeframe(&est, est.ip);
                peeked = lit_vmexec_peek(fiber, argc);
                vm_callvalue(peeked, argc);
                continue;
            }
            op_case(OP_CLOSURE)
            {
                function = lit_value_asfunction(lit_vmexec_readconstantlong(&est));
                closure = lit_create_closure(state, function);
                lit_vmexec_push(fiber, lit_value_objectvalue(closure));
                for(i = 0; i < closure->upvalue_count; i++)
                {
                    is_local = lit_vmexec_readbyte(&est);
                    index = lit_vmexec_readbyte(&est);
                    if(is_local)
                    {
                        closure->upvalues[i] = lit_execvm_captureupvalue(state, est.frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = est.upvalues[index];
                    }
                }
                continue;
            }
            op_case(OP_CLOSE_UPVALUE)
            {
                lit_vm_closeupvalues(vm, fiber->stack_top - 1);
                lit_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_CLASS)
            {
                name = lit_vmexec_readstringlong(&est);
                klassobj = lit_create_class(state, name);
                lit_vmexec_push(fiber, lit_value_objectvalue(klassobj));
                klassobj->super = state->objectvalue_class;
                lit_table_add_all(state, &klassobj->super->methods, &klassobj->methods);
                lit_table_add_all(state, &klassobj->super->static_fields, &klassobj->static_fields);
                lit_table_set(state, &vm->globals->values, name, lit_value_objectvalue(klassobj));
                continue;
            }
            op_case(OP_GET_FIELD)
            {
                object = lit_vmexec_peek(fiber, 1);
                name = lit_value_asstring(lit_vmexec_peek(fiber, 0));
                if(lit_value_isnull(object))
                {
                    vmexec_raiseerrorfmt("attempt to set field '%s' of a null value", name->chars);
                }
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
                                    vmexec_raiseerrorfmt("class %s does not have a getter for field '%s'",
                                                       instobj->klass->name->chars, name->chars);
                                }
                                lit_vmexec_drop(fiber);
                                lit_vmexec_writeframe(&est, est.ip);
                                LitField* field = lit_value_asfield(getval);
                                tmpval =lit_value_objectvalue(field->getter);
                                vm_callvalue(tmpval, 0);
                                lit_vmexec_readframe(fiber, &est);
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
                                vmexec_raiseerrorfmt("class %s does not have a getter for field '%s'", klassobj->name->chars,
                                                   name->chars);
                            }
                            lit_vmexec_drop(fiber);
                            lit_vmexec_writeframe(&est, est.ip);
                            tmpval = lit_value_objectvalue(field->getter);
                            vm_callvalue(tmpval, 0);
                            lit_vmexec_readframe(fiber, &est);
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
                        vmexec_raiseerrorfmt("GET_FIELD: cannot get class object for type '%s'", lit_tostring_typename(object));
                    }
                    if(lit_table_get(&klassobj->methods, name, &getval))
                    {
                        if(lit_value_isfield(getval))
                        {
                            field = lit_value_asfield(getval);
                            if(field->getter == NULL)
                            {
                                vmexec_raiseerrorfmt("class %s does not have a getter for field '%s'", klassobj->name->chars,
                                                   name->chars);
                            }
                            lit_vmexec_drop(fiber);
                            lit_vmexec_writeframe(&est, est.ip);
                            tmpval = lit_value_objectvalue(lit_value_asfield(getval)->getter);
                            vm_callvalue(tmpval, 0);
                            lit_vmexec_readframe(fiber, &est);
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
                lit_vmexec_drop(fiber);// Pop field name
                fiber->stack_top[-1] = getval;
                continue;
            }
            op_case(OP_SET_FIELD)
            {
                instval = lit_vmexec_peek(fiber, 2);
                value = lit_vmexec_peek(fiber, 1);
                field_name = lit_value_asstring(lit_vmexec_peek(fiber, 0));
                if(lit_value_isnull(instval))
                {
                    vmexec_raiseerrorfmt("attempt to set field '%s' of a null value", field_name->chars)
                }
                if(lit_value_isclass(instval))
                {
                    klassobj = lit_value_asclass(instval);
                    if(lit_table_get(&klassobj->static_fields, field_name, &setter) && lit_value_isfield(setter))
                    {
                        field = lit_value_asfield(setter);
                        if(field->setter == NULL)
                        {
                            vmexec_raiseerrorfmt("class %s does not have a setter for field '%s'", klassobj->name->chars,
                                               field_name->chars);
                        }

                        lit_vmexec_dropn(fiber, 2);
                        lit_vmexec_push(fiber, value);
                        lit_vmexec_writeframe(&est, est.ip);
                        tmpval = lit_value_objectvalue(field->setter);
                        vm_callvalue(tmpval, 1);
                        lit_vmexec_readframe(fiber, &est);
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
                    lit_vmexec_dropn(fiber, 2);// Pop field name and the value
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
                            vmexec_raiseerrorfmt("class %s does not have a setter for field '%s'", instobj->klass->name->chars,
                                               field_name->chars);
                        }
                        lit_vmexec_dropn(fiber, 2);
                        lit_vmexec_push(fiber, value);
                        lit_vmexec_writeframe(&est, est.ip);
                        tmpval = lit_value_objectvalue(field->setter);
                        vm_callvalue(tmpval, 1);
                        lit_vmexec_readframe(fiber, &est);
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
                    lit_vmexec_dropn(fiber, 2);// Pop field name and the value
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
                            vmexec_raiseerrorfmt("class '%s' does not have a setter for field '%s'", klassobj->name->chars,
                                               field_name->chars);
                        }
                        lit_vmexec_dropn(fiber, 2);
                        lit_vmexec_push(fiber, value);
                        lit_vmexec_writeframe(&est, est.ip);
                        tmpval = lit_value_objectvalue(field->setter);
                        vm_callvalue(tmpval, 1);
                        lit_vmexec_readframe(fiber, &est);
                        continue;
                    }
                    else
                    {
                        vmexec_raiseerrorfmt("class '%s' does not contain field '%s'", klassobj->name->chars, field_name->chars);
                    }
                }
                continue;
            }
            op_case(OP_SUBSCRIPT_GET)
            {
                vm_invokemethod(lit_vmexec_peek(fiber, 1), "[]", 1);
                continue;
            }
            op_case(OP_SUBSCRIPT_SET)
            {
                vm_invokemethod(lit_vmexec_peek(fiber, 2), "[]", 2);
                continue;
            }
            op_case(OP_PUSH_ARRAY_ELEMENT)
            {
                values = &lit_value_asarray(lit_vmexec_peek(fiber, 1))->list;
                arindex = lit_vallist_count(values);
                lit_vallist_ensuresize(state, values, arindex + 1);
                lit_vallist_set(values, arindex, lit_vmexec_peek(fiber, 0));
                lit_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_PUSH_OBJECT_FIELD)
            {
                operand = lit_vmexec_peek(fiber, 2);
                if(lit_value_ismap(operand))
                {
                    lit_table_set(state, &lit_value_asmap(operand)->values, lit_value_asstring(lit_vmexec_peek(fiber, 1)), lit_vmexec_peek(fiber, 0));
                }
                else if(lit_value_isinstance(operand))
                {
                    lit_table_set(state, &lit_value_asinstance(operand)->fields, lit_value_asstring(lit_vmexec_peek(fiber, 1)), lit_vmexec_peek(fiber, 0));
                }
                else
                {
                    vmexec_raiseerrorfmt("cannot set field '%s' on type '%s'", lit_tostring_typename(operand));
                }
                lit_vmexec_dropn(fiber, 2);
                continue;
            }
            op_case(OP_STATIC_FIELD)
            {
                lit_table_set(state, &lit_value_asclass(lit_vmexec_peek(fiber, 1))->static_fields, lit_vmexec_readstringlong(&est), lit_vmexec_peek(fiber, 0));
                lit_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_METHOD)
            {
                klassobj = lit_value_asclass(lit_vmexec_peek(fiber, 1));
                name = lit_vmexec_readstringlong(&est);
                if((klassobj->init_method == NULL || (klassobj->super != NULL && klassobj->init_method == ((LitClass*)klassobj->super)->init_method))
                   && lit_string_getlength(name) == 11 && memcmp(name->chars, "constructor", 11) == 0)
                {
                    klassobj->init_method = lit_value_asobject(lit_vmexec_peek(fiber, 0));
                }
                lit_table_set(state, &klassobj->methods, name, lit_vmexec_peek(fiber, 0));
                lit_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_DEFINE_FIELD)
            {
                lit_table_set(state, &lit_value_asclass(lit_vmexec_peek(fiber, 1))->methods, lit_vmexec_readstringlong(&est), lit_vmexec_peek(fiber, 0));
                lit_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_INVOKE)
            {
                vm_invokeoperation(false);
                continue;
            }
            op_case(OP_INVOKE_IGNORING)
            {
                vm_invokeoperation(true);
                continue;
            }
            op_case(OP_INVOKE_SUPER)
            {
                argc = lit_vmexec_readbyte(&est);
                mthname = lit_vmexec_readstringlong(&est);
                klassobj = lit_value_asclass(lit_vmexec_pop(fiber));
                lit_vmexec_writeframe(&est, est.ip);
                vmexec_invokefromclass(klassobj, mthname, argc, true, methods, false);
                continue;
            }
            op_case(OP_INVOKE_SUPER_IGNORING)
            {
                argc = lit_vmexec_readbyte(&est);
                mthname = lit_vmexec_readstringlong(&est);
                klassobj = lit_value_asclass(lit_vmexec_pop(fiber));
                lit_vmexec_writeframe(&est, est.ip);
                vmexec_invokefromclass(klassobj, mthname, argc, true, methods, true);
                continue;
            }
            op_case(OP_GET_SUPER_METHOD)
            {
                mthname = lit_vmexec_readstringlong(&est);
                klassobj = lit_value_asclass(lit_vmexec_pop(fiber));
                instval = lit_vmexec_pop(fiber);
                if(lit_table_get(&klassobj->methods, mthname, &value))
                {
                    value = lit_value_objectvalue(lit_create_bound_method(state, instval, value));
                }
                else
                {
                    value = NULL_VALUE;
                }
                lit_vmexec_push(fiber, value);
                continue;
            }
            op_case(OP_INHERIT)
            {
                super = lit_vmexec_peek(fiber, 1);
                if(!lit_value_isclass(super))
                {
                    vmexec_raiseerror("superclass must be a class");
                }
                klassobj = lit_value_asclass(lit_vmexec_peek(fiber, 0));
                super_klass = lit_value_asclass(super);
                klassobj->super = super_klass;
                klassobj->init_method = super_klass->init_method;
                lit_table_add_all(state, &super_klass->methods, &klassobj->methods);
                lit_table_add_all(state, &klassobj->super->static_fields, &klassobj->static_fields);
                continue;
            }
            op_case(OP_IS)
            {
                instval = lit_vmexec_peek(fiber, 1);
                if(lit_value_isnull(instval))
                {
                    lit_vmexec_dropn(fiber, 2);
                    lit_vmexec_push(fiber, FALSE_VALUE);

                    continue;
                }
                instance_klass = lit_state_getclassfor(state, instval);
                klassval = lit_vmexec_peek(fiber, 0);
                if(instance_klass == NULL || !lit_value_isclass(klassval))
                {
                    vmexec_raiseerror("operands must be an instance or a class");
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
                lit_vmexec_dropn(fiber, 2);// Drop the instance and class
                lit_vmexec_push(fiber, lit_bool_to_value(vm->state, found));
                continue;
            }
            op_case(OP_POP_LOCALS)
            {
                lit_vmexec_dropn(fiber, lit_vmexec_readshort(&est));
                continue;
            }
            op_case(OP_VARARG)
            {
                slot = est.slots[lit_vmexec_readbyte(&est)];
                if(!lit_value_isarray(slot))
                {
                    continue;
                }
                values = &lit_value_asarray(slot)->list;
                lit_ensure_fiber_stack(state, fiber, lit_vallist_count(values) + est.frame->function->max_slots + (int)(fiber->stack_top - fiber->stack));
                for(i = 0; i < lit_vallist_count(values); i++)
                {
                    lit_vmexec_push(fiber, lit_vallist_get(values, i));
                }
                // Hot-bytecode patching, increment the amount of arguments to OP_CALL
                est.ip[1] = est.ip[1] + lit_vallist_count(values) - 1;
                continue;
            }

            op_case(OP_REFERENCE_GLOBAL)
            {
                name = lit_vmexec_readstringlong(&est);
                if(lit_table_get_slot(&vm->globals->values, name, &pval))
                {
                    lit_vmexec_push(fiber, lit_value_objectvalue(lit_create_reference(state, pval)));
                }
                else
                {
                    vmexec_raiseerror("attempt to reference a null value");
                }
                continue;
            }
            op_case(OP_REFERENCE_PRIVATE)
            {
                lit_vmexec_push(fiber, lit_value_objectvalue(lit_create_reference(state, &est.privates[lit_vmexec_readshort(&est)])));
                continue;
            }
            op_case(OP_REFERENCE_LOCAL)
            {
                lit_vmexec_push(fiber, lit_value_objectvalue(lit_create_reference(state, &est.slots[lit_vmexec_readshort(&est)])));
                continue;
            }
            op_case(OP_REFERENCE_UPVALUE)
            {
                lit_vmexec_push(fiber, lit_value_objectvalue(lit_create_reference(state, est.upvalues[lit_vmexec_readbyte(&est)]->location)));
                continue;
            }
            op_case(OP_REFERENCE_FIELD)
            {
                object = lit_vmexec_peek(fiber, 1);
                if(lit_value_isnull(object))
                {
                    vmexec_raiseerror("attempt to index a null value");
                }
                name = lit_value_asstring(lit_vmexec_peek(fiber, 0));
                if(lit_value_isinstance(object))
                {
                    if(!lit_table_get_slot(&lit_value_asinstance(object)->fields, name, &pval))
                    {
                        vmexec_raiseerror("attempt to reference a null value");
                    }
                }
                else
                {
                    lit_towriter_value(state, &state->debugwriter, object);
                    printf("\n");
                    vmexec_raiseerrorfmt("cannot reference field '%s' of a non-instance", name->chars);
                }
                lit_vmexec_drop(fiber);// Pop field name
                fiber->stack_top[-1] = lit_value_objectvalue(lit_create_reference(state, pval));
                continue;
            }
            op_case(OP_SET_REFERENCE)
            {
                reference = lit_vmexec_pop(fiber);
                if(!lit_value_isreference(reference))
                {
                    vmexec_raiseerror("cannot set reference value of a non-reference");
                }
                *lit_value_asreference(reference)->slot = lit_vmexec_peek(fiber, 0);
                continue;
            }
            vm_default()
            {
                vmexec_raiseerrorfmt("unknown VM op code '%d'", *est.ip);
                break;
            }
        }
    }

    vm_returnerror();
}


void lit_vmutil_callexitjump()
{
    longjmp(jump_buffer, 1);
}

bool lit_vmutil_setexitjump()
{
    return setjmp(jump_buffer);
}


