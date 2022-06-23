
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
        LitInterpretResult r = lit_call(state, callee, argv, 2); \
        if(r.type != INTERPRET_OK) \
        { \
            return; \
        } \
        !lit_is_falsey(r.result); \
    })
#else
static inline LitInterpretResult COMPARE_inl(LitState* state, LitValue callee, LitValue a, LitValue b)
{
    LitValue argv[2];
    argv[0] = a;
    argv[1] = b;
    return lit_call(state, callee, argv, 2);
}

#define COMPARE(state, callee, a, b) \
    COMPARE_inl(state, callee, a, b)
#endif

static void custom_quick_sort(LitVm* vm, LitValue* l, int length, LitValue callee)
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
            if((rt = COMPARE(state, callee, l[i], pivot)).type != INTERPRET_OK)
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
            if((rt = COMPARE(state, callee, pivot, l[j])).type != INTERPRET_OK)
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
    custom_quick_sort(vm, l, i, callee);
    custom_quick_sort(vm, l + i, length - i, callee);
}


static int table_iterator(LitTable* table, int number)
{
    if(table->count == 0)
    {
        return -1;
    }
    if(number >= (int)table->capacity)
    {
        return -1;
    }
    number++;
    for(; number < table->capacity; number++)
    {
        if(table->entries[number].key != NULL)
        {
            return number;
        }
    }

    return -1;
}

static LitValue table_iterator_key(LitTable* table, int index)
{
    if(table->capacity <= index)
    {
        return NULL_VALUE;
    }
    return OBJECT_VALUE(table->entries[index].key);
}

static LitValue objfn_string_splice(LitVm* vm, LitString* string, int from, int to)
{
    int length = lit_ustring_length(string);
    if(from < 0)
    {
        from = length + from;
    }
    if(to < 0)
    {
        to = length + to;
    }
    from = fmax(from, 0);
    to = fmin(to, length - 1);
    if(from > to)
    {
        lit_runtime_error_exiting(vm, "String.splice argument 'from' is larger than argument 'to'");
    }
    from = lit_uchar_offset(string->chars, from);
    to = lit_uchar_offset(string->chars, to);
    return OBJECT_VALUE(lit_ustring_from_range(vm->state, string, from, to - from + 1));
}

static bool is_fiber_done(LitFiber* fiber)
{
    return fiber->frame_count == 0 || fiber->abort;
}

static void run_fiber(LitVm* vm, LitFiber* fiber, LitValue* argv, size_t argc, bool catcher)
{
    if(is_fiber_done(fiber))
    {
        lit_runtime_error_exiting(vm, "Fiber already finished executing");
    }
    fiber->parent = vm->fiber;
    fiber->catcher = catcher;
    vm->fiber = fiber;
    LitCallFrame* frame = &fiber->frames[fiber->frame_count - 1];
    if(frame->ip == frame->function->chunk.code)
    {
        fiber->arg_count = argc;
        lit_ensure_fiber_stack(vm->state, fiber, frame->function->max_slots + 1 + (int)(fiber->stack_top - fiber->stack));
        frame->slots = fiber->stack_top;
        lit_push(vm, OBJECT_VALUE(frame->function));
        bool vararg = frame->function->vararg;
        int objfn_function_arg_count = frame->function->arg_count;
        int to = objfn_function_arg_count - (vararg ? 1 : 0);
        fiber->arg_count = objfn_function_arg_count;
        for(int i = 0; i < to; i++)
        {
            lit_push(vm, i < (int)argc ? argv[i] : NULL_VALUE);
        }
        if(vararg)
        {
            LitArray* array = lit_create_array(vm->state);
            lit_push(vm, OBJECT_VALUE(array));
            int vararg_count = argc - objfn_function_arg_count + 1;
            if(vararg_count > 0)
            {
                lit_values_ensure_size(vm->state, &array->values, vararg_count);
                for(int i = 0; i < vararg_count; i++)
                {
                    array->values.values[i] = argv[i + objfn_function_arg_count - 1];
                }
            }
        }
    }
}


static LitValue objfn_array_splice(LitVm* vm, LitArray* array, int from, int to)
{
    size_t length = array->values.count;
    if(from < 0)
    {
        from = (int)length + from;
    }
    if(to < 0)
    {
        to = (int)length + to;
    }
    if(from > to)
    {
        lit_runtime_error_exiting(vm, "Array.splice argument 'from' is larger than argument 'to'");
    }
    from = fmax(from, 0);
    to = fmin(to, (int)length - 1);
    length = fmin(length, to - from + 1);
    LitArray* new_array = lit_create_array(vm->state);
    for(size_t i = 0; i < length; i++)
    {
        lit_values_write(vm->state, &new_array->values, array->values.values[from + i]);
    }
    return OBJECT_VALUE(new_array);
}

static int indexOf(LitArray* array, LitValue value)
{
    for(size_t i = 0; i < array->values.count; i++)
    {
        if(array->values.values[i] == value)
        {
            return (int)i;
        }
    }
    return -1;
}


static LitValue removeAt(LitArray* array, size_t index)
{
    LitValues* values = &array->values;
    size_t count = values->count;
    if(index >= count)
    {
        return NULL_VALUE;
    }
    LitValue value = values->values[index];
    if(index == count - 1)
    {
        values->values[index] = NULL_VALUE;
    }
    else
    {
        for(size_t i = index; i < values->count - 1; i++)
        {
            values->values[i] = values->values[i + 1];
        }
        values->values[count - 1] = NULL_VALUE;
    }
    values->count--;
    return value;
}

static inline bool compare(LitState* state, LitValue a, LitValue b)
{
    LitValue argv[1];
    if(IS_NUMBER(a) && IS_NUMBER(b))
    {
        return lit_value_to_number(a) < lit_value_to_number(b);
    }
    argv[0] = b;
    return !lit_is_falsey(lit_find_and_call_method(state, a, CONST_STRING(state, "<"), argv, 1).result);
}

