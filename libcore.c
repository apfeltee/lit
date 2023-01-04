
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

static bool should_update_locals;


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
        LitInterpretResult r = lit_state_callvalue(state, callee, argv, 2, false); \
        if(r.type != LITRESULT_OK) \
        { \
            return; \
        } \
        !lit_value_isfalsey(r.result); \
    })
#else
static LitInterpretResult COMPARE_inl(LitState* state, LitValue callee, LitValue a, LitValue b)
{
    LitValue argv[2];
    argv[0] = a;
    argv[1] = b;
    return lit_state_callvalue(state, callee, argv, 2, false);
}

#define COMPARE(state, callee, a, b) \
    COMPARE_inl(state, callee, a, b)
#endif



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
            if(lit_value_isfalsey(rt.result))
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
            if(lit_value_isfalsey(rt.result))
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
        lit_vm_raiseexitingerror(vm, "Fiber already finished executing");
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
        lit_vm_push(vm, lit_value_objectvalue(frame->function));
        vararg = frame->function->vararg;
        objfn_function_arg_count = frame->function->arg_count;
        to = objfn_function_arg_count - (vararg ? 1 : 0);
        fiber->arg_count = objfn_function_arg_count;
        for(i = 0; i < to; i++)
        {
            lit_vm_push(vm, i < (int)argc ? argv[i] : NULL_VALUE);
        }
        if(vararg)
        {
            array = lit_create_array(vm->state);
            lit_vm_push(vm, lit_value_objectvalue(array));
            vararg_count = argc - objfn_function_arg_count + 1;
            if(vararg_count > 0)
            {
                lit_vallist_ensuresize(vm->state, &array->list, vararg_count);
                for(i = 0; i < vararg_count; i++)
                {
                    lit_vallist_set(&array->list, i, argv[i + objfn_function_arg_count - 1]);
                }
            }
        }
    }
}

static inline bool compare(LitState* state, LitValue a, LitValue b)
{
    LitValue argv[1];
    if(lit_value_isnumber(a) && lit_value_isnumber(b))
    {
        return lit_value_asnumber(a) < lit_value_asnumber(b);
    }
    argv[0] = b;
    return !lit_value_isfalsey(lit_state_findandcallmethod(state, a, CONST_STRING(state, "<"), argv, 1, false).result);
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
        lit_vm_push(vm, lit_value_objectvalue(frame->function));
    }
    return true;
}

static bool compile_and_interpret(LitVM* vm, LitString* modname, char* source, size_t len)
{
    LitModule* module;
    module = lit_state_compilemodule(vm->state, modname, source, len);
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

#if 1
bool util_attempt_to_require(LitVM* vm, LitValue* argv, size_t argc, const char* path, bool ignore_previous, bool folders)
{
    bool rt;
    bool found;
    size_t i;
    size_t flen;
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
            lit_vm_raiseexitingerror(vm, "cannot recursively require folders");
        }
        dir_path = LIT_ALLOCATE(vm->state, sizeof(char), length+1);
        dir_path[length - 2] = '\0';
        memcpy((void*)dir_path, path, length - 2);
        rt = util_attempt_to_require(vm, argv, argc, dir_path, ignore_previous, true);
        LIT_FREE(vm->state, sizeof(char), dir_path);
        return rt;
    }
    //char modname[length + 5];
    modname = LIT_ALLOCATE(vm->state, sizeof(char), length+5);
    //char modnamedotted[length + 5];
    modnamedotted = LIT_ALLOCATE(vm->state, sizeof(char), length+5);
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
                    lit_vm_raiseexitingerror(vm, "failed to open folder '%s'", modname);
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
                                lit_vm_raiseexitingerror(vm, "failed to require module '%s'", name);
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
                lit_vm_raiseexitingerror(vm, "folder '%s' contains no modules that can be required", modname);
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
            LitModule* loaded_module = lit_value_asmodule(existing_module);
            if(loaded_module->ran)
            {
                vm->fiber->stack_top -= argc;
                argv[-1] = lit_value_asmodule(existing_module)->return_value;
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
    source = lit_util_readfile(modname, &flen);
    if(source == NULL)
    {
        return false;
    }
    if(compile_and_interpret(vm, name, source, flen))
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
    path = LIT_ALLOCATE(vm->state, sizeof(char), total_length+1);
    memcpy((void*)path, a, a_length);
    memcpy((void*)path + a_length + 1, b, b_length);
    path[a_length] = '.';
    path[total_length] = '\0';
    rt = util_attempt_to_require(vm, argv, argc, (const char*)&path, ignore_previous, false);
    LIT_FREE(vm->state, sizeof(char), path);
    return rt;
}
#endif

