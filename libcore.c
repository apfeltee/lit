
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#if defined(__unix__) || defined(__linux__)
#include <dirent.h>
#endif
#include "lit.h"

void lit_open_libraries(LitState* state)
{
    lit_open_math_library(state);
    lit_open_file_library(state);
    lit_open_gc_library(state);
}

#if 0
#define COMPARE(state, callee, a, b) \
    ( \
    { \
        LitValue argv[2]; \
        argv[0] = a; \
        argv[1] = b; \
        LitInterpretResult r = lit_call(state, callee, argv, 2, false); \
        if(r.type != LITRESULT_OK) \
        { \
            return; \
        } \
        !lit_is_falsey(r.result); \
    })
#else
static LitInterpretResult COMPARE_inl(LitState* state, LitValue callee, LitValue a, LitValue b)
{
    LitValue argv[2];
    argv[0] = a;
    argv[1] = b;
    return lit_call(state, callee, argv, 2, false);
}

#define COMPARE(state, callee, a, b) \
    COMPARE_inl(state, callee, a, b)
#endif


static bool should_update_locals;

void util_custom_quick_sort(LitVM* vm, LitValue* l, int length, LitValue callee)
{
    LitInterpretResult rt;
    LitState* state;
    if(length < 2)
    {
        return;
    }
    state = vm->state;
    int pivot_index = length / 2;
    int i;
    int j;
    LitValue pivot = l[pivot_index];
    for(i = 0, j = length - 1;; i++, j--)
    {
        //while(i < pivot_index && COMPARE(state, callee, l[i], pivot))
        while(i < pivot_index)
        {
            if((rt = COMPARE(state, callee, l[i], pivot)).type != LITRESULT_OK)
            {
                return;
            }
            if(lit_is_falsey(rt.result))
            {
                break;
            }
            i++;
        }
        //while(j > pivot_index && COMPARE(state, callee, pivot, l[j]))
        while(j > pivot_index)
        {
            if((rt = COMPARE(state, callee, pivot, l[j])).type != LITRESULT_OK)
            {
                return;
            }
            if(lit_is_falsey(rt.result))
            {
                break;
            }
            j--;
        }
        if(i >= j)
        {
            break;
        }
        LitValue tmp = l[i];
        l[i] = l[j];
        l[j] = tmp;
    }
    util_custom_quick_sort(vm, l, i, callee);
    util_custom_quick_sort(vm, l + i, length - i, callee);
}

bool util_is_fiber_done(LitFiber* fiber)
{
    return fiber->frame_count == 0 || fiber->abort;
}

void util_run_fiber(LitVM* vm, LitFiber* fiber, LitValue* argv, size_t argc, bool catcher)
{
    bool vararg;
    int i;
    int to;
    int vararg_count;
    int objfn_function_arg_count;
    LitArray* array;
    LitCallFrame* frame;
    if(util_is_fiber_done(fiber))
    {
        lit_runtime_error_exiting(vm, "Fiber already finished executing");
    }
    fiber->parent = vm->fiber;
    fiber->catcher = catcher;
    vm->fiber = fiber;
    frame = &fiber->frames[fiber->frame_count - 1];
    if(frame->ip == frame->function->chunk.code)
    {
        fiber->arg_count = argc;
        lit_ensure_fiber_stack(vm->state, fiber, frame->function->max_slots + 1 + (int)(fiber->stack_top - fiber->stack));
        frame->slots = fiber->stack_top;
        lit_push(vm, OBJECT_VALUE(frame->function));
        vararg = frame->function->vararg;
        objfn_function_arg_count = frame->function->arg_count;
        to = objfn_function_arg_count - (vararg ? 1 : 0);
        fiber->arg_count = objfn_function_arg_count;
        for(i = 0; i < to; i++)
        {
            lit_push(vm, i < (int)argc ? argv[i] : NULL_VALUE);
        }
        if(vararg)
        {
            array = lit_create_array(vm->state);
            lit_push(vm, OBJECT_VALUE(array));
            vararg_count = argc - objfn_function_arg_count + 1;
            if(vararg_count > 0)
            {
                lit_values_ensure_size(vm->state, &array->values, vararg_count);
                for(i = 0; i < vararg_count; i++)
                {
                    array->values.values[i] = argv[i + objfn_function_arg_count - 1];
                }
            }
        }
    }
}