static void basic_quick_sort(LitState* state, LitValue* l, int length)
{
    if(length < 2)
    {
        return;
    }
    int pivot_index = length / 2;
    int i;
    int j;
    LitValue pivot = l[pivot_index];
    for(i = 0, j = length - 1;; i++, j--)
    {
        while(i < pivot_index && compare(state, l[i], pivot))
        {
            i++;
        }

        while(j > pivot_index && compare(state, pivot, l[j]))
        {
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
    basic_quick_sort(state, l, i);
    basic_quick_sort(state, l + i, length - i);
}

static bool interpret(LitVm* vm, LitModule* module)
{
    LitFunction* function = module->main_function;
    LitFiber* fiber = lit_create_fiber(vm->state, module, function);
    fiber->parent = vm->fiber;
    vm->fiber = fiber;
    LitCallFrame* frame = &fiber->frames[fiber->frame_count - 1];
    if(frame->ip == frame->function->chunk.code)
    {
        frame->slots = fiber->stack_top;
        lit_push(vm, OBJECT_VALUE(frame->function));
    }
    return true;
}


static bool compile_and_interpret(LitVm* vm, LitString* objfn_module_name, char* source)
{
    LitModule* module = lit_compile_module(vm->state, objfn_module_name, source);
    if(module == NULL)
    {
        return false;
    }
    module->ran = true;
    return interpret(vm, module);
}

static bool test_file_exists(const char* filename)
{
    struct stat buffer;
    return stat(filename, &buffer) == 0;
}

static bool should_update_locals;
static bool attempt_to_require_combined(LitVm* vm, LitValue* argv, size_t argc, const char* a, const char* b, bool ignore_previous);

static bool attempt_to_require(LitVm* vm, LitValue* argv, size_t argc, const char* path, bool ignore_previous, bool folders)
{
    bool found;
    size_t length;
    found = false;
    length = strlen(path);
    should_update_locals = false;
    if(path[length - 2] == '.' && path[length - 1] == '*')
    {
        if(folders)
        {
            lit_runtime_error_exiting(vm, "cannot recursively require folders");
        }
        char dir_path[length - 1];
        dir_path[length - 2] = '\0';
        memcpy((void*)dir_path, path, length - 2);
        return attempt_to_require(vm, argv, argc, dir_path, ignore_previous, true);
    }
    char objfn_module_name[length + 5];
    char objfn_module_name_dotted[length + 5];
    memcpy((void*)objfn_module_name_dotted, path, length);
    memcpy((void*)objfn_module_name_dotted + length, ".lit", 4);
    objfn_module_name_dotted[length + 4] = '\0';
    for(size_t i = 0; i < length + 5; i++)
    {
        char c = objfn_module_name_dotted[i];
        if(c == '.' || c == '\\')
        {
            objfn_module_name[i] = '/';
        }
        else
        {
            objfn_module_name[i] = c;
        }
    }
    // You can require dirs if they have init.lit in them
    objfn_module_name[length] = '\0';
    if(lit_dir_exists(objfn_module_name))
    {
        if(folders)
        {
            #if defined(__unix__) || defined(__linux__)
            {
                struct dirent* ep;
                DIR* dir = opendir(objfn_module_name);
                if(dir == NULL)
                {
                    lit_runtime_error_exiting(vm, "failed to open folder '%s'", objfn_module_name);
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
                            if(!attempt_to_require(vm, argv + argc, 0, dir_path, false, false))
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
            }
            #endif
            if(!found)
            {
                lit_runtime_error_exiting(vm, "folder '%s' contains no modules that can be required", objfn_module_name);
            }
            return found;
        }
        else
        {
            char dir_name[length + 6];
            dir_name[length + 5] = '\0';
            memcpy((void*)dir_name, objfn_module_name, length);
            memcpy((void*)dir_name + length, ".init", 5);
            if(attempt_to_require(vm, argv, argc, dir_name, ignore_previous, false))
            {
                return true;
            }
        }
    }
    else if(folders)
    {
        return false;
    }
    objfn_module_name[length] = '.';
    LitString* name = lit_copy_string(vm->state, objfn_module_name_dotted, length);
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
                if(interpret(vm, loaded_module))
                {
                    should_update_locals = true;
                }
            }
            return true;
        }
    }
    if(!test_file_exists(objfn_module_name))
    {
        // .lit -> .lbc
        memcpy((void*)objfn_module_name + length + 2, "bc", 2);
        if(!test_file_exists(objfn_module_name))
        {
            return false;
        }
    }
    char* source = lit_read_file(objfn_module_name);
    if(source == NULL)
    {
        return false;
    }
    if(compile_and_interpret(vm, name, source))
    {
        should_update_locals = true;
    }
    return true;
}

static bool attempt_to_require_combined(LitVm* vm, LitValue* argv, size_t argc, const char* a, const char* b, bool ignore_previous)
{
    size_t a_length = strlen(a);
    size_t b_length = strlen(b);
    size_t total_length = a_length + b_length + 1;

    char path[total_length + 1];

    memcpy((void*)path, a, a_length);
    memcpy((void*)path + a_length + 1, b, b_length);

    path[a_length] = '.';
    path[total_length] = '\0';

    return attempt_to_require(vm, argv, argc, (const char*)&path, ignore_previous, false);
}


static LitValue invalid_constructor(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    lit_runtime_error_exiting(vm, "cannot create an instance of built-in type", AS_INSTANCE(instance)->klass->name);
    return NULL_VALUE;
}


/*
 * Class
 */

static LitValue objfn_class_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_format(vm->state, "class @", OBJECT_VALUE(AS_CLASS(instance)->name)));
}


static LitValue objfn_class_iterator(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    LIT_ENSURE_ARGS(1);
    LitClass* klass = AS_CLASS(instance);
    int index = argv[0] == NULL_VALUE ? -1 : lit_value_to_number(argv[0]);
    int methodsCapacity = (int)klass->methods.capacity;
    bool fields = index >= methodsCapacity;

    int value = table_iterator(fields ? &klass->static_fields : &klass->methods, fields ? index - methodsCapacity : index);

    if(value == -1)
    {
        if(fields)
        {
            return NULL_VALUE;
        }

        index++;
        fields = true;
        value = table_iterator(&klass->static_fields, index - methodsCapacity);
    }

    return value == -1 ? NULL_VALUE : lit_number_to_value(fields ? value + methodsCapacity : value);
}


static LitValue objfn_class_iteratorvalue(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    size_t index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    LitClass* klass = AS_CLASS(instance);
    size_t methodsCapacity = klass->methods.capacity;
    bool fields = index >= methodsCapacity;

    return table_iterator_key(fields ? &klass->static_fields : &klass->methods, fields ? index - methodsCapacity : index);
}


static LitValue objfn_class_super(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitClass* super;
    (void)vm;
    (void)argc;
    (void)argv;
    super = NULL;
    if(IS_INSTANCE(instance))
    {
        super = AS_INSTANCE(instance)->klass->super;
    }
    else
    {
        super = AS_CLASS(instance)->super;
    }
    if(super == NULL)
    {
        return NULL_VALUE;
    }
    return OBJECT_VALUE(super);
}

