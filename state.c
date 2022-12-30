
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lit.h"
#include "priv.h"

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
    state->bytes_allocated = 0;
    state->next_gc = 256 * 1024;
    state->allow_gc = false;
    state->error_fn = lit_util_default_error;
    state->print_fn = lit_util_default_printf;
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
    lit_init_parser(state, (LitParser*)state->parser);
    state->emitter = (LitEmitter*)malloc(sizeof(LitEmitter));
    lit_init_emitter(state, state->emitter);
    state->optimizer = (LitOptimizer*)malloc(sizeof(LitOptimizer));
    lit_init_optimizer(state, state->optimizer);
    state->vm = (LitVM*)malloc(sizeof(LitVM));
    lit_init_vm(state, state->vm);
    lit_init_api(state);
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
    lit_free_api(state);
    lit_preproc_destroy(state->preprocessor);
    free(state->preprocessor);
    free(state->scanner);
    lit_free_parser(state->parser);
    free(state->parser);
    lit_free_emitter(state->emitter);
    free(state->emitter);
    free(state->optimizer);
    lit_free_vm(state->vm);
    free(state->vm);
    amount = state->bytes_allocated;
    free(state);
    return amount;
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
        switch(OBJECT_TYPE(value))
        {
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
                    upvalue = AS_UPVALUE(value);
                    if(upvalue->location == NULL)
                    {
                        return lit_state_getclassfor(state, upvalue->closed);
                    }
                    return lit_state_getclassfor(state, *upvalue->location);
                }
                break;
            case LITTYPE_INSTANCE:
                {
                    return AS_INSTANCE(value)->klass;
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
                    slot = AS_REFERENCE(value)->slot;
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
    else if(IS_BOOL(value))
    {
        return state->boolvalue_class;
    }
    //fprintf(stderr, "failed to find class object!\n");
    return NULL;
}

static void free_statements(LitState* state, LitExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        lit_free_statement(state, statements->values[i]);
    }
    lit_exprlist_destroy(state, statements);
}

LitInterpretResult lit_state_execsource(LitState* state, const char* module_name, const char* code, size_t len)
{
    return lit_state_internexecsource(state, lit_string_copy(state, module_name, strlen(module_name)), code, len);
}

LitModule* lit_state_compilemodule(LitState* state, LitString* module_name, const char* code, size_t len)
{
    clock_t t;
    clock_t total_t;
    bool allowed_gc;
    LitModule* module;
    LitExprList statements;
    allowed_gc = state->allow_gc;
    state->allow_gc = false;
    state->had_error = false;
    module = NULL;
    // This is a lbc format
    if((code[1] << 8 | code[0]) == LIT_BYTECODE_MAGIC_NUMBER)
    {
        module = lit_load_module(state, code, len);
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
        if(lit_parse(state->parser, module_name->chars, code, &statements))
        {
            free_statements(state, &statements);
            return NULL;
        }
        if(measure_compilation_time)
        {
            printf("Parsing:        %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            t = clock();
        }
        lit_optimize(state->optimizer, &statements);
        if(measure_compilation_time)
        {
            printf("Optimization:   %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            t = clock();
        }
        module = lit_emit(state->emitter, &statements, module_name);
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
        return AS_MODULE(value);
    }
    return NULL;
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
    result = lit_interpret_module(state, module);
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

char* lit_util_patchfilename(char* file_name)
{
    int i;
    int name_length;
    char c;
    name_length = strlen(file_name);
    // Check, if our file_name ends with .lit or lbc, and remove it
    if(name_length > 4 && (memcmp(file_name + name_length - 4, ".lit", 4) == 0 || memcmp(file_name + name_length - 4, ".lbc", 4) == 0))
    {
        file_name[name_length - 4] = '\0';
        name_length -= 4;
    }
    // Check, if our file_name starts with ./ and remove it (useless, and makes the module name be ..main)
    if(name_length > 2 && memcmp(file_name, "./", 2) == 0)
    {
        file_name += 2;
        name_length -= 2;
    }
    for(i = 0; i < name_length; i++)
    {
        c = file_name[i];
        if(c == '/' || c == '\\')
        {
            file_name[i] = '.';
        }
    }
    return file_name;
}

char* lit_util_copystring(const char* string)
{
    size_t length;
    char* new_string;
    length = strlen(string) + 1;
    new_string = (char*)malloc(length);
    memcpy(new_string, string, length);
    return new_string;
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
    lit_set_optimization_level(LITOPTLEVEL_EXTREME);
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
    lit_write_uint16_t(file, LIT_BYTECODE_MAGIC_NUMBER);
    lit_write_uint8_t(file, LIT_BYTECODE_VERSION);
    lit_write_uint16_t(file, num_files);
    for(i = 0; i < num_files; i++)
    {
        lit_save_module(compiled_modules[i], file);
    }
    lit_write_uint16_t(file, LIT_BYTECODE_END_NUMBER);
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

void lit_state_raiseerror(LitState* state, LitErrorType type, const char* message, ...)
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