static inline bool compare(LitState* state, LitValue a, LitValue b)
{
    LitValue argv[1];
    if(IS_NUMBER(a) && IS_NUMBER(b))
    {
        return lit_value_to_number(a) < lit_value_to_number(b);
    }
    argv[0] = b;
    return !lit_is_falsey(lit_find_and_call_method(state, a, CONST_STRING(state, "<"), argv, 1, false).result);
}

void util_basic_quick_sort(LitState* state, LitValue* clist, int length)
{
    int i;
    int j;
    int pivot_index;
    LitValue tmp;
    LitValue pivot;
    if(length < 2)
    {
        return;
    }
    pivot_index = length / 2;
    pivot = clist[pivot_index];
    for(i = 0, j = length - 1;; i++, j--)
    {
        while(i < pivot_index && compare(state, clist[i], pivot))
        {
            i++;
        }

        while(j > pivot_index && compare(state, pivot, clist[j]))
        {
            j--;
        }

        if(i >= j)
        {
            break;
        }
        tmp = clist[i];
        clist[i] = clist[j];
        clist[j] = tmp;
    }
    util_basic_quick_sort(state, clist, i);
    util_basic_quick_sort(state, clist + i, length - i);
}

bool util_interpret(LitVM* vm, LitModule* module)
{
    LitFunction* function;
    LitFiber* fiber;
    LitCallFrame* frame;
    function = module->main_function;
    fiber = lit_create_fiber(vm->state, module, function);
    fiber->parent = vm->fiber;
    vm->fiber = fiber;
    frame = &fiber->frames[fiber->frame_count - 1];
    if(frame->ip == frame->function->chunk.code)
    {
        frame->slots = fiber->stack_top;
        lit_push(vm, OBJECT_VALUE(frame->function));
    }
    return true;
}

static bool compile_and_interpret(LitVM* vm, LitString* modname, char* source)
{
    LitModule* module;
    module = lit_compile_module(vm->state, modname, source);
    if(module == NULL)
    {
        return false;
    }
    module->ran = true;
    return util_interpret(vm, module);
}

bool util_test_file_exists(const char* filename)
{
    struct stat buffer;
    return stat(filename, &buffer) == 0;
}