static LitValue objfn_class_subscript(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitClass* klass;
    (void)argc;
    klass = AS_CLASS(instance);
    if(argc == 2)
    {
        if(!IS_STRING(argv[0]))
        {
            lit_runtime_error_exiting(vm, "class index must be a string");
        }

        lit_table_set(vm->state, &klass->static_fields, AS_STRING(argv[0]), argv[1]);
        return argv[1];
    }

    if(!IS_STRING(argv[0]))
    {
        lit_runtime_error_exiting(vm, "class index must be a string");
    }

    LitValue value;

    if(lit_table_get(&klass->static_fields, AS_STRING(argv[0]), &value))
    {
        return value;
    }

    if(lit_table_get(&klass->methods, AS_STRING(argv[0]), &value))
    {
        return value;
    }

    return NULL_VALUE;
}

static LitValue objfn_class_name(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(AS_CLASS(instance)->name);
}

/*
 * Object
 */
static LitValue objfn_object_class(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_get_class_for(vm->state, instance));
}


static LitValue objfn_object_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_format(vm->state, "@ instance", OBJECT_VALUE(lit_get_class_for(vm->state, instance)->name)));
}

static LitValue objfn_object_subscript(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    if(!IS_INSTANCE(instance))
    {
        lit_runtime_error_exiting(vm, "cannot modify built-in types");
    }
    LitInstance* inst = AS_INSTANCE(instance);
    if(argc == 2)
    {
        if(!IS_STRING(argv[0]))
        {
            lit_runtime_error_exiting(vm, "object index must be a string");
        }

        lit_table_set(vm->state, &inst->fields, AS_STRING(argv[0]), argv[1]);
        return argv[1];
    }
    if(!IS_STRING(argv[0]))
    {
        lit_runtime_error_exiting(vm, "object index must be a string");
    }
    LitValue value;
    if(lit_table_get(&inst->fields, AS_STRING(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&inst->klass->static_fields, AS_STRING(argv[0]), &value))
    {
        return value;
    }
    if(lit_table_get(&inst->klass->methods, AS_STRING(argv[0]), &value))
    {
        return value;
    }
    return NULL_VALUE;
}

static LitValue objfn_object_iterator(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LIT_ENSURE_ARGS(1)
    LitInstance* self = AS_INSTANCE(instance);
    int index = argv[0] == NULL_VALUE ? -1 : lit_value_to_number(argv[0]);
    int value = table_iterator(&self->fields, index);
    return value == -1 ? NULL_VALUE : lit_number_to_value(value);
}


static LitValue objfn_object_iteratorvalue(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    LitInstance* self = AS_INSTANCE(instance);

    return table_iterator_key(&self->fields, index);
}


/*
 * Number
 */

static LitValue objfn_number_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_number_to_string(vm->state, lit_value_to_number(instance)));
}


static LitValue objfn_number_tochar(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    char ch;
    (void)argc;
    (void)argv;
    ch = lit_value_to_number(instance);
    return OBJECT_VALUE(lit_copy_string(vm->state, &ch, 1));
}


/*
 * Bool
 */

static LitValue bool_toString(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_CONST_STRING(vm->state, AS_BOOL(instance) ? "true" : "false");
}


/*
 * String
 */

static LitValue objfn_string_plus(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* string;
    (void)argc;
    string = AS_STRING(instance);
    LitValue value = argv[0];
    LitString* objfn_string_value = NULL;

    if(IS_STRING(value))
    {
        objfn_string_value = AS_STRING(value);
    }
    else
    {
        objfn_string_value = lit_to_string(vm->state, value);
    }

    size_t length = string->length + objfn_string_value->length;
    LitString* result = lit_allocate_empty_string(vm->state, length);

    result->chars = LIT_ALLOCATE(vm->state, char, length + 1);
    result->chars[length] = '\0';

    memcpy(result->chars, string->chars, string->length);
    memcpy(result->chars + string->length, objfn_string_value->chars, objfn_string_value->length);

    result->hash = lit_hash_string(result->chars, result->length);
    lit_register_string(vm->state, result);

    return OBJECT_VALUE(result);
}


static LitValue objfn_string_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return instance;
}


static LitValue objfn_string_tonumber(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    double result;
    (void)vm;
    (void)argc;
    (void)argv;
    result = strtod(AS_STRING(instance)->chars, NULL);
    if(errno == ERANGE)
    {
        errno = 0;
        return NULL_VALUE;
    }

    return lit_number_to_value(result);
}


static LitValue objfn_string_touppercase(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    char* buffer;
    LitString* rt;
    LitString* string;
    string = AS_STRING(instance);
    (void)argc;
    (void)argv;
    buffer = LIT_ALLOCATE(vm->state, char, string->length + 1);

    for(i = 0; i < string->length; i++)
    {
        buffer[i] = (char)toupper(string->chars[i]);
    }
    buffer[i] = 0;
    rt = lit_copy_string(vm->state, buffer, string->length);
    LIT_FREE(vm->state, char, buffer);
    return OBJECT_VALUE(rt);
}


static LitValue objfn_string_tolowercase(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitString* rt;
    LitString* string = AS_STRING(instance);
    char* buffer;
    (void)argc;
    (void)argv;
    buffer = LIT_ALLOCATE(vm->state, char, string->length + 1);
    for(i = 0; i < string->length; i++)
    {
        buffer[i] = (char)tolower(string->chars[i]);
    }
    buffer[i] = 0;
    rt = lit_copy_string(vm->state, buffer, string->length);
    LIT_FREE(vm->state, char, buffer);
    return OBJECT_VALUE(rt);
}


static LitValue objfn_string_contains(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LitString* string = AS_STRING(instance);
    LitString* sub = LIT_CHECK_OBJECT_STRING(0);

    if(sub == string)
    {
        return TRUE_VALUE;
    }

    return BOOL_VALUE(strstr(string->chars, sub->chars) != NULL);
}


static LitValue objfn_string_startswith(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* string = AS_STRING(instance);
    LitString* sub = LIT_CHECK_OBJECT_STRING(0);

    if(sub == string)
    {
        return TRUE_VALUE;
    }

    if(sub->length > string->length)
    {
        return FALSE_VALUE;
    }

    for(size_t i = 0; i < sub->length; i++)
    {
        if(sub->chars[i] != string->chars[i])
        {
            return FALSE_VALUE;
        }
    }

    return TRUE_VALUE;
}


static LitValue objfn_string_endswith(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* string = AS_STRING(instance);
    LitString* sub = LIT_CHECK_OBJECT_STRING(0);

    if(sub == string)
    {
        return TRUE_VALUE;
    }

    if(sub->length > string->length)
    {
        return FALSE_VALUE;
    }

    size_t start = string->length - sub->length;

    for(size_t i = 0; i < sub->length; i++)
    {
        if(sub->chars[i] != string->chars[i + start])
        {
            return FALSE_VALUE;
        }
    }

    return TRUE_VALUE;
}


