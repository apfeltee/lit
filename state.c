
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lit.h"

static bool measure_compilation_time;
static double last_source_time = 0;

void lit_enable_compilation_time_measurement()
{
    measure_compilation_time = true;
}

static void lit_util_default_error(LitState* state, const char* message)
{
    (void)state;
    fflush(stdout);
    fprintf(stderr, "%s%s%s\n", COLOR_RED, message, COLOR_RESET);
    fflush(stderr);
}

static void lit_util_default_printf(LitState* state, const char* message)
{
    (void)state;
    printf("%s", message);
}

LitState* lit_make_state()
{
    LitState* state;
    state = (LitState*)malloc(sizeof(LitState));

    {
        state->config.dumpbytecode = false;
        state->config.dumpast = false;
        state->config.runafterdump = true;
    }
    {
        state->classvalue_class = NULL;
        state->objectvalue_class = NULL;
        state->numbervalue_class = NULL;
        state->stringvalue_class = NULL;
        state->boolvalue_class = NULL;
        state->functionvalue_class = NULL;
        state->fibervalue_class = NULL;
        state->modulevalue_class = NULL;
        state->arrayvalue_class = NULL;
        state->mapvalue_class = NULL;
        state->rangevalue_class = NULL;
    }
    state->bytes_allocated = 0;
    state->next_gc = 256 * 1024;
    state->allow_gc = false;
    /* io stuff */
    {
        state->error_fn = lit_util_default_error;
        state->print_fn = lit_util_default_printf;
        lit_writer_init_file(state, &state->stdoutwriter, stdout, true);
    }
    lit_vallist_init(&state->lightobjects);
    state->had_error = false;
    state->roots = NULL;
    state->root_count = 0;
    state->root_capacity = 0;
    state->last_module = NULL;
    lit_writer_init_file(state, &state->debugwriter, stdout, true);
    state->preprocessor = (LitPreprocessor*)malloc(sizeof(LitPreprocessor));
    lit_preproc_init(state, state->preprocessor);
    state->scanner = (LitScanner*)malloc(sizeof(LitScanner));
    state->parser = (LitParser*)malloc(sizeof(LitParser));
    lit_parser_init(state, (LitParser*)state->parser);
    state->emitter = (LitEmitter*)malloc(sizeof(LitEmitter));
    lit_emitter_init(state, state->emitter);
    state->optimizer = (LitOptimizer*)malloc(sizeof(LitOptimizer));
    lit_astopt_init(state, state->optimizer);
    state->vm = (LitVM*)malloc(sizeof(LitVM));
    lit_vm_init(state, state->vm);
    lit_api_init(state);
    lit_open_core_library(state);
    return state;
}

int64_t lit_destroy_state(LitState* state)
{
    int64_t amount;
    if(state->roots != NULL)
    {
        free(state->roots);
        state->roots = NULL;
    }
    lit_api_destroy(state);
    lit_preproc_destroy(state->preprocessor);
    free(state->preprocessor);
    free(state->scanner);
    lit_parser_destroy(state->parser);
    free(state->parser);
    lit_emitter_destroy(state->emitter);
    free(state->emitter);
    free(state->optimizer);
    lit_vm_destroy(state->vm);
    free(state->vm);
    amount = state->bytes_allocated;
    free(state);
    return amount;
}

void lit_api_init(LitState* state)
{
    state->api_name = lit_string_copy(state, "c", 1);
    state->api_function = NULL;
    state->api_fiber = NULL;
}

void lit_api_destroy(LitState* state)
{
    state->api_name = NULL;
    state->api_function = NULL;
    state->api_fiber = NULL;
}

LitValue lit_state_getglobalvalue(LitState* state, LitString* name)
{
    LitValue global;
    if(!lit_table_get(&state->vm->globals->values, name, &global))
    {
        return NULL_VALUE;
    }
    return global;
}

LitFunction* lit_state_getglobalfunction(LitState* state, LitString* name)
{
    LitValue function = lit_state_getglobalvalue(state, name);
    if(lit_value_isfunction(function))
    {
        return lit_value_asfunction(function);
    }
    return NULL;
}

void lit_state_setglobal(LitState* state, LitString* name, LitValue value)
{
    lit_state_pushroot(state, (LitObject*)name);
    lit_state_pushvalueroot(state, value);
    lit_table_set(state, &state->vm->globals->values, name, value);
    lit_state_poproots(state, 2);
}