LitValue util_invalid_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    lit_vm_raiseexitingerror(vm, "cannot create an instance of built-in type", lit_value_asinstance(instance)->klass->name);
    return NULL_VALUE;
}

static LitValue objfn_number_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_value_objectvalue(lit_string_numbertostring(vm->state, lit_value_asnumber(instance)));
}

static LitValue objfn_number_tochar(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    char ch;
    (void)argc;
    (void)argv;
    ch = lit_value_asnumber(instance);
    return lit_value_objectvalue(lit_string_copy(vm->state, &ch, 1));
}

static LitValue objfn_bool_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    bool bv;
    (void)vm;
    (void)argc;
    bv = lit_value_asbool(instance);
    if(lit_value_isnull(argv[0]))
    {
        return lit_bool_to_value(vm->state, false);
    }
    return lit_bool_to_value(vm->state, lit_value_asbool(argv[0]) == bv);
}

static LitValue objfn_bool_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_CONST_STRING(vm->state, lit_value_asbool(instance) ? "true" : "false");
}

static LitValue cfn_time(LitVM* vm, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_numbertovalue(vm->state, (double)clock() / CLOCKS_PER_SEC);
}

static LitValue cfn_systemTime(LitVM* vm, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_numbertovalue(vm->state, time(NULL));
}

static LitValue cfn_print(LitVM* vm, size_t argc, LitValue* argv)
{
    size_t i;
    size_t written = 0;
    LitString* sv;
    written = 0;
    if(argc == 0)
    {
        return lit_value_numbertovalue(vm->state, 0);
    }
    for(i = 0; i < argc; i++)
    {
        sv = lit_value_tostring(vm->state, argv[i]);
        written += fwrite(sv->chars, sizeof(char), lit_string_getlength(sv), stdout);
    }
    return lit_value_numbertovalue(vm->state, written);
}

static LitValue cfn_println(LitVM* vm, size_t argc, LitValue* argv)
{
    LitValue r;
    r = cfn_print(vm, argc, argv);
    fprintf(stdout, "\n");
    return r;
}

static bool cfn_eval(LitVM* vm, size_t argc, LitValue* argv)
{
    LitString* sc;
    (void)argc;
    (void)argv;
    sc = lit_value_checkobjstring(vm, argv, argc, 0);
    return compile_and_interpret(vm, vm->fiber->module->name, sc->chars, lit_string_getlength(sc));
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
    name = lit_value_checkobjstring(vm, argv, argc, 0);
    ignore_previous = argc > 1 && lit_value_isbool(argv[1]) && lit_value_asbool(argv[1]);
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
    lit_vm_raiseexitingerror(vm, "failed to require module '%s'", name->chars);
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
    LitClass* klass;
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
        klass = lit_create_classobject(state, "Number");
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
            lit_class_bindconstructor(state, klass, util_invalid_constructor);
            lit_class_bindmethod(state, klass, "toString", objfn_number_tostring);
            lit_class_bindmethod(state, klass, "toChar", objfn_number_tochar);
            lit_class_bindgetset(state, klass, "chr", objfn_number_tochar, NULL, false);
            state->numbervalue_class = klass;
        }
        lit_state_setglobal(state, klass->name, lit_value_objectvalue(klass));
        if(klass->super == NULL)
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
        };
    }
    {
        klass = lit_create_classobject(state, "Bool");
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
            lit_class_bindconstructor(state, klass, util_invalid_constructor);
            lit_class_bindmethod(state, klass, "==", objfn_bool_compare);
            lit_class_bindmethod(state, klass, "toString", objfn_bool_tostring);
            state->boolvalue_class = klass;
        }
        lit_state_setglobal(state, klass->name, lit_value_objectvalue(klass));
        if(klass->super == NULL)
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
        };
    }
    {
        lit_state_defnativefunc(state, "time", cfn_time);
        lit_state_defnativefunc(state, "systemTime", cfn_systemTime);
        lit_state_defnativefunc(state, "print", cfn_print);
        lit_state_defnativefunc(state, "println", cfn_println);
        //lit_state_defnativeprimitive(state, "require", cfn_require);
        lit_state_defnativeprimitive(state, "eval", cfn_eval);
        lit_state_setglobal(state, CONST_STRING(state, "globals"), lit_value_objectvalue(state->vm->globals));
    }
}