static LitValue objfn_string_replace(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(2)

        if(!IS_STRING(argv[0]) || !IS_STRING(argv[1]))
    {
        lit_runtime_error_exiting(vm, "expected 2 string arguments");
    }

    LitString* string = AS_STRING(instance);
    LitString* what = AS_STRING(argv[0]);
    LitString* with = AS_STRING(argv[1]);

    size_t buffer_length = 0;

    for(size_t i = 0; i < string->length; i++)
    {
        if(strncmp(string->chars + i, what->chars, what->length) == 0)
        {
            i += what->length - 1;
            buffer_length += with->length;
        }
        else
        {
            buffer_length++;
        }
    }

    size_t buffer_index = 0;
    char buffer[buffer_length + 1];

    for(size_t i = 0; i < string->length; i++)
    {
        if(strncmp(string->chars + i, what->chars, what->length) == 0)
        {
            memcpy(buffer + buffer_index, with->chars, with->length);

            buffer_index += with->length;
            i += what->length - 1;
        }
        else
        {
            buffer[buffer_index] = string->chars[i];
            buffer_index++;
        }
    }

    buffer[buffer_length] = '\0';

    return OBJECT_VALUE(lit_copy_string(vm->state, buffer, buffer_length));
}


static LitValue objfn_string_substring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int from = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    int to = LIT_CHECK_NUMBER(vm, argv, argc, 1);

    return objfn_string_splice(vm, AS_STRING(instance), from, to);
}


static LitValue objfn_string_subscript(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    if(IS_RANGE(argv[0]))
    {
        LitRange* range = AS_RANGE(argv[0]);
        return objfn_string_splice(vm, AS_STRING(instance), range->from, range->to);
    }

    LitString* string = AS_STRING(instance);
    int index = LIT_CHECK_NUMBER(vm, argv, argc, 0);

    if(argc != 1)
    {
        lit_runtime_error_exiting(vm, "cannot modify strings with the subscript op");
    }

    if(index < 0)
    {
        index = lit_ustring_length(string) + index;

        if(index < 0)
        {
            return NULL_VALUE;
        }
    }

    LitString* c = lit_ustring_code_point_at(vm->state, string, lit_uchar_offset(string->chars, index));

    return c == NULL ? NULL_VALUE : OBJECT_VALUE(c);
}


static LitValue objfn_string_less(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    return BOOL_VALUE(strcmp(AS_STRING(instance)->chars, LIT_CHECK_STRING(0)) < 0);
}


static LitValue objfn_string_greater(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    return BOOL_VALUE(strcmp(AS_STRING(instance)->chars, LIT_CHECK_STRING(0)) > 0);
}


static LitValue objfn_string_length(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_ustring_length(AS_STRING(instance)));
}


static LitValue objfn_string_iterator(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* string;
    string = AS_STRING(instance);

    if(IS_NULL(argv[0]))
    {
        if(string->length == 0)
        {
            return NULL_VALUE;
        }

        return lit_number_to_value(0);
    }

    int index = LIT_CHECK_NUMBER(vm, argv, argc, 0);

    if(index < 0)
    {
        return NULL_VALUE;
    }

    do
    {
        index++;

        if(index >= (int)string->length)
        {
            return NULL_VALUE;
        }
    } while((string->chars[index] & 0xc0) == 0x80);

    return lit_number_to_value(index);
}


static LitValue objfn_string_iteratorvalue(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* string = AS_STRING(instance);
    uint32_t index = LIT_CHECK_NUMBER(vm, argv, argc, 0);

    if(index == UINT32_MAX)
    {
        return false;
    }

    return OBJECT_VALUE(lit_ustring_code_point_at(vm->state, string, index));
}


/*
 * Function
 */

static LitValue objfn_function_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_get_function_name(vm, instance);
}


static LitValue objfn_function_name(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_get_function_name(vm, instance);
}


/*
 * Fiber
 */

static LitValue objfn_fiber_constructor(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    if(argc < 1 || !IS_FUNCTION(argv[0]))
    {
        lit_runtime_error_exiting(vm, "Fiber constructor expects a function as its argument");
    }

    LitFunction* function = AS_FUNCTION(argv[0]);
    LitModule* module = vm->fiber->module;
    LitFiber* fiber = lit_create_fiber(vm->state, module, function);

    fiber->parent = vm->fiber;

    return OBJECT_VALUE(fiber);
}


static LitValue objfn_fiber_done(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return BOOL_VALUE(is_fiber_done(AS_FIBER(instance)));
}


static LitValue objfn_fiber_error(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return AS_FIBER(instance)->error;
}


static LitValue objfn_fiber_current(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(vm->fiber);
}


static bool objfn_fiber_run(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    run_fiber(vm, AS_FIBER(instance), argv, argc, false);
    return true;
}


static bool objfn_fiber_try(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    run_fiber(vm, AS_FIBER(instance), argv, argc, true);
    return true;
}


static bool objfn_fiber_yield(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    if(vm->fiber->parent == NULL)
    {
        lit_handle_runtime_error(vm, argc == 0 ? CONST_STRING(vm->state, "Fiber was yielded") :
        lit_to_string(vm->state, argv[0]));
        return true;
    }

    LitFiber* fiber = vm->fiber;

    vm->fiber = vm->fiber->parent;
    vm->fiber->stack_top -= fiber->arg_count;
    vm->fiber->stack_top[-1] = argc == 0 ? NULL_VALUE : OBJECT_VALUE(lit_to_string(vm->state, argv[0]));

    argv[-1] = NULL_VALUE;
    return true;
}


static bool objfn_fiber_yeet(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    if(vm->fiber->parent == NULL)
    {
        lit_handle_runtime_error(vm, argc == 0 ? CONST_STRING(vm->state, "Fiber was yeeted") :
        lit_to_string(vm->state, argv[0]));
        return true;
    }

    LitFiber* fiber = vm->fiber;

    vm->fiber = vm->fiber->parent;
    vm->fiber->stack_top -= fiber->arg_count;
    vm->fiber->stack_top[-1] = argc == 0 ? NULL_VALUE : OBJECT_VALUE(lit_to_string(vm->state, argv[0]));

    argv[-1] = NULL_VALUE;
    return true;
}


static bool objfn_fiber_abort(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    lit_handle_runtime_error(vm, argc == 0 ? CONST_STRING(vm->state, "Fiber was aborted") :
    lit_to_string(vm->state, argv[0]));
    argv[-1] = NULL_VALUE;
    return true;
}