bool lit_state_hasglobal(LitState* state, LitString* name)
{
    LitValue global;
    return lit_table_get(&state->vm->globals->values, name, &global);
}

void lit_state_defnativefunc(LitState* state, const char* name, LitNativeFunctionFn native)
{
    lit_state_pushroot(state, (LitObject*)CONST_STRING(state, name));
    lit_state_pushroot(state, (LitObject*)lit_create_native_function(state, native, lit_value_asstring(lit_state_peekroot(state, 0))));
    lit_table_set(state, &state->vm->globals->values, lit_value_asstring(lit_state_peekroot(state, 1)), lit_state_peekroot(state, 0));
    lit_state_poproots(state, 2);
}

void lit_state_defnativeprimitive(LitState* state, const char* name, LitNativePrimitiveFn native)
{
    lit_state_pushroot(state, (LitObject*)CONST_STRING(state, name));
    lit_state_pushroot(state, (LitObject*)lit_create_native_primitive(state, native, lit_value_asstring(lit_state_peekroot(state, 0))));
    lit_table_set(state, &state->vm->globals->values, lit_value_asstring(lit_state_peekroot(state, 1)), lit_state_peekroot(state, 0));
    lit_state_poproots(state, 2);
}

LitValue lit_state_getinstancemethod(LitState* state, LitValue callee, LitString* mthname)
{
    LitValue mthval;
    LitClass* klass;
    klass = lit_state_getclassfor(state, callee);
    if((lit_value_isinstance(callee) && lit_table_get(&lit_value_asinstance(callee)->fields, mthname, &mthval)) || lit_table_get(&klass->methods, mthname, &mthval))
    {
        return mthval;
    }
    return NULL_VALUE;
}

LitInterpretResult lit_state_callinstancemethod(LitState* state, LitValue callee, LitString* mthname, LitValue* argv, size_t argc)
{
    LitValue mthval;
    mthval = lit_state_getinstancemethod(state, callee, mthname);
    if(!lit_value_isnull(mthval))
    {
        return lit_state_callvalue(state, mthval, argv, argc, false);
    }
    return INTERPRET_RUNTIME_FAIL;    
}


LitValue lit_state_getfield(LitState* state, LitTable* table, const char* name)
{
    LitValue value;

    if(!lit_table_get(table, CONST_STRING(state, name), &value))
    {
        value = NULL_VALUE;
    }

    return value;
}

LitValue lit_state_getmapfield(LitState* state, LitMap* map, const char* name)
{
    LitValue value;

    if(!lit_table_get(&map->values, CONST_STRING(state, name), &value))
    {
        value = NULL_VALUE;
    }

    return value;
}

void lit_state_setfield(LitState* state, LitTable* table, const char* name, LitValue value)
{
    lit_table_set(state, table, CONST_STRING(state, name), value);
}

void lit_state_setmapfield(LitState* state, LitMap* map, const char* name, LitValue value)
{
    lit_table_set(state, &map->values, CONST_STRING(state, name), value);
}

bool lit_state_ensurefiber(LitVM* vm, LitFiber* fiber)
{
    size_t newcapacity;
    size_t osize;
    size_t newsize;
    if(fiber == NULL)
    {
        lit_vm_raiseerror(vm, "no fiber to run on");
        return true;
    }
    if(fiber->frame_count == LIT_CALL_FRAMES_MAX)
    {
        lit_vm_raiseerror(vm, "fiber frame overflow");
        return true;
    }
    if(fiber->frame_count + 1 > fiber->frame_capacity)
    {
        //newcapacity = fmin(LIT_CALL_FRAMES_MAX, fiber->frame_capacity * 2);
        newcapacity = (fiber->frame_capacity * 2) + 1;
        osize = (sizeof(LitCallFrame) * fiber->frame_capacity);
        newsize = (sizeof(LitCallFrame) * newcapacity);
        fiber->frames = (LitCallFrame*)lit_gcmem_memrealloc(vm->state, fiber->frames, osize, newsize);
        fiber->frame_capacity = newcapacity;
    }

    return false;
}