#if 0
bool util_attempt_to_require(LitVM* vm, LitValue* argv, size_t argc, const char* path, bool ignore_previous, bool folders)
{
    bool rt;
    bool found;
    size_t i;
    size_t length;
    char c;
    char* source;
    char* dir_path;
    char* modname;
    char* modnamedotted;
    found = false;
    length = strlen(path);
    should_update_locals = false;
    if(path[length - 2] == '.' && path[length - 1] == '*')
    {
        if(folders)
        {
            lit_runtime_error_exiting(vm, "cannot recursively require folders");
        }
        dir_path = LIT_ALLOCATE(vm->state, char, length+1);
        dir_path[length - 2] = '\0';
        memcpy((void*)dir_path, path, length - 2);
        rt = util_attempt_to_require(vm, argv, argc, dir_path, ignore_previous, true);
        LIT_FREE(vm->state, char, dir_path);
        return rt;
    }
    //char modname[length + 5];
    modname = LIT_ALLOCATE(vm->state, char, length+5);
    //char modnamedotted[length + 5];
    modnamedotted = LIT_ALLOCATE(vm->state, char, length+5);
    memcpy((void*)modnamedotted, path, length);
    memcpy((void*)modnamedotted + length, ".lit", 4);
    modnamedotted[length + 4] = '\0';
    for(i = 0; i < length + 5; i++)
    {
        c = modnamedotted[i];
        if(c == '.' || c == '\\')
        {
            modname[i] = '/';
        }
        else
        {
            modname[i] = c;
        }
    }
    // You can require dirs if they have init.lit in them
    modname[length] = '\0';
    if(lit_dir_exists(modname))
    {
        if(folders)
        {
            #if defined(__unix__) || defined(__linux__)
            {
                struct dirent* ep;
                DIR* dir = opendir(modname);
                if(dir == NULL)
                {
                    lit_runtime_error_exiting(vm, "failed to open folder '%s'", modname);
                }
                while((ep = readdir(dir)))
                {
                    if(ep->d_type == DT_REG)
                    {
                        const char* name = ep->d_name;
                        int name_length = strlen(name);
                        if(name_length > 4 && (strcmp(name + name_length - 4, ".lit") == 0 || strcmp(name + name_length - 4, ".lbc")))
                        {
                            char dir_path[length + name_length - 2];
                            dir_path[length + name_length - 3] = '\0';
                            memcpy((void*)dir_path, path, length);
                            memcpy((void*)dir_path + length + 1, name, name_length - 4);
                            dir_path[length] = '.';
                            if(!util_attempt_to_require(vm, argv + argc, 0, dir_path, false, false))
                            {
                                lit_runtime_error_exiting(vm, "failed to require module '%s'", name);
                            }
                            else
                            {
                                found = true;
                            }
                        }
                    }
                }
                closedir(dir);
            }
            #endif
            if(!found)
            {
                lit_runtime_error_exiting(vm, "folder '%s' contains no modules that can be required", modname);
            }
            return found;
        }
        else
        {
            char dir_name[length + 6];
            dir_name[length + 5] = '\0';
            memcpy((void*)dir_name, modname, length);
            memcpy((void*)dir_name + length, ".init", 5);
            if(util_attempt_to_require(vm, argv, argc, dir_name, ignore_previous, false))
            {
                return true;
            }
        }
    }
    else if(folders)
    {
        return false;
    }
    modname[length] = '.';
    LitString* name = lit_string_copy(vm->state, modnamedotted, length);
    if(!ignore_previous)
    {
        LitValue existing_module;
        if(lit_table_get(&vm->modules->values, name, &existing_module))
        {
            LitModule* loaded_module = AS_MODULE(existing_module);
            if(loaded_module->ran)
            {
                vm->fiber->stack_top -= argc;
                argv[-1] = AS_MODULE(existing_module)->return_value;
            }
            else
            {
                if(util_interpret(vm, loaded_module))
                {
                    should_update_locals = true;
                }
            }
            return true;
        }
    }
    if(!util_test_file_exists(modname))
    {
        // .lit -> .lbc
        memcpy((void*)modname + length + 2, "bc", 2);
        if(!util_test_file_exists(modname))
        {
            return false;
        }
    }
    source = lit_read_file(modname);
    if(source == NULL)
    {
        return false;
    }
    if(compile_and_interpret(vm, name, source))
    {
        should_update_locals = true;
    }
    free(source);
    return true;
}


bool util_attempt_to_require_combined(LitVM* vm, LitValue* argv, size_t argc, const char* a, const char* b, bool ignore_previous)
{
    bool rt;
    size_t a_length;
    size_t b_length;
    size_t total_length;
    char* path;
    a_length = strlen(a);
    b_length = strlen(b);
    total_length = a_length + b_length + 1;
    path = LIT_ALLOCATE(vm->state, char, total_length+1);
    memcpy((void*)path, a, a_length);
    memcpy((void*)path + a_length + 1, b, b_length);
    path[a_length] = '.';
    path[total_length] = '\0';
    rt = util_attempt_to_require(vm, argv, argc, (const char*)&path, ignore_previous, false);
    LIT_FREE(vm->state, char, path);
    return rt;
}
#endif

LitValue util_invalid_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    lit_runtime_error_exiting(vm, "cannot create an instance of built-in type", AS_INSTANCE(instance)->klass->name);
    return NULL_VALUE;
}

static LitValue objfn_number_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_number_to_string(vm->state, lit_value_to_number(instance)));
}

static LitValue objfn_number_tochar(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    char ch;
    (void)argc;
    (void)argv;
    ch = lit_value_to_number(instance);
    return OBJECT_VALUE(lit_string_copy(vm->state, &ch, 1));
}

static LitValue objfn_bool_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_CONST_STRING(vm->state, AS_BOOL(instance) ? "true" : "false");
}

static LitValue cfn_time(LitVM* vm, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value((double)clock() / CLOCKS_PER_SEC);
}

static LitValue cfn_systemTime(LitVM* vm, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(time(NULL));
}