/*
 * Module
 */

LitValue access_private(LitVm* vm, LitMap* map, LitString* name, LitValue* val)
{
    LitValue value;
    LitString* id = CONST_STRING(vm->state, "_module");

    if(!lit_table_get(&map->values, id, &value) || !IS_MODULE(value))
    {
        return NULL_VALUE;
    }

    LitModule* module = AS_MODULE(value);

    if(id == name)
    {
        return OBJECT_VALUE(module);
    }

    if(lit_table_get(&module->private_names->values, name, &value))
    {
        int index = (int)lit_value_to_number(value);

        if(index > -1 && index < (int)module->private_count)
        {
            if(val != NULL)
            {
                module->privates[index] = *val;
                return *val;
            }

            return module->privates[index];
        }
    }

    return NULL_VALUE;
}


static LitValue objfn_module_privates(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitModule* module;
    (void)argc;
    (void)argv;
    module = IS_MODULE(instance) ? AS_MODULE(instance) : vm->fiber->module;
    LitMap* map = module->private_names;

    if(map->index_fn == NULL)
    {
        map->index_fn = access_private;
        lit_table_set(vm->state, &map->values, CONST_STRING(vm->state, "_module"), OBJECT_VALUE(module));
    }

    return OBJECT_VALUE(map);
}

static LitValue objfn_module_current(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(vm->fiber->module);
}

static LitValue objfn_module_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_string_format(vm->state, "Module @", OBJECT_VALUE(AS_MODULE(instance)->name)));
}


static LitValue objfn_module_name(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(AS_MODULE(instance)->name);
}

/*
 * Array
 */

static LitValue objfn_array_constructor(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_create_array(vm->state));
}


static LitValue objfn_array_slice(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int from;
    int to;
    from = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    to = LIT_CHECK_NUMBER(vm, argv, argc, 1);

    return objfn_array_splice(vm, AS_ARRAY(instance), from, to);
}


static LitValue objfn_array_subscript(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    if(argc == 2)
    {
        if(!IS_NUMBER(argv[0]))
        {
            lit_runtime_error_exiting(vm, "array index must be a number");
        }

        LitValues* values = &AS_ARRAY(instance)->values;
        int index = lit_value_to_number(argv[0]);

        if(index < 0)
        {
            index = fmax(0, values->count + index);
        }

        lit_values_ensure_size(vm->state, values, index + 1);
        return values->values[index] = argv[1];
    }

    if(!IS_NUMBER(argv[0]))
    {
        if(IS_RANGE(argv[0]))
        {
            LitRange* range = AS_RANGE(argv[0]);
            return objfn_array_splice(vm, AS_ARRAY(instance), (int)range->from, (int)range->to);
        }

        lit_runtime_error_exiting(vm, "array index must be a number");
        return NULL_VALUE;
    }

    LitValues* values = &AS_ARRAY(instance)->values;
    int index = lit_value_to_number(argv[0]);

    if(index < 0)
    {
        index = fmax(0, values->count + index);
    }

    if(values->capacity <= (size_t)index)
    {
        return NULL_VALUE;
    }

    return values->values[index];
}


static LitValue objfn_array_add(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)
        lit_values_write(vm->state, &AS_ARRAY(instance)->values, argv[0]);

    return NULL_VALUE;
}


static LitValue objfn_array_insert(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(2)

        LitValues* values = &AS_ARRAY(instance)->values;
    int index = LIT_CHECK_NUMBER(vm, argv, argc, 0);

    if(index < 0)
    {
        index = fmax(0, values->count + index);
    }

    LitValue value = argv[1];

    if((int)values->count <= index)
    {
        lit_values_ensure_size(vm->state, values, index + 1);
    }
    else
    {
        lit_values_ensure_size(vm->state, values, values->count + 1);

        for(int i = values->count - 1; i > index; i--)
        {
            values->values[i] = values->values[i - 1];
        }
    }

    values->values[index] = value;
    return NULL_VALUE;
}


static LitValue objfn_array_addall(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

        if(!IS_ARRAY(argv[0]))
    {
        lit_runtime_error_exiting(vm, "expected array as the argument");
    }

    LitArray* array = AS_ARRAY(instance);
    LitArray* toAdd = AS_ARRAY(argv[0]);

    for(size_t i = 0; i < toAdd->values.count; i++)
    {
        lit_values_write(vm->state, &array->values, toAdd->values.values[i]);
    }

    return NULL_VALUE;
}


static LitValue objfn_array_indexof(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

        int index = indexOf(AS_ARRAY(instance), argv[0]);
    return index == -1 ? NULL_VALUE : lit_number_to_value(index);
}


static LitValue objfn_array_remove(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

        LitArray* array = AS_ARRAY(instance);
    int index = indexOf(array, argv[0]);

    if(index != -1)
    {
        return removeAt(array, (size_t)index);
    }

    return NULL_VALUE;
}


static LitValue objfn_array_removeat(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index = LIT_CHECK_NUMBER(vm, argv, argc, 0);

    if(index < 0)
    {
        return NULL_VALUE;
    }

    return removeAt(AS_ARRAY(instance), (size_t)index);
}


static LitValue objfn_array_contains(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)
        return BOOL_VALUE(indexOf(AS_ARRAY(instance), argv[0]) != -1);
}


static LitValue objfn_array_clear(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    AS_ARRAY(instance)->values.count = 0;
    return NULL_VALUE;
}


static LitValue objfn_array_iterator(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int number;
    LitArray* array;
    (void)vm;
    LIT_ENSURE_ARGS(1)
    array = AS_ARRAY(instance);
    number = 0;
    if(IS_NUMBER(argv[0]))
    {
        number = lit_value_to_number(argv[0]);

        if(number >= (int)array->values.count - 1)
        {
            return NULL_VALUE;
        }

        number++;
    }
    return array->values.count == 0 ? NULL_VALUE : lit_number_to_value(number);
}


static LitValue objfn_array_iteratorvalue(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    LitValues* values = &AS_ARRAY(instance)->values;

    if(values->count <= index)
    {
        return NULL_VALUE;
    }

    return values->values[index];
}


static LitValue objfn_array_join(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    size_t index;
    size_t length;
    LitValues* values;
    (void)argc;
    (void)argv;
    length = 0;
    values = &AS_ARRAY(instance)->values;
    LitString* strings[values->count];

    for(i = 0; i < values->count; i++)
    {
        LitString* string = lit_to_string(vm->state, values->values[i]);

        strings[i] = string;
        length += string->length;
    }
    index = 0;
    char chars[length + 1];
    chars[length] = '\0';
    for(i = 0; i < values->count; i++)
    {
        LitString* string = strings[i];
        memcpy(chars + index, string->chars, string->length);
        index += string->length;
    }

    return OBJECT_VALUE(lit_copy_string(vm->state, chars, length));
}