static inline LitCallFrame* setup_call(LitState* state, LitFunction* callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    bool vararg;
    int amount;
    size_t i;
    size_t varargc;
    size_t function_arg_count;
    LitVM* vm;
    LitFiber* fiber;
    LitCallFrame* frame;
    LitArray* array;
    (void)argc;
    (void)varargc;
    vm = state->vm;
    fiber = vm->fiber;
    if(callee == NULL)
    {
        lit_vm_raiseerror(vm, "attempt to lit_vm_callcallable a null value");
        return NULL;
    }
    if(ignfiber)
    {
        if(fiber == NULL)
        {
            fiber = state->api_fiber;
        }
    }
    if(!ignfiber)
    {
        if(lit_state_ensurefiber(vm, fiber))
        {
            return NULL;
        }        
    }
    lit_ensure_fiber_stack(state, fiber, callee->max_slots + (int)(fiber->stack_top - fiber->stack));
    frame = &fiber->frames[fiber->frame_count++];
    frame->slots = fiber->stack_top;
    PUSH(lit_value_objectvalue(callee));
    for(i = 0; i < argc; i++)
    {
        PUSH(argv[i]);
    }
    function_arg_count = callee->arg_count;
    if(argc != function_arg_count)
    {
        vararg = callee->vararg;
        if(argc < function_arg_count)
        {
            amount = (int)function_arg_count - argc - (vararg ? 1 : 0);
            for(i = 0; i < (size_t)amount; i++)
            {
                PUSH(NULL_VALUE);
            }
            if(vararg)
            {
                PUSH(lit_value_objectvalue(lit_create_array(vm->state)));
            }
        }
        else if(callee->vararg)
        {
            array = lit_create_array(vm->state);
            varargc = argc - function_arg_count + 1;
            lit_vallist_ensuresize(vm->state, &array->list, varargc);
            for(i = 0; i < varargc; i++)
            {
                lit_vallist_set(&array->list, i, fiber->stack_top[(int)i - (int)varargc]);
            }

            fiber->stack_top -= varargc;
            lit_vm_push(vm, lit_value_objectvalue(array));
        }
        else
        {
            fiber->stack_top -= (argc - function_arg_count);
        }
    }
    else if(callee->vararg)
    {
        array = lit_create_array(vm->state);
        varargc = argc - function_arg_count + 1;
        lit_vallist_push(vm->state, &array->list, *(fiber->stack_top - 1));
        *(fiber->stack_top - 1) = lit_value_objectvalue(array);
    }
    frame->ip = callee->chunk.code;
    frame->closure = NULL;
    frame->function = callee;
    frame->result_ignored = false;
    frame->return_to_c = true;
    return frame;
}

static inline LitInterpretResult execute_call(LitState* state, LitCallFrame* frame)
{
    LitFiber* fiber;
    LitInterpretResult result;
    if(frame == NULL)
    {
        RETURN_RUNTIME_ERROR();
    }
    fiber = state->vm->fiber;
    result = lit_vm_execfiber(state, fiber);
    if(fiber->lit_emitter_raiseerror != NULL_VALUE)
    {
        result.result = fiber->lit_emitter_raiseerror;
    }
    return result;
}

LitInterpretResult lit_state_callfunction(LitState* state, LitFunction* callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    return execute_call(state, setup_call(state, callee, argv, argc, ignfiber));
}

LitInterpretResult lit_state_callclosure(LitState* state, LitClosure* callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    LitCallFrame* frame;
    frame = setup_call(state, callee->function, argv, argc, ignfiber);
    if(frame == NULL)
    {
        RETURN_RUNTIME_ERROR();
    }
    frame->closure = callee;
    return execute_call(state, frame);
}