static LitValue cfn_print(LitVM* vm, size_t argc, LitValue* argv)
{
    size_t i;
    if(argc == 0)
    {
        return NULL_VALUE;
    }
    for(i = 0; i < argc; i++)
    {
        lit_printf(vm->state, "%s", lit_to_string(vm->state, argv[i])->chars);
    }
    return NULL_VALUE;
}

static LitValue cfn_println(LitVM* vm, size_t argc, LitValue* argv)
{
    size_t i;
    if(argc == 0)
    {
        return NULL_VALUE;
    }
    for(i = 0; i < argc; i++)
    {
        lit_printf(vm->state, "%s", lit_to_string(vm->state, argv[i])->chars);
    }
    lit_printf(vm->state, "\n");
    return NULL_VALUE;
}

static bool cfn_eval(LitVM* vm, size_t argc, LitValue* argv)
{
    char* code;
    (void)argc;
    (void)argv;
    code = (char*)lit_check_string(vm, argv, argc, 0);
    return compile_and_interpret(vm, vm->fiber->module->name, code);
}

#if 0
static bool cfn_require(LitVM* vm, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    bool ignore_previous;
    size_t length;
    char* buffer;
    char* index;
    LitString* name;
    LitString* modname;
    name = lit_check_object_string(vm, argv, argc, 0);
    ignore_previous = argc > 1 && IS_BOOL(argv[1]) && AS_BOOL(argv[1]);
    // First check, if a file with this name exists in the local path
    if(util_attempt_to_require(vm, argv, argc, name->chars, ignore_previous, false))
    {
        return should_update_locals;
    }
    // If not, we join the path of the current module to it (the path goes all the way from the root)
    modname = vm->fiber->module->name;
    // We need to get rid of the module name (test.folder.module -> test.folder)
    index = strrchr(modname->chars, '.');
    if(index != NULL)
    {
        length = index - modname->chars;
        buffer = (char*)malloc(length + 1);
        //char buffer[length + 1];
        memcpy((void*)buffer, modname->chars, length);
        buffer[length] = '\0';
        if(util_attempt_to_require_combined(vm, argv, argc, (const char*)&buffer, name->chars, ignore_previous))
        {
            free(buffer);
            return should_update_locals;
        }
        else
        {
            free(buffer);
        }
    }
    lit_runtime_error_exiting(vm, "failed to require module '%s'", name->chars);
    return false;
}
#endif

void lit_open_string_library(LitState* state);
void lit_open_array_library(LitState* state);
void lit_open_map_library(LitState* state);
void lit_open_range_library(LitState* state);
void lit_open_fiber_library(LitState* state);
void lit_open_module_library(LitState* state);
void lit_open_function_library(LitState* state);
void lit_open_class_library(LitState* state);
void lit_open_object_library(LitState* state);


void lit_open_core_library(LitState* state)
{
    /*
    * the order here is important: class must be declared first, and object second,
    * since object derives class, and everything else derives object.
    */
    {
        lit_open_class_library(state);
        lit_open_object_library(state);
        lit_open_string_library(state);
        lit_open_array_library(state);
        lit_open_map_library(state);
        lit_open_range_library(state);
        lit_open_fiber_library(state);
        lit_open_module_library(state);
        lit_open_function_library(state);
    }
    {
        LIT_BEGIN_CLASS("Number");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(util_invalid_constructor);
            LIT_BIND_METHOD("toString", objfn_number_tostring);
            LIT_BIND_METHOD("toChar", objfn_number_tochar);
            LIT_BIND_GETTER("chr", objfn_number_tochar);
            state->numbervalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Bool");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(util_invalid_constructor);
            LIT_BIND_METHOD("toString", objfn_bool_tostring);
            state->boolvalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        lit_define_native(state, "time", cfn_time);
        lit_define_native(state, "systemTime", cfn_systemTime);
        lit_define_native(state, "print", cfn_print);
        lit_define_native(state, "println", cfn_println);
        //lit_define_native_primitive(state, "require", cfn_require);
        lit_define_native_primitive(state, "eval", cfn_eval);
        lit_set_global(state, CONST_STRING(state, "globals"), OBJECT_VALUE(state->vm->globals));
    }
}