static LitValue objfn_array_sort(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitValues* values;
    values = &AS_ARRAY(instance)->values;

    if(argc == 1 && IS_CALLABLE_FUNCTION(argv[0]))
    {
        custom_quick_sort(vm, values->values, values->count, argv[0]);
    }
    else
    {
        basic_quick_sort(vm->state, values->values, values->count);
    }

    return instance;
}


static LitValue objfn_array_clone(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitState* state = vm->state;
    LitValues* values = &AS_ARRAY(instance)->values;
    LitArray* array = lit_create_array(state);
    LitValues* new_values = &array->values;

    lit_values_ensure_size(state, new_values, values->count);

    // lit_values_ensure_size sets the count to max of previous count (0 in this case) and new count, so we have to reset it
    new_values->count = 0;

    for(size_t i = 0; i < values->count; i++)
    {
        lit_values_write(state, new_values, values->values[i]);
    }

    return OBJECT_VALUE(array);
}


static LitValue objfn_array_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    bool has_more;
    size_t i;
    size_t buffer_index;
    size_t value_amount;
    size_t objfn_string_length;
    LitArray* self;
    LitValues* values;
    LitValue val;
    LitState* state;
    LitString* part;
    LitString* stringified;
    static const char* recstring = "(recursion)";
    self = AS_ARRAY(instance);
    values = &self->values;
    state = vm->state;
    if(values->count == 0)
    {
        return OBJECT_CONST_STRING(state, "[]");
    }
    has_more = values->count > LIT_CONTAINER_OUTPUT_MAX;
    value_amount = has_more ? LIT_CONTAINER_OUTPUT_MAX : values->count;
    LitString* values_converted[value_amount];

    // "[ ]"
    objfn_string_length = 3;
    if(has_more)
    {
        objfn_string_length += 3;
    }
    for(i = 0; i < value_amount; i++)
    {
        val = values->values[(has_more && i == value_amount - 1) ? values->count - 1 : i];
        if(IS_ARRAY(val) && (AS_ARRAY(val) == self))
        {
            stringified = lit_copy_string(state, recstring, strlen(recstring));
        }
        else
        {
            stringified = lit_to_string(state, val);
        }
        values_converted[i] = stringified;
        objfn_string_length += stringified->length + (i == value_amount - 1 ? 1 : 2);
    }
    char buffer[objfn_string_length + 1];
    memcpy(buffer, "[ ", 2);
    buffer_index = 2;
    for(i = 0; i < value_amount; i++)
    {
        part = values_converted[i];
        memcpy(&buffer[buffer_index], part->chars, part->length);
        buffer_index += part->length;
        if(has_more && i == value_amount - 2)
        {
            memcpy(&buffer[buffer_index], " ... ", 5);
            buffer_index += 5;
        }
        else
        {
            memcpy(&buffer[buffer_index], (i == value_amount - 1) ? " ]" : ", ", 2);
            buffer_index += 2;
        }
    }

    buffer[objfn_string_length] = '\0';
    return OBJECT_VALUE(lit_copy_string(vm->state, buffer, objfn_string_length));
}


static LitValue objfn_array_length(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(AS_ARRAY(instance)->values.count);
}


/*
 * Map
 */

static LitValue objfn_map_constructor(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return OBJECT_VALUE(lit_create_map(vm->state));
}


static LitValue objfn_map_subscript(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    if(!IS_STRING(argv[0]))
    {
        lit_runtime_error_exiting(vm, "map index must be a string");
    }

    LitMap* map = AS_MAP(instance);
    LitString* index = AS_STRING(argv[0]);

    if(argc == 2)
    {
        LitValue val = argv[1];

        if(map->index_fn != NULL)
        {
            return map->index_fn(vm, map, index, &val);
        }

        lit_map_set(vm->state, map, index, val);
        return val;
    }

    LitValue value;

    if(map->index_fn != NULL)
    {
        return map->index_fn(vm, map, index, NULL);
    }

    if(!lit_table_get(&map->values, index, &value))
    {
        return NULL_VALUE;
    }

    return value;
}


static LitValue objfn_map_addall(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

        if(!IS_MAP(argv[0]))
    {
        lit_runtime_error_exiting(vm, "expected map as the argument");
    }

    lit_map_add_all(vm->state, AS_MAP(argv[0]), AS_MAP(instance));
    return NULL_VALUE;
}


static LitValue objfn_map_clear(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argv;
    (void)argc;
    AS_MAP(instance)->values.count = 0;
    return NULL_VALUE;
}


static LitValue objfn_map_iterator(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    (void)vm;
    int index = argv[0] == NULL_VALUE ? -1 : lit_value_to_number(argv[0]);

    int value = table_iterator(&AS_MAP(instance)->values, index);
    return value == -1 ? NULL_VALUE : lit_number_to_value(value);
}


static LitValue objfn_map_iteratorvalue(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index = LIT_CHECK_NUMBER(vm, argv, argc, 0);
    return table_iterator_key(&AS_MAP(instance)->values, index);
}


static LitValue objfn_map_clone(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitState* state = vm->state;
    LitMap* map = lit_create_map(state);

    lit_table_add_all(state, &AS_MAP(instance)->values, &map->values);

    return OBJECT_VALUE(map);
}