LitInterpretResult lit_state_callmethod(LitState* state, LitValue instance, LitValue callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    uint8_t i;
    LitVM* vm;
    LitInterpretResult lir;
    LitObjType type;
    LitClass* klass;
    LitFiber* fiber;
    LitValue* slot;
    LitNativeMethod* natmethod;
    LitBoundMethod* bound_method;
    LitValue mthval;
    LitValue result;
    lir.result = NULL_VALUE;
    lir.type = LITRESULT_OK;
    vm = state->vm;
    if(lit_value_isobject(callee))
    {
        if(lit_vmutil_setexitjump())
        {
            RETURN_RUNTIME_ERROR();
        }
        type = lit_value_type(callee);

        if(type == LITTYPE_FUNCTION)
        {
            return lit_state_callfunction(state, lit_value_asfunction(callee), argv, argc, ignfiber);
        }
        else if(type == LITTYPE_CLOSURE)
        {
            return lit_state_callclosure(state, lit_value_asclosure(callee), argv, argc, ignfiber);
        }
        fiber = vm->fiber;
        if(ignfiber)
        {
            if(fiber == NULL)
            {
                fiber = state->api_fiber;
            }
        }
        if(!ignfiber)
        {
            if(lit_state_ensurefiber(vm, fiber))
            {
                RETURN_RUNTIME_ERROR();
            }
        }
        lit_ensure_fiber_stack(state, fiber, 3 + argc + (int)(fiber->stack_top - fiber->stack));
        slot = fiber->stack_top;
        PUSH(instance);
        if(type != LITTYPE_CLASS)
        {
            for(i = 0; i < argc; i++)
            {
                PUSH(argv[i]);
            }
        }
        switch(type)
        {
            case LITTYPE_NATIVE_FUNCTION:
                {
                    LitValue result = lit_value_asnativefunction(callee)->function(vm, argc, fiber->stack_top - argc);
                    fiber->stack_top = slot;
                    RETURN_OK(result);
                }
                break;
            case LITTYPE_NATIVE_PRIMITIVE:
                {
                    lit_value_asnativeprimitive(callee)->function(vm, argc, fiber->stack_top - argc);
                    fiber->stack_top = slot;
                    RETURN_OK(NULL_VALUE);
                }
                break;
            case LITTYPE_NATIVE_METHOD:
                {
                    natmethod = lit_value_asnativemethod(callee);
                    result = natmethod->method(vm, *(fiber->stack_top - argc - 1), argc, fiber->stack_top - argc);
                    fiber->stack_top = slot;
                    RETURN_OK(result);
                }
                break;
            case LITTYPE_CLASS:
                {
                    klass = lit_value_asclass(callee);
                    *slot = lit_value_objectvalue(lit_create_instance(vm->state, klass));
                    if(klass->init_method != NULL)
                    {
                        lir = lit_state_callmethod(state, *slot, lit_value_objectvalue(klass->init_method), argv, argc, ignfiber);
                    }
                    // TODO: when should this return *slot instead of lir?
                    fiber->stack_top = slot;
                    //RETURN_OK(*slot);
                    return lir;
                }
                break;
            case LITTYPE_BOUND_METHOD:
                {
                    bound_method = lit_value_asboundmethod(callee);
                    mthval = bound_method->method;
                    *slot = bound_method->receiver;
                    if(lit_value_isnatmethod(mthval))
                    {
                        result = lit_value_asnativemethod(mthval)->method(vm, bound_method->receiver, argc, fiber->stack_top - argc);
                        fiber->stack_top = slot;
                        RETURN_OK(result);
                    }
                    else if(lit_value_isprimmethod(mthval))
                    {
                        lit_value_asprimitivemethod(mthval)->method(vm, bound_method->receiver, argc, fiber->stack_top - argc);

                        fiber->stack_top = slot;
                        RETURN_OK(NULL_VALUE);
                    }
                    else
                    {
                        fiber->stack_top = slot;
                        return lit_state_callfunction(state, lit_value_asfunction(mthval), argv, argc, ignfiber);
                    }
                }
                break;
            case LITTYPE_PRIMITIVE_METHOD:
                {
                    lit_value_asprimitivemethod(callee)->method(vm, *(fiber->stack_top - argc - 1), argc, fiber->stack_top - argc);
                    fiber->stack_top = slot;
                    RETURN_OK(NULL_VALUE);
                }
                break;
            default:
                {
                }
                break;
        }
    }
    if(lit_value_isnull(callee))
    {
        lit_vm_raiseerror(vm, "attempt to lit_vm_callcallable a null value");
    }
    else
    {
        lit_vm_raiseerror(vm, "can only lit_vm_callcallable functions and classes");
    }

    RETURN_RUNTIME_ERROR();
}

LitInterpretResult lit_state_callvalue(LitState* state, LitValue callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    return lit_state_callmethod(state, callee, callee, argv, argc, ignfiber);
}