static LitValue objfn_map_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    bool has_wrapper;
    bool has_more;
    size_t i;
    size_t index;
    size_t value_amount;
    size_t objfn_string_length;
    size_t buffer_index;
    LitState* state;
    LitMap* map;
    LitTable* values;
    LitTableEntry* entry;
    LitValue field;
    LitString* value;
    state = vm->state;
    map = AS_MAP(instance);
    values = &map->values;
    if(values->count == 0)
    {
        return OBJECT_CONST_STRING(state, "{}");
    }
    has_wrapper = map->index_fn != NULL;
    has_more = values->count > LIT_CONTAINER_OUTPUT_MAX;
    value_amount = has_more ? LIT_CONTAINER_OUTPUT_MAX : values->count;

    LitString* values_converted[value_amount];
    LitString* keys[value_amount];
    objfn_string_length = 3;
    if(has_more)
    {
        objfn_string_length += SINGLE_LINE_MAPS_ENABLED ? 5 : 6;
    }
    i = 0;
    index = 0;
    do
    {
        entry = &values->entries[index++];
        if(entry->key != NULL)
        {
            // Special hidden key
            field = has_wrapper ? map->index_fn(vm, map, entry->key, NULL) : entry->value;
            // This check is required to prevent infinite loops when playing with Module.privates and such
            value = (IS_MAP(field) && AS_MAP(field)->index_fn != NULL) ? CONST_STRING(state, "map") :
            lit_to_string(state, field);
            lit_push_root(state, (LitObject*)value);

            values_converted[i] = value;
            keys[i] = entry->key;
            objfn_string_length += entry->key->length + 3 + value->length +
                #ifdef SINGLE_LINE_MAPS
                    (i == value_amount - 1 ? 1 : 2);
                #else
                    (i == value_amount - 1 ? 2 : 3);
                #endif
            i++;
        }
    } while(i < value_amount);
    char buffer[objfn_string_length + 1];
    #ifdef SINGLE_LINE_MAPS
    memcpy(buffer, "{ ", 2);
    #else
    memcpy(buffer, "{\n", 2);
    #endif
    buffer_index = 2;
    for(i = 0; i < value_amount; i++)
    {
        LitString* key = keys[i];
        LitString* value = values_converted[i];

        #ifndef SINGLE_LINE_MAPS
        buffer[buffer_index++] = '\t';
        #endif

        memcpy(&buffer[buffer_index], key->chars, key->length);
        buffer_index += key->length;

        memcpy(&buffer[buffer_index], " = ", 3);
        buffer_index += 3;
        memcpy(&buffer[buffer_index], value->chars, value->length);
        buffer_index += value->length;
        if(has_more && i == value_amount - 1)
        {
            #ifdef SINGLE_LINE_MAPS
            memcpy(&buffer[buffer_index], ", ... }", 7);
            #else
            memcpy(&buffer[buffer_index], ",\n\t...\n}", 8);
            #endif
            buffer_index += 8;
        }
        else
        {
            #ifdef SINGLE_LINE_MAPS
            memcpy(&buffer[buffer_index], (i == value_amount - 1) ? " }" : ", ", 2);
            #else
            memcpy(&buffer[buffer_index], (i == value_amount - 1) ? "\n}" : ",\n", 2);
            #endif
            buffer_index += 2;
        }
        lit_pop_root(state);
    }
    buffer[objfn_string_length] = '\0';
    return OBJECT_VALUE(lit_copy_string(vm->state, buffer, objfn_string_length));
}


static LitValue objfn_map_length(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(AS_MAP(instance)->values.count);
}

/*
 * Range
 */

static LitValue objfn_range_iterator(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    LitRange* range = AS_RANGE(instance);
    int number = range->from;
    (void)vm;
    (void)argc;
    if(IS_NUMBER(argv[0]))
    {
        number = lit_value_to_number(argv[0]);

        if(range->to > range->from ? number >= range->to : number >= range->from)
        {
            return NULL_VALUE;
        }

        number += (range->from - range->to) > 0 ? -1 : 1;
    }
    return lit_number_to_value(number);
}


static LitValue objfn_range_iteratorvalue(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1);
    (void)vm;
    (void)instance;
    return argv[0];
}


static LitValue objfn_range_tostring(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitRange* range = AS_RANGE(instance);
    return OBJECT_VALUE(lit_string_format(vm->state, "Range(#, #)", range->from, range->to));
}


static LitValue objfn_range_from(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argv;
    (void)argc;
    return lit_number_to_value(AS_RANGE(instance)->from);
}


static LitValue objfn_range_set_from(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    AS_RANGE(instance)->from = lit_value_to_number(argv[0]);
    return argv[0];
}


static LitValue objfn_range_to(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(AS_RANGE(instance)->to);
}


static LitValue objfn_range_set_to(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    AS_RANGE(instance)->to = lit_value_to_number(argv[0]);
    return argv[0];
}


static LitValue objfn_range_length(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LitRange* range = AS_RANGE(instance);
    return lit_number_to_value(range->to - range->from);
}

/*
 * Natives
 */

static LitValue cfn_time(LitVm* vm, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value((double)clock() / CLOCKS_PER_SEC);
}


static LitValue cfn_systemTime(LitVm* vm, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_number_to_value(time(NULL));
}


static LitValue cfn_print(LitVm* vm, size_t argc, LitValue* argv)
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

static bool eval_primitive(LitVm* vm, size_t argc, LitValue* argv)
{
    char* code;
    (void)argc;
    (void)argv;
    code = (char*)LIT_CHECK_STRING(0);
    return compile_and_interpret(vm, vm->fiber->module->name, code);
}

static bool require_primitive(LitVm* vm, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitString* name = LIT_CHECK_OBJECT_STRING(0);
    bool ignore_previous = argc > 1 && IS_BOOL(argv[1]) && AS_BOOL(argv[1]);
    // First check, if a file with this name exists in the local path
    if(attempt_to_require(vm, argv, argc, name->chars, ignore_previous, false))
    {
        return should_update_locals;
    }
    // If not, we join the path of the current module to it (the path goes all the way from the root)
    LitString* objfn_module_name = vm->fiber->module->name;
    // We need to get rid of the module name (test.folder.module -> test.folder)
    char* index = strrchr(objfn_module_name->chars, '.');
    if(index != NULL)
    {
        size_t length = index - objfn_module_name->chars;
        char buffer[length + 1];
        memcpy((void*)buffer, objfn_module_name->chars, length);
        buffer[length] = '\0';
        if(attempt_to_require_combined(vm, argv, argc, (const char*)&buffer, name->chars, ignore_previous))
        {
            return should_update_locals;
        }
    }
    lit_runtime_error_exiting(vm, "failed to require module '%s'", name->chars);
    return false;
}