LitInterpretResult lit_state_findandcallmethod(LitState* state, LitValue callee, LitString* method_name, LitValue* argv, uint8_t argc, bool ignfiber)
{
    LitClass* klass;
    LitVM* vm;
    LitFiber* fiber;
    LitValue mthval;
    vm = state->vm;
    fiber = vm->fiber;
    if(fiber == NULL)
    {
        if(!ignfiber)
        {
            lit_vm_raiseerror(vm, "no fiber to run on");
            RETURN_RUNTIME_ERROR();
        }
    }
    klass = lit_state_getclassfor(state, callee);
    if((lit_value_isinstance(callee) && lit_table_get(&lit_value_asinstance(callee)->fields, method_name, &mthval)) || lit_table_get(&klass->methods, method_name, &mthval))
    {
        return lit_state_callmethod(state, callee, mthval, argv, argc, ignfiber);
    }
    return (LitInterpretResult){ LITRESULT_INVALID, NULL_VALUE };
}

void lit_state_pushroot(LitState* state, LitObject* object)
{
    lit_state_pushvalueroot(state, lit_value_objectvalue(object));
}

void lit_state_pushvalueroot(LitState* state, LitValue value)
{
    if(state->root_count + 1 >= state->root_capacity)
    {
        state->root_capacity = LIT_GROW_CAPACITY(state->root_capacity);
        state->roots = (LitValue*)realloc(state->roots, state->root_capacity * sizeof(LitValue));
    }
    state->roots[state->root_count++] = value;
}

LitValue lit_state_peekroot(LitState* state, uint8_t distance)
{
    assert(state->root_count - distance + 1 > 0);
    return state->roots[state->root_count - distance - 1];
}

void lit_state_poproot(LitState* state)
{
    state->root_count--;
}

void lit_state_poproots(LitState* state, uint8_t amount)
{
    state->root_count -= amount;
}

LitClass* lit_state_getclassfor(LitState* state, LitValue value)
{
    LitValue* slot;
    LitUpvalue* upvalue;
    if(lit_value_isobject(value))
    {
        switch(lit_value_type(value))
        {
            case LITTYPE_NUMBER:
                {
                    return state->numbervalue_class;
                }
                break;
            case LITTYPE_STRING:
                {
                    return state->stringvalue_class;
                }
                break;
            case LITTYPE_USERDATA:
                {
                    return state->objectvalue_class;
                }
                break;
            case LITTYPE_FIELD:
            case LITTYPE_FUNCTION:
            case LITTYPE_CLOSURE:
            case LITTYPE_NATIVE_FUNCTION:
            case LITTYPE_NATIVE_PRIMITIVE:
            case LITTYPE_BOUND_METHOD:
            case LITTYPE_PRIMITIVE_METHOD:
            case LITTYPE_NATIVE_METHOD:
                {
                    return state->functionvalue_class;
                }
                break;
            case LITTYPE_FIBER:
                {
                    //fprintf(stderr, "should return fiber class ....\n");
                    return state->fibervalue_class;
                }
                break;
            case LITTYPE_MODULE:
                {
                    return state->modulevalue_class;
                }
                break;
            case LITTYPE_UPVALUE:
                {
                    upvalue = lit_value_asupvalue(value);
                    if(upvalue->location == NULL)
                    {
                        return lit_state_getclassfor(state, upvalue->closed);
                    }
                    return lit_state_getclassfor(state, *upvalue->location);
                }
                break;
            case LITTYPE_INSTANCE:
                {
                    return lit_value_asinstance(value)->klass;
                }
                break;
            case LITTYPE_CLASS:
                {
                    return state->classvalue_class;
                }
                break;
            case LITTYPE_ARRAY:
                {
                    return state->arrayvalue_class;
                }
                break;
            case LITTYPE_MAP:
                {
                    return state->mapvalue_class;
                }
                break;
            case LITTYPE_RANGE:
                {
                    return state->rangevalue_class;
                }
                break;
            case LITTYPE_REFERENCE:
                {
                    slot = lit_value_asreference(value)->slot;
                    if(slot != NULL)
                    {
                        return lit_state_getclassfor(state, *slot);
                    }

                    return state->objectvalue_class;
                }
                break;
        }
    }
    else if(lit_value_isnumber(value))
    {
        return state->numbervalue_class;
    }
    else if(lit_value_isbool(value))
    {
        return state->boolvalue_class;
    }
    //fprintf(stderr, "failed to find class object!\n");
    return NULL;
}

static void free_statements(LitState* state, LitAstExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        lit_ast_destroyexpression(state, statements->values[i]);
    }
    lit_exprlist_destroy(state, statements);
}


LitModule* lit_state_compilemodule(LitState* state, LitString* module_name, const char* code, size_t len)
{
    clock_t t;
    clock_t total_t;
    bool allowed_gc;
    LitModule* module;
    LitAstExprList statements;
    allowed_gc = state->allow_gc;
    state->allow_gc = false;
    state->had_error = false;
    module = NULL;
    // This is a lbc format
    if((code[1] << 8 | code[0]) == LIT_BYTECODE_MAGIC_NUMBER)
    {
        module = lit_ioutil_readmodule(state, code, len);
    }
    else
    {
        t = 0;
        total_t = 0;
        if(measure_compilation_time)
        {
            total_t = t = clock();
        }
        if(!lit_preproc_run(state->preprocessor, code))
        {
            return NULL;
        }
        if(measure_compilation_time)
        {
            printf("-----------------------\nPreprocessing:  %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            t = clock();
        }
        lit_exprlist_init(&statements);
        if(lit_parser_parsesource(state->parser, module_name->chars, code, &statements))
        {
            free_statements(state, &statements);
            return NULL;
        }
        if(state->config.dumpast)
        {
            lit_towriter_ast(state, &state->stdoutwriter, &statements);
        }
        if(measure_compilation_time)
        {
            printf("Parsing:        %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            t = clock();
        }
        lit_astopt_optast(state->optimizer, &statements);
        if(measure_compilation_time)
        {
            printf("Optimization:   %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            t = clock();
        }
        module = lit_emitter_modemit(state->emitter, &statements, module_name);
        free_statements(state, &statements);
        if(measure_compilation_time)
        {
            printf("Emitting:       %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            printf("\nTotal:          %gms\n-----------------------\n",
                   (double)(clock() - total_t) / CLOCKS_PER_SEC * 1000 + last_source_time);
        }
    }
    state->allow_gc = allowed_gc;
    return state->had_error ? NULL : module;
}

LitModule* lit_state_getmodule(LitState* state, const char* name)
{
    LitValue value;
    if(lit_table_get(&state->vm->modules->values, CONST_STRING(state, name), &value))
    {
        return lit_value_asmodule(value);
    }
    return NULL;
}

LitInterpretResult lit_state_execsource(LitState* state, const char* module_name, const char* code, size_t len)
{
    return lit_state_internexecsource(state, lit_string_copy(state, module_name, strlen(module_name)), code, len);
}


LitInterpretResult lit_state_internexecsource(LitState* state, LitString* module_name, const char* code, size_t len)
{
    intptr_t istack;
    intptr_t itop;
    intptr_t idif;
    LitModule* module;
    LitFiber* fiber;
    LitInterpretResult result;
    module = lit_state_compilemodule(state, module_name, code, len);
    if(module == NULL)
    {
        return (LitInterpretResult){ LITRESULT_COMPILE_ERROR, NULL_VALUE };
    }
    
    result = lit_vm_execmodule(state, module);
    fiber = module->main_fiber;
    if(!state->had_error && !fiber->abort && fiber->stack_top != fiber->stack)
    {
        istack = (intptr_t)(fiber->stack);
        itop = (intptr_t)(fiber->stack_top);
        idif = (intptr_t)(fiber->stack - fiber->stack_top);
        /* me fail english. how do i put this better? */
        lit_state_raiseerror(state, RUNTIME_ERROR, "stack should be same as stack top", idif, istack, istack, itop, itop);
    }
    state->last_module = module;
    return result;
}


bool lit_state_compileandsave(LitState* state, char* files[], size_t num_files, const char* output_file)
{
    size_t i;
    size_t len;
    char* file_name;
    char* source;
    FILE* file;
    LitString* module_name;
    LitModule* module;
    LitModule** compiled_modules;
    compiled_modules = LIT_ALLOCATE(state, sizeof(LitModule*), num_files+1);
    lit_astopt_setoptlevel(LITOPTLEVEL_EXTREME);
    for(i = 0; i < num_files; i++)
    {
        file_name = lit_util_copystring(files[i]);
        source = lit_util_readfile(file_name, &len);
        if(source == NULL)
        {
            lit_state_raiseerror(state, COMPILE_ERROR, "failed to open file '%s' for reading", file_name);
            return false;
        }
        file_name = lit_util_patchfilename(file_name);
        module_name = lit_string_copy(state, file_name, strlen(file_name));
        module = lit_state_compilemodule(state, module_name, source, len);
        compiled_modules[i] = module;
        free((void*)source);
        free((void*)file_name);
        if(module == NULL)
        {
            return false;
        }
    }
    file = fopen(output_file, "w+b");
    if(file == NULL)
    {
        lit_state_raiseerror(state, COMPILE_ERROR, "failed to open file '%s' for writing", output_file);
        return false;
    }
    lit_ioutil_writeuint16(file, LIT_BYTECODE_MAGIC_NUMBER);
    lit_ioutil_writeuint8(file, LIT_BYTECODE_VERSION);
    lit_ioutil_writeuint16(file, num_files);
    for(i = 0; i < num_files; i++)
    {
        lit_ioutil_writemodule(compiled_modules[i], file);
    }
    lit_ioutil_writeuint16(file, LIT_BYTECODE_END_NUMBER);
    LIT_FREE(state, sizeof(LitModule), compiled_modules);
    fclose(file);
    return true;
}

static char* lit_util_readsource(LitState* state, const char* file, char** patched_file_name, size_t* dlen)
{
    clock_t t;
    size_t len;
    char* file_name;
    char* source;
    t = 0;
    if(measure_compilation_time)
    {
        t = clock();
    }
    file_name = lit_util_copystring(file);
    source = lit_util_readfile(file_name, &len);
    if(source == NULL)
    {
        lit_state_raiseerror(state, RUNTIME_ERROR, "failed to open file '%s' for reading", file_name);
    }
    *dlen = len;
    file_name = lit_util_patchfilename(file_name);
    if(measure_compilation_time)
    {
        printf("reading source: %gms\n", last_source_time = (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
    }
    *patched_file_name = file_name;
    return source;
}

LitInterpretResult lit_state_execfile(LitState* state, const char* file)
{
    size_t len;
    char* source;
    char* patched_file_name;
    LitInterpretResult result;
    source = lit_util_readsource(state, file, &patched_file_name, &len);
    if(source == NULL)
    {
        return INTERPRET_RUNTIME_FAIL;
    }
    result = lit_state_execsource(state, patched_file_name, source, len);
    free((void*)source);
    free(patched_file_name);
    return result;
}

LitInterpretResult lit_state_dumpfile(LitState* state, const char* file)
{
    size_t len;
    char* patched_file_name;
    char* source;
    LitInterpretResult result;
    LitString* module_name;
    LitModule* module;
    source = lit_util_readsource(state, file, &patched_file_name, &len);
    if(source == NULL)
    {
        return INTERPRET_RUNTIME_FAIL;
    }
    module_name = lit_string_copy(state, patched_file_name, strlen(patched_file_name));
    module = lit_state_compilemodule(state, module_name, source, len);
    if(module == NULL)
    {
        result = INTERPRET_RUNTIME_FAIL;
    }
    else
    {
        lit_disassemble_module(state, module, source);
        result = (LitInterpretResult){ LITRESULT_OK, NULL_VALUE };
    }
    free((void*)source);
    free((void*)patched_file_name);
    return result;
}

void lit_state_raiseerror(LitState* state, LitErrType type, const char* message, ...)
{
    size_t buffer_size;
    char* buffer;
    va_list args;
    va_list args_copy;
    (void)type;
    va_start(args, message);
    va_copy(args_copy, args);
    buffer_size = vsnprintf(NULL, 0, message, args_copy) + 1;
    va_end(args_copy);
    buffer = (char*)malloc(buffer_size+1);
    vsnprintf(buffer, buffer_size, message, args);
    va_end(args);
    state->error_fn(state, buffer);
    state->had_error = true;
    /* TODO: is this safe? */
    free(buffer);
}

void lit_state_printf(LitState* state, const char* message, ...)
{
    size_t buffer_size;
    char* buffer;
    va_list args;
    va_start(args, message);
    va_list args_copy;
    va_copy(args_copy, args);
    buffer_size = vsnprintf(NULL, 0, message, args_copy) + 1;
    va_end(args_copy);
    buffer = (char*)malloc(buffer_size+1);
    vsnprintf(buffer, buffer_size, message, args);
    va_end(args);
    state->print_fn(state, buffer);
    free(buffer);
}