// NB. clang-format will mess this up.
// clang-format off
void lit_open_core_library(LitState* state)
{
    {
        LIT_BEGIN_CLASS("Class");
        {
            LIT_BIND_METHOD("toString", objfn_class_tostring);
            LIT_BIND_METHOD("[]", objfn_class_subscript);
            LIT_BIND_STATIC_METHOD("toString", objfn_class_tostring);
            LIT_BIND_STATIC_METHOD("iterator", objfn_class_iterator);
            LIT_BIND_STATIC_METHOD("iteratorValue", objfn_class_iteratorvalue);
            LIT_BIND_GETTER("super", objfn_class_super);
            LIT_BIND_STATIC_GETTER("super", objfn_class_super);
            LIT_BIND_STATIC_GETTER("name", objfn_class_name);
            state->classvalue_class = klass;
        }
        LIT_END_CLASS_IGNORING();
    }
    {
        LIT_BEGIN_CLASS("Object");
        {
            LIT_INHERIT_CLASS(state->classvalue_class);
            LIT_BIND_METHOD("toString", objfn_object_tostring);
            LIT_BIND_METHOD("[]", objfn_object_subscript);
            LIT_BIND_METHOD("iterator", objfn_object_iterator);
            LIT_BIND_METHOD("iteratorValue", objfn_object_iteratorvalue);
            LIT_BIND_GETTER("class", objfn_object_class);
            state->objectvalue_class = klass;
            state->objectvalue_class->super = state->classvalue_class;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Number");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(invalid_constructor);
            LIT_BIND_METHOD("toString", objfn_number_tostring);
            LIT_BIND_METHOD("toChar", objfn_number_tochar);
            state->numbervalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("String");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(invalid_constructor);
            LIT_BIND_METHOD("+", objfn_string_plus);
            LIT_BIND_METHOD("toString", objfn_string_tostring);
            LIT_BIND_METHOD("toNumber", objfn_string_tonumber);
            LIT_BIND_METHOD("toUpperCase", objfn_string_touppercase);
            LIT_BIND_METHOD("toLowerCase", objfn_string_tolowercase);
            LIT_BIND_METHOD("contains", objfn_string_contains);
            LIT_BIND_METHOD("startsWith", objfn_string_startswith);
            LIT_BIND_METHOD("endsWith", objfn_string_endswith);
            LIT_BIND_METHOD("replace", objfn_string_replace);
            LIT_BIND_METHOD("substring", objfn_string_substring);
            LIT_BIND_METHOD("iterator", objfn_string_iterator);
            LIT_BIND_METHOD("iteratorValue", objfn_string_iteratorvalue);
            LIT_BIND_METHOD("[]", objfn_string_subscript);
            LIT_BIND_METHOD("<", objfn_string_less);
            LIT_BIND_METHOD(">", objfn_string_greater);
            LIT_BIND_GETTER("length", objfn_string_length);
            state->stringvalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Bool");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(invalid_constructor);
            LIT_BIND_METHOD("toString", bool_toString);
            state->boolvalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Function");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(invalid_constructor);
            LIT_BIND_METHOD("toString", objfn_function_tostring);
            LIT_BIND_GETTER("name", objfn_function_name);
            state->functionvalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Fiber");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(objfn_fiber_constructor);
            LIT_BIND_PRIMITIVE("run", objfn_fiber_run);
            LIT_BIND_PRIMITIVE("try", objfn_fiber_try);
            LIT_BIND_GETTER("done", objfn_fiber_done);
            LIT_BIND_GETTER("error", objfn_fiber_error);
            LIT_BIND_STATIC_PRIMITIVE("yield", objfn_fiber_yield);
            LIT_BIND_STATIC_PRIMITIVE("yeet", objfn_fiber_yeet);
            LIT_BIND_STATIC_PRIMITIVE("abort", objfn_fiber_abort);
            LIT_BIND_STATIC_GETTER("current", objfn_fiber_current);
            state->functionvalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Module");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(invalid_constructor);
            LIT_SET_STATIC_FIELD("loaded", OBJECT_VALUE(state->vm->modules));
            LIT_BIND_STATIC_GETTER("privates", objfn_module_privates);
            LIT_BIND_STATIC_GETTER("current", objfn_module_current);
            LIT_BIND_METHOD("toString", objfn_module_tostring);
            LIT_BIND_GETTER("name", objfn_module_name);
            LIT_BIND_GETTER("privates", objfn_module_privates);
            state->modulevalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Array");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(objfn_array_constructor);
            LIT_BIND_METHOD("[]", objfn_array_subscript);
            LIT_BIND_METHOD("add", objfn_array_add);
            LIT_BIND_METHOD("push", objfn_array_add);
            LIT_BIND_METHOD("insert", objfn_array_insert);
            LIT_BIND_METHOD("slice", objfn_array_slice);
            LIT_BIND_METHOD("addAll", objfn_array_addall);
            LIT_BIND_METHOD("remove", objfn_array_remove);
            LIT_BIND_METHOD("removeAt", objfn_array_removeat);
            LIT_BIND_METHOD("indexOf", objfn_array_indexof);
            LIT_BIND_METHOD("contains", objfn_array_contains);
            LIT_BIND_METHOD("clear", objfn_array_clear);
            LIT_BIND_METHOD("iterator", objfn_array_iterator);
            LIT_BIND_METHOD("iteratorValue", objfn_array_iteratorvalue);
            LIT_BIND_METHOD("join", objfn_array_join);
            LIT_BIND_METHOD("sort", objfn_array_sort);
            LIT_BIND_METHOD("clone", objfn_array_clone);
            LIT_BIND_METHOD("toString", objfn_array_tostring);
            LIT_BIND_GETTER("length", objfn_array_length);
            state->arrayvalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Map");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(objfn_map_constructor);
            LIT_BIND_METHOD("[]", objfn_map_subscript);
            LIT_BIND_METHOD("addAll", objfn_map_addall);
            LIT_BIND_METHOD("clear", objfn_map_clear);
            LIT_BIND_METHOD("iterator", objfn_map_iterator);
            LIT_BIND_METHOD("iteratorValue", objfn_map_iteratorvalue);
            LIT_BIND_METHOD("clone", objfn_map_clone);
            LIT_BIND_METHOD("toString", objfn_map_tostring);
            LIT_BIND_GETTER("length", objfn_map_length);
            state->mapvalue_class = klass;
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Range");
        {
            LIT_INHERIT_CLASS(state->objectvalue_class);
            LIT_BIND_CONSTRUCTOR(invalid_constructor);
            LIT_BIND_METHOD("iterator", objfn_range_iterator);
            LIT_BIND_METHOD("iteratorValue", objfn_range_iteratorvalue);
            LIT_BIND_METHOD("toString", objfn_range_tostring);
            LIT_BIND_FIELD("from", objfn_range_from, objfn_range_set_from);
            LIT_BIND_FIELD("to", objfn_range_to, objfn_range_set_to);
            LIT_BIND_GETTER("length", objfn_range_length);
            state->rangevalue_class  = klass;
        }
        LIT_END_CLASS();
    }
    {
        lit_define_native(state, "time", cfn_time);
        lit_define_native(state, "systemTime", cfn_systemTime);
        lit_define_native(state, "print", cfn_print);
        lit_define_native_primitive(state, "require", require_primitive);
        lit_define_native_primitive(state, "eval", eval_primitive);
        lit_set_global(state, CONST_STRING(state, "globals"), OBJECT_VALUE(state->vm->globals));
    }
}
