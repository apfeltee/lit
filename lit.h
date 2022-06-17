
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <wchar.h>


#define LIT_REPOSITORY "https://github.com/egordorichev/lit"

#define LIT_VERSION_MAJOR 0
#define LIT_VERSION_MINOR 1
#define LIT_VERSION_STRING "0.1"
#define LIT_BYTECODE_VERSION 0

#define TESTING
// #define DEBUG

#ifdef DEBUG
    #define LIT_TRACE_EXECUTION
    #define LIT_TRACE_STACK
    #define LIT_CHECK_STACK_SIZE
// #define LIT_TRACE_CHUNK
// #define LIT_MINIMIZE_CONTAINERS
// #define LIT_LOG_GC
// #define LIT_LOG_ALLOCATION
// #define LIT_LOG_MARKING
// #define LIT_LOG_BLACKING
// #define LIT_STRESS_TEST_GC
#endif

#ifdef TESTING
    // So that we can actually test the map contents with a single-line expression
    #define SINGLE_LINE_MAPS
    #define SINGLE_LINE_MAPS_ENABLED true

// Make sure that we did not break anything
// #define LIT_STRESS_TEST_GC
#else
    #define SINGLE_LINE_MAPS_ENABLED false
#endif

#define LIT_MAX_INTERPOLATION_NESTING 4

#define LIT_GC_HEAP_GROW_FACTOR 2
#define LIT_CALL_FRAMES_MAX (1024*8)
#define LIT_INITIAL_CALL_FRAMES 128
#define LIT_CONTAINER_OUTPUT_MAX 10


#if defined(__ANDROID__) || defined(_ANDROID_)
    #define LIT_OS_ANDROID
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    #define LIT_OS_WINDOWS
#elif __APPLE__
    #define LIT_OS_MAC
    #define LIT_OS_UNIX_LIKE
#elif __linux__
    #define LIT_OS_LINUX
    #define LIT_OS_UNIX_LIKE
#else
    #define LIT_OS_UNKNOWN
#endif

#ifdef LIT_OS_UNIX_LIKE
    #define LIT_USE_LIBREADLINE
#endif

#ifdef LIT_USE_LIBREADLINE
#else
    #define LIT_REPL_INPUT_MAX 1024
#endif

#define LIT_EXIT_CODE_ARGUMENT_ERROR 1
#define LIT_EXIT_CODE_MEM_LEAK 2
#define LIT_EXIT_CODE_RUNTIME_ERROR 70
#define LIT_EXIT_CODE_COMPILE_ERROR 65

#define LIT_TESTS_DIRECTORY "tests"


#if !defined(LIT_DISABLE_COLOR) && !defined(LIT_ENABLE_COLOR) && !(defined(LIT_OS_WINDOWS) || defined(EMSCRIPTEN))
    #define LIT_ENABLE_COLOR
#endif

#ifdef LIT_ENABLE_COLOR
    #define COLOR_RESET "\x1B[0m"
    #define COLOR_RED "\x1B[31m"
    #define COLOR_GREEN "\x1B[32m"
    #define COLOR_YELLOW "\x1B[33m"
    #define COLOR_BLUE "\x1B[34m"
    #define COLOR_MAGENTA "\x1B[35m"
    #define COLOR_CYAN "\x1B[36m"
    #define COLOR_WHITE "\x1B[37m"
#else
    #define COLOR_RESET ""
    #define COLOR_RED ""
    #define COLOR_GREEN ""
    #define COLOR_YELLOW ""
    #define COLOR_BLUE ""
    #define COLOR_MAGENTA ""
    #define COLOR_CYAN ""
    #define COLOR_WHITE ""
#endif


#define UNREACHABLE assert(false);
#define UINT8_COUNT UINT8_MAX + 1
#define UINT16_COUNT UINT16_MAX + 1




#define SIGN_BIT ((uint64_t)1 << 63u)
#define QNAN ((uint64_t)0x7ffc000000000000u)

#define TAG_NULL 1u
#define TAG_FALSE 2u
#define TAG_TRUE 3u

#define IS_BOOL(v) (((v)&FALSE_VALUE) == FALSE_VALUE)
#define IS_NULL(v) ((v) == NULL_VALUE)
#define IS_NUMBER(v) (((v)&QNAN) != QNAN)
#define IS_OBJECT(v) (((v) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(v) ((v) == TRUE_VALUE)
#define AS_NUMBER(v) lit_value_to_number(v)
#define AS_OBJECT(v) ((LitObject*)(uintptr_t)((v) & ~(SIGN_BIT | QNAN)))

#define BOOL_VALUE(boolean) \
    ((boolean) ? TRUE_VALUE : FALSE_VALUE)

#define FALSE_VALUE ((LitValue)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VALUE ((LitValue)(uint64_t)(QNAN | TAG_TRUE))
#define NULL_VALUE ((LitValue)(uint64_t)(QNAN | TAG_NULL))
#define NUMBER_VALUE(num) lit_number_to_value(num)

#define OBJECT_VALUE(obj) \
    (LitValue)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

#define LIT_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity)*2)

#define LIT_GROW_ARRAY(state, previous, type, old_count, count) \
    (type*)lit_reallocate(state, previous, sizeof(type) * (old_count), sizeof(type) * (count))

#define LIT_FREE_ARRAY(state, type, pointer, old_count) \
    lit_reallocate(state, pointer, sizeof(type) * (old_count), 0)

#define LIT_ALLOCATE(state, type, count) \
    (type*)lit_reallocate(state, NULL, 0, sizeof(type) * (count))

#define LIT_FREE(state, type, pointer) \
    lit_reallocate(state, pointer, sizeof(type), 0)

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_OBJECTS_TYPE(value, t) \
    (IS_OBJECT(value) && AS_OBJECT(value)->type == t)

#define IS_STRING(value) \
    IS_OBJECTS_TYPE(value, OBJECT_STRING)

#define IS_FUNCTION(value) \
    IS_OBJECTS_TYPE(value, OBJECT_FUNCTION)

#define IS_NATIVE_FUNCTION(value) \
    IS_OBJECTS_TYPE(value, OBJECT_NATIVE_FUNCTION)

#define IS_NATIVE_PRIMITIVE(value) \
    IS_OBJECTS_TYPE(value, OBJECT_NATIVE_PRIMITIVE)

#define IS_NATIVE_METHOD(value) \
    IS_OBJECTS_TYPE(value, OBJECT_NATIVE_METHOD)

#define IS_PRIMITIVE_METHOD(value) \
    IS_OBJECTS_TYPE(value, OBJECT_PRIMITIVE_METHOD)

#define IS_MODULE(value) IS_OBJECTS_TYPE(value, OBJECT_MODULE)

#define IS_CLOSURE(value) IS_OBJECTS_TYPE(value, OBJECT_CLOSURE)

#define IS_UPVALUE(value) IS_OBJECTS_TYPE(value, OBJECT_UPVALUE)

#define IS_CLASS(value) IS_OBJECTS_TYPE(value, OBJECT_CLASS)

#define IS_INSTANCE(value) IS_OBJECTS_TYPE(value, OBJECT_INSTANCE)

#define IS_ARRAY(value) IS_OBJECTS_TYPE(value, OBJECT_ARRAY)

#define IS_MAP(value) IS_OBJECTS_TYPE(value, OBJECT_MAP)

#define IS_BOUND_METHOD(value) IS_OBJECTS_TYPE(value, OBJECT_BOUND_METHOD)

#define IS_USERDATA(value) IS_OBJECTS_TYPE(value, OBJECT_USERDATA)

#define IS_RANGE(value) IS_OBJECTS_TYPE(value, OBJECT_RANGE)

#define IS_FIELD(value) IS_OBJECTS_TYPE(value, OBJECT_FIELD)

#define IS_REFERENCE(value) IS_OBJECTS_TYPE(value, OBJECT_REFERENCE)

#define IS_CALLABLE_FUNCTION(value) lit_is_callable_function(value)

#define AS_STRING(value) ((LitString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((LitString*)AS_OBJECT(value))->chars)
#define AS_FUNCTION(value) ((LitFunction*)AS_OBJECT(value))
#define AS_NATIVE_FUNCTION(value) ((LitNativeFunction*)AS_OBJECT(value))
#define AS_NATIVE_PRIMITIVE(value) ((LitNativePrimitive*)AS_OBJECT(value))
#define AS_NATIVE_METHOD(value) ((LitNativeMethod*)AS_OBJECT(value))
#define AS_PRIMITIVE_METHOD(value) ((LitPrimitiveMethod*)AS_OBJECT(value))
#define AS_MODULE(value) ((LitModule*)AS_OBJECT(value))
#define AS_CLOSURE(value) ((LitClosure*)AS_OBJECT(value))
#define AS_UPVALUE(value) ((LitUpvalue*)AS_OBJECT(value))
#define AS_CLASS(value) ((LitClass*)AS_OBJECT(value))
#define AS_INSTANCE(value) ((LitInstance*)AS_OBJECT(value))
#define AS_ARRAY(value) ((LitArray*)AS_OBJECT(value))
#define AS_MAP(value) ((LitMap*)AS_OBJECT(value))
#define AS_BOUND_METHOD(value) ((LitBoundMethod*)AS_OBJECT(value))
#define AS_USERDATA(value) ((LitUserdata*)AS_OBJECT(value))
#define AS_RANGE(value) ((LitRange*)AS_OBJECT(value))
#define AS_FIELD(value) ((LitField*)AS_OBJECT(value))
#define AS_FIBER(value) ((LitFiber*)AS_OBJECT(value))
#define AS_REFERENCE(value) ((LitReference*)AS_OBJECT(value))

#define ALLOCATE_OBJECT(state, type, objectType) (type*)lit_allocate_object(state, sizeof(type), objectType)
#define OBJECT_CONST_STRING(state, text) OBJECT_VALUE(lit_copy_string((state), (text), strlen(text)))
#define CONST_STRING(state, text) lit_copy_string((state), (text), strlen(text))

#define INTERPRET_RUNTIME_FAIL ((LitInterpretResult){ INTERPRET_INVALID, NULL_VALUE })

#define RETURN_RUNTIME_ERROR() return (LitInterpretResult){ INTERPRET_RUNTIME_ERROR, NULL_VALUE };
#define RETURN_OK(r) return (LitInterpretResult){ INTERPRET_OK, r };

#define LIT_BEGIN_CLASS(name)                                               \
    {                                                                       \
        LitString* klass_name = lit_copy_string(state, name, strlen(name)); \
        LitClass* klass = lit_create_class(state, klass_name);

#define LIT_INHERIT_CLASS(super_klass)                                \
    klass->super = (LitClass*)super_klass;                            \
    if(klass->init_method == NULL)                                    \
    {                                                                 \
        klass->init_method = super_klass->init_method;                \
    }                                                                 \
    lit_table_add_all(state, &super_klass->methods, &klass->methods); \
    lit_table_add_all(state, &super_klass->static_fields, &klass->static_fields);

#define LIT_END_CLASS_IGNORING()                            \
    lit_set_global(state, klass_name, OBJECT_VALUE(klass)); \
    }


#define LIT_END_CLASS()                                     \
    lit_set_global(state, klass_name, OBJECT_VALUE(klass)); \
    if(klass->super == NULL) \
    {                                                       \
        LIT_INHERIT_CLASS(state->object_class);             \
    }                                                       \
    }

#define LIT_BIND_METHOD(name, method)                                                                         \
    {                                                                                                         \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                           \
        lit_table_set(state, &klass->methods, nm, OBJECT_VALUE(lit_create_native_method(state, method, nm))); \
    }

#define LIT_BIND_PRIMITIVE(name, method)                                                                         \
    {                                                                                                            \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                              \
        lit_table_set(state, &klass->methods, nm, OBJECT_VALUE(lit_create_primitive_method(state, method, nm))); \
    }

#define LIT_BIND_CONSTRUCTOR(method)                                      \
    {                                                                     \
        LitString* nm = lit_copy_string(state, "constructor", 11);        \
        LitNativeMethod* m = lit_create_native_method(state, method, nm); \
        klass->init_method = (LitObject*)m;                               \
        lit_table_set(state, &klass->methods, nm, OBJECT_VALUE(m));       \
    }

#define LIT_BIND_STATIC_METHOD(name, method)                                                                        \
    {                                                                                                               \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                                 \
        lit_table_set(state, &klass->static_fields, nm, OBJECT_VALUE(lit_create_native_method(state, method, nm))); \
    }

#define LIT_BIND_STATIC_PRIMITIVE(name, method)                                                                        \
    {                                                                                                                  \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                                    \
        lit_table_set(state, &klass->static_fields, nm, OBJECT_VALUE(lit_create_primitive_method(state, method, nm))); \
    }

#define LIT_SET_STATIC_FIELD(name, field)                           \
    {                                                               \
        LitString* nm = lit_copy_string(state, name, strlen(name)); \
        lit_table_set(state, &klass->static_fields, nm, field);     \
    }

#define LIT_BIND_SETTER(name, setter)                                                                                        \
    {                                                                                                                        \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                                          \
        lit_table_set(state, &klass->methods, nm,                                                                            \
                      OBJECT_VALUE(lit_create_field(state, NULL, (LitObject*)lit_create_native_method(state, setter, nm)))); \
    }
#define LIT_BIND_GETTER(name, getter)                                                                                        \
    {                                                                                                                        \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                                          \
        lit_table_set(state, &klass->methods, nm,                                                                            \
                      OBJECT_VALUE(lit_create_field(state, (LitObject*)lit_create_native_method(state, getter, nm), NULL))); \
    }
#define LIT_BIND_FIELD(name, getter, setter)                                                                        \
    {                                                                                                               \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                                 \
        lit_table_set(state, &klass->methods, nm,                                                                   \
                      OBJECT_VALUE(lit_create_field(state, (LitObject*)lit_create_native_method(state, getter, nm), \
                                                    (LitObject*)lit_create_native_method(state, setter, nm))));     \
    }

#define LIT_BIND_STATIC_SETTER(name, setter)                                                                                 \
    {                                                                                                                        \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                                          \
        lit_table_set(state, &klass->static_fields, nm,                                                                      \
                      OBJECT_VALUE(lit_create_field(state, NULL, (LitObject*)lit_create_native_method(state, setter, nm)))); \
    }
#define LIT_BIND_STATIC_GETTER(name, getter)                                                                                 \
    {                                                                                                                        \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                                          \
        lit_table_set(state, &klass->static_fields, nm,                                                                      \
                      OBJECT_VALUE(lit_create_field(state, (LitObject*)lit_create_native_method(state, getter, nm), NULL))); \
    }
#define LIT_BIND_STATIC_FIELD(name, getter, setter)                                                                 \
    {                                                                                                               \
        LitString* nm = lit_copy_string(state, name, strlen(name));                                                 \
        lit_table_set(state, &klass->static_fields, nm,                                                             \
                      OBJECT_VALUE(lit_create_field(state, (LitObject*)lit_create_native_method(state, getter, nm), \
                                                    (LitObject*)lit_create_native_method(state, setter, nm))));     \
    }


#define LIT_CHECK_NUMBER(vm, args, argc, id) lit_check_number(vm, argv, argc, id)
#define LIT_GET_NUMBER(id, def) lit_get_number(vm, argv, argc, id, def)

#define LIT_CHECK_BOOL(id) lit_check_bool(vm, argv, argc, id)
#define LIT_GET_BOOL(id, def) lit_get_bool(vm, argv, argc, id, def)

#define LIT_CHECK_STRING(id) lit_check_string(vm, argv, argc, id)
#define LIT_GET_STRING(id, def) lit_get_string(vm, argv, argc, id, def)

#define LIT_CHECK_OBJECT_STRING(id) lit_check_object_string(vm, argv, argc, id)
#define LIT_CHECK_INSTANCE(id) lit_check_instance(vm, argv, argc, id)
#define LIT_CHECK_REFERENCE(id) lit_check_reference(vm, argv, argc, id)

#define LIT_GET_FIELD(id) lit_get_field(vm->state, &AS_INSTANCE(instance)->fields, id)
#define LIT_GET_MAP_FIELD(id) lit_get_map_field(vm->state, &AS_INSTANCE(instance)->fields, id)
#define LIT_SET_FIELD(id, value) lit_set_field(vm->state, &AS_INSTANCE(instance)->fields, id, value)
#define LIT_SET_MAP_FIELD(id, value) lit_set_map_field(vm->state, &AS_INSTANCE(instance)->fields, id, value)


#define LIT_ENSURE_ARGS(count)                                                   \
    if(argc != count)                                                       \
    {                                                                            \
        lit_runtime_error(vm, "Expected %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                       \
    }

#define LIT_ENSURE_MIN_ARGS(count)                                                       \
    if(argc < count)                                                                \
    {                                                                                    \
        lit_runtime_error(vm, "Expected minimum %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                               \
    }

#define LIT_ENSURE_MAX_ARGS(count)                                                       \
    if(argc > count)                                                                \
    {                                                                                    \
        lit_runtime_error(vm, "Expected maximum %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                               \
    }

#define LIT_INSERT_DATA(type, cleanup)                                                                                      \
    (                                                                                                                       \
    {                                                                                                                       \
        LitUserdata* userdata = lit_create_userdata(vm->state, sizeof(type));                                               \
        userdata->cleanup_fn = cleanup;                                                                                     \
        lit_table_set(vm->state, &AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), OBJECT_VALUE(userdata)); \
        (type*)userdata->data;                                                                                              \
    })

#define LIT_EXTRACT_DATA(type)                                                                    \
    (                                                                                             \
    {                                                                                             \
        LitValue _d;                                                                              \
        if(!lit_table_get(&AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), &_d)) \
        {                                                                                         \
            lit_runtime_error_exiting(vm, "Failed to extract userdata");                          \
        }                                                                                         \
        (type*)AS_USERDATA(_d)->data;                                                             \
    })

#define LIT_EXTRACT_DATA_FROM(from, type)                                                     \
    (                                                                                         \
    {                                                                                         \
        LitValue _d;                                                                          \
        if(!lit_table_get(&AS_INSTANCE(from)->fields, CONST_STRING(vm->state, "_data"), &_d)) \
        {                                                                                     \
            lit_runtime_error_exiting(vm, "Failed to extract userdata");                      \
        }                                                                                     \
        (type*)AS_USERDATA(_d)->data;                                                         \
    })


#define TABLE_MAX_LOAD 0.75
// Do not change these, or old bytecode files will break!
#define LIT_BYTECODE_MAGIC_NUMBER 6932
#define LIT_BYTECODE_END_NUMBER 2942
#define LIT_STRING_KEY 48



typedef uint64_t LitValue;

enum LitOpCode
{
#define OPCODE(name, effect) OP_##name,
#include "opcodes.inc"
#undef OPCODE
};

enum LitExpressionType
{
    LITERAL_EXPRESSION,
    BINARY_EXPRESSION,
    UNARY_EXPRESSION,
    VAR_EXPRESSION,
    ASSIGN_EXPRESSION,
    CALL_EXPRESSION,
    SET_EXPRESSION,
    GET_EXPRESSION,
    LAMBDA_EXPRESSION,
    ARRAY_EXPRESSION,
    OBJECT_EXPRESSION,
    SUBSCRIPT_EXPRESSION,
    THIS_EXPRESSION,
    SUPER_EXPRESSION,
    RANGE_EXPRESSION,
    IF_EXPRESSION,
    INTERPOLATION_EXPRESSION,
    REFERENCE_EXPRESSION
};

enum LitOptimizationLevel
{
    OPTIMIZATION_LEVEL_NONE,
    OPTIMIZATION_LEVEL_REPL,
    OPTIMIZATION_LEVEL_DEBUG,
    OPTIMIZATION_LEVEL_RELEASE,
    OPTIMIZATION_LEVEL_EXTREME,

    OPTIMIZATION_LEVEL_TOTAL
};

enum LitOptimization
{
    OPTIMIZATION_CONSTANT_FOLDING,
    OPTIMIZATION_LITERAL_FOLDING,
    OPTIMIZATION_UNUSED_VAR,
    OPTIMIZATION_UNREACHABLE_CODE,
    OPTIMIZATION_EMPTY_BODY,
    OPTIMIZATION_LINE_INFO,
    OPTIMIZATION_PRIVATE_NAMES,
    OPTIMIZATION_C_FOR,

    OPTIMIZATION_TOTAL
};

enum LitError
{
    // Preprocessor errors
    ERROR_UNCLOSED_MACRO,
    ERROR_UNKNOWN_MACRO,

    // Scanner errors
    ERROR_UNEXPECTED_CHAR,
    ERROR_UNTERMINATED_STRING,
    ERROR_INVALID_ESCAPE_CHAR,
    ERROR_INTERPOLATION_NESTING_TOO_DEEP,
    ERROR_NUMBER_IS_TOO_BIG,
    ERROR_CHAR_EXPECTATION_UNMET,

    // Parser errors
    ERROR_EXPECTATION_UNMET,
    ERROR_INVALID_ASSIGMENT_TARGET,
    ERROR_TOO_MANY_FUNCTION_ARGS,
    ERROR_MULTIPLE_ELSE_BRANCHES,
    ERROR_VAR_MISSING_IN_FORIN,
    ERROR_NO_GETTER_AND_SETTER,
    ERROR_STATIC_OPERATOR,
    ERROR_SELF_INHERITED_CLASS,
    ERROR_STATIC_FIELDS_AFTER_METHODS,
    ERROR_MISSING_STATEMENT,
    ERROR_EXPECTED_EXPRESSION,
    ERROR_DEFAULT_ARG_CENTRED,

    // Emitter errors
    ERROR_TOO_MANY_CONSTANTS,
    ERROR_TOO_MANY_PRIVATES,
    ERROR_VAR_REDEFINED,
    ERROR_TOO_MANY_LOCALS,
    ERROR_TOO_MANY_UPVALUES,
    ERROR_VARIABLE_USED_IN_INIT,
    ERROR_JUMP_TOO_BIG,
    ERROR_NO_SUPER,
    ERROR_THIS_MISSUSE,
    ERROR_SUPER_MISSUSE,
    ERROR_UNKNOWN_EXPRESSION,
    ERROR_UNKNOWN_STATEMENT,
    ERROR_LOOP_JUMP_MISSUSE,
    ERROR_RETURN_FROM_CONSTRUCTOR,
    ERROR_STATIC_CONSTRUCTOR,
    ERROR_CONSTANT_MODIFIED,
    ERROR_INVALID_REFERENCE_TARGET,

    ERROR_TOTAL
};

enum LitPrecedence
{
    PREC_NONE,
    PREC_ASSIGNMENT,// =
    PREC_OR,// ||
    PREC_AND,// &&
    PREC_BOR,// | ^
    PREC_BAND,// &
    PREC_SHIFT,// << >>
    PREC_EQUALITY,// == !=
    PREC_COMPARISON,// < > <= >=
    PREC_COMPOUND,// += -= *= /= ++ --
    PREC_TERM,// + -
    PREC_FACTOR,// * /
    PREC_IS,// is
    PREC_RANGE,// ..
    PREC_UNARY,// ! - ~
    PREC_NULL,// ??
    PREC_CALL,// . ()
    PREC_PRIMARY
};

enum LitStatementType
{
    EXPRESSION_STATEMENT,
    BLOCK_STATEMENT,
    IF_STATEMENT,
    WHILE_STATEMENT,
    FOR_STATEMENT,
    VAR_STATEMENT,
    CONTINUE_STATEMENT,
    BREAK_STATEMENT,
    FUNCTION_STATEMENT,
    RETURN_STATEMENT,
    METHOD_STATEMENT,
    CLASS_STATEMENT,
    FIELD_STATEMENT
};

enum LitTokenType
{
    TOKEN_NEW_LINE,

    // Single-character tokens.
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,

    // One or two character tokens.
    TOKEN_BAR_EQUAL,
    TOKEN_BAR,
    TOKEN_BAR_BAR,
    TOKEN_AMPERSAND_EQUAL,
    TOKEN_AMPERSAND,
    TOKEN_AMPERSAND_AMPERSAND,
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_GREATER_GREATER,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_LESS_LESS,
    TOKEN_PLUS,
    TOKEN_PLUS_EQUAL,
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS,
    TOKEN_MINUS_EQUAL,
    TOKEN_MINUS_MINUS,
    TOKEN_STAR,
    TOKEN_STAR_EQUAL,
    TOKEN_STAR_STAR,
    TOKEN_SLASH,
    TOKEN_SLASH_EQUAL,
    TOKEN_QUESTION,
    TOKEN_QUESTION_QUESTION,
    TOKEN_PERCENT,
    TOKEN_PERCENT_EQUAL,
    TOKEN_ARROW,
    TOKEN_SMALL_ARROW,
    TOKEN_TILDE,
    TOKEN_CARET,
    TOKEN_CARET_EQUAL,
    TOKEN_DOT,
    TOKEN_DOT_DOT,
    TOKEN_DOT_DOT_DOT,
    TOKEN_SHARP,
    TOKEN_SHARP_EQUAL,

    // Literals.
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_INTERPOLATION,
    TOKEN_NUMBER,

    // Keywords.
    TOKEN_CLASS,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUNCTION,
    TOKEN_IF,
    TOKEN_NULL,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,
    TOKEN_CONTINUE,
    TOKEN_BREAK,
    TOKEN_NEW,
    TOKEN_EXPORT,
    TOKEN_IS,
    TOKEN_STATIC,
    TOKEN_OPERATOR,
    TOKEN_GET,
    TOKEN_SET,
    TOKEN_IN,
    TOKEN_CONST,
    TOKEN_REF,

    TOKEN_ERROR,
    TOKEN_EOF
};

enum LitInterpretResultType
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_INVALID
};

enum LitErrorType
{
    COMPILE_ERROR,
    RUNTIME_ERROR
};

enum LitFunctionType
{
    FUNCTION_REGULAR,
    FUNCTION_SCRIPT,
    FUNCTION_METHOD,
    FUNCTION_STATIC_METHOD,
    FUNCTION_CONSTRUCTOR
};

enum LitObjectType
{
    OBJECT_STRING,
    OBJECT_FUNCTION,
    OBJECT_NATIVE_FUNCTION,
    OBJECT_NATIVE_PRIMITIVE,
    OBJECT_NATIVE_METHOD,
    OBJECT_PRIMITIVE_METHOD,
    OBJECT_FIBER,
    OBJECT_MODULE,
    OBJECT_CLOSURE,
    OBJECT_UPVALUE,
    OBJECT_CLASS,
    OBJECT_INSTANCE,
    OBJECT_BOUND_METHOD,
    OBJECT_ARRAY,
    OBJECT_MAP,
    OBJECT_USERDATA,
    OBJECT_RANGE,
    OBJECT_FIELD,
    OBJECT_REFERENCE
};

typedef enum LitOpCode LitOpCode;
typedef enum LitExpressionType LitExpressionType;
typedef enum LitOptimizationLevel LitOptimizationLevel;
typedef enum LitOptimization LitOptimization;
typedef enum LitError LitError;
typedef enum LitPrecedence LitPrecedence;
typedef enum LitStatementType LitStatementType;
typedef enum LitTokenType LitTokenType;
typedef enum LitInterpretResultType LitInterpretResultType;
typedef enum LitErrorType LitErrorType;
typedef enum LitFunctionType LitFunctionType;
typedef enum LitObjectType LitObjectType;
typedef struct LitScanner LitScanner;
typedef struct LitPreprocessor LitPreprocessor;
typedef struct LitVm LitVm;
typedef struct LitParser LitParser;
typedef struct LitEmitter LitEmitter;
typedef struct LitOptimizer LitOptimizer;
typedef struct LitState LitState;
typedef struct LitInterpretResult LitInterpretResult;
typedef struct LitObject LitObject;
typedef struct LitMap LitMap;
typedef struct LitString LitString;
typedef struct LitModule LitModule;
typedef struct LitFiber LitFiber;
typedef struct LitUserdata LitUserdata;
typedef struct LitChunk LitChunk;
typedef struct LitTableEntry LitTableEntry;
typedef struct LitTable LitTable;
typedef struct LitFunction LitFunction;
typedef struct LitUpvalue LitUpvalue;
typedef struct LitClosure LitClosure;
typedef struct LitNativeFunction LitNativeFunction;
typedef struct LitNativePrimitive LitNativePrimitive;
typedef struct LitNativeMethod LitNativeMethod;
typedef struct LitPrimitiveMethod LitPrimitiveMethod;
typedef struct LitCallFrame LitCallFrame;
typedef struct LitClass LitClass;
typedef struct LitInstance LitInstance;
typedef struct LitBoundMethod LitBoundMethod;
typedef struct LitArray LitArray;
typedef struct LitRange LitRange;
typedef struct LitField LitField;
typedef struct LitReference LitReference;
typedef struct LitToken LitToken;
typedef struct LitExpression LitExpression;
typedef struct LitStatement LitStatement;
typedef struct LitLiteralExpression LitLiteralExpression;
typedef struct LitBinaryExpression LitBinaryExpression;
typedef struct LitUnaryExpression LitUnaryExpression;
typedef struct LitVarExpression LitVarExpression;
typedef struct LitAssignExpression LitAssignExpression;
typedef struct LitCallExpression LitCallExpression;
typedef struct LitGetExpression LitGetExpression;
typedef struct LitSetExpression LitSetExpression;
typedef struct LitParameter LitParameter;
typedef struct LitLambdaExpression LitLambdaExpression;
typedef struct LitArrayExpression LitArrayExpression;
typedef struct LitObjectExpression LitObjectExpression;
typedef struct LitSubscriptExpression LitSubscriptExpression;
typedef struct LitThisExpression LitThisExpression;
typedef struct LitSuperExpression LitSuperExpression;
typedef struct LitRangeExpression LitRangeExpression;
typedef struct LitIfExpression LitIfExpression;
typedef struct LitInterpolationExpression LitInterpolationExpression;
typedef struct LitReferenceExpression LitReferenceExpression;
typedef struct LitExpressionStatement LitExpressionStatement;
typedef struct LitBlockStatement LitBlockStatement;
typedef struct LitVarStatement LitVarStatement;
typedef struct LitIfStatement LitIfStatement;
typedef struct LitWhileStatement LitWhileStatement;
typedef struct LitForStatement LitForStatement;
typedef struct LitContinueStatement LitContinueStatement;
typedef struct LitBreakStatement LitBreakStatement;
typedef struct LitFunctionStatement LitFunctionStatement;
typedef struct LitReturnStatement LitReturnStatement;
typedef struct LitMethodStatement LitMethodStatement;
typedef struct LitClassStatement LitClassStatement;
typedef struct LitFieldStatement LitFieldStatement;
typedef struct LitPrivate LitPrivate;
typedef struct LitLocal LitLocal;
typedef struct LitCompilerUpvalue LitCompilerUpvalue;
typedef struct LitCompiler LitCompiler;
typedef struct LitParseRule LitParseRule;
typedef struct LitEmulatedFile LitEmulatedFile;
typedef struct LitVariable LitVariable;

/* ARRAYTYPES */
typedef struct LitVariables LitVariables;
typedef struct LitBytes LitBytes;
typedef struct LitUInts LitUInts;
typedef struct LitValues LitValues;
typedef struct LitExpressions LitExpressions;
typedef struct LitParameters LitParameters;
typedef struct LitStatements LitStatements;
typedef struct LitPrivates LitPrivates;
typedef struct LitLocals LitLocals;

typedef LitValue (*LitNativeFunctionFn)(LitVm* vm, size_t arg_count, LitValue* args);
typedef bool (*LitNativePrimitiveFn)(LitVm* vm, size_t arg_count, LitValue* args);
typedef LitValue (*LitNativeMethodFn)(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args);
typedef bool (*LitPrimitiveMethodFn)(LitVm* vm, LitValue instance, size_t arg_count, LitValue* args);
typedef LitValue (*LitMapIndexFn)(LitVm* vm, LitMap* map, LitString* index, LitValue* value);
typedef void (*LitCleanupFn)(LitState* state, LitUserdata* userdata, bool mark);
typedef void (*LitErrorFn)(LitState* state, const char* message);
typedef void (*LitPrintFn)(LitState* state, const char* message);
typedef LitExpression* (*LitPrefixParseFn)(LitParser*, bool);
typedef LitExpression* (*LitInfixParseFn)(LitParser*, LitExpression*, bool);

struct LitUInts
{
    size_t capacity;
    size_t count;
    size_t* values;
};

struct LitBytes
{
    size_t capacity;
    size_t count;
    uint8_t* values;
};

struct LitValues
{
    size_t capacity;
    size_t count;
    LitValue* values;
};

struct LitChunk
{
    size_t count;
    size_t capacity;
    uint8_t* code;
    bool has_line_info;
    size_t line_count;
    size_t line_capacity;
    uint16_t* lines;
    LitValues constants;
};

struct LitTableEntry
{
    LitString* key;
    LitValue value;
};

struct LitTable
{
    int count;
    int capacity;
    LitTableEntry* entries;
};

struct LitObject
{
    LitObjectType type;
    LitObject* next;
    bool marked;
};

struct LitString
{
    LitObject object;
    size_t length;
    uint32_t hash;
    char* chars;
};

struct LitFunction
{
    LitObject object;
    LitChunk chunk;
    LitString* name;
    uint8_t arg_count;
    uint16_t upvalue_count;
    size_t max_slots;
    bool vararg;
    LitModule* module;
};

struct LitUpvalue
{
    LitObject object;
    LitValue* location;
    LitValue closed;
    LitUpvalue* next;
};

struct LitClosure
{
    LitObject object;
    LitFunction* function;
    LitUpvalue** upvalues;
    size_t upvalue_count;
};

struct LitNativeFunction
{
    LitObject object;
    LitNativeFunctionFn function;
    LitString* name;
};

struct LitNativePrimitive
{
    LitObject object;
    LitNativePrimitiveFn function;
    LitString* name;
};

struct LitNativeMethod
{
    LitObject object;
    LitNativeMethodFn method;
    LitString* name;
};

struct LitPrimitiveMethod
{
    LitObject object;
    LitPrimitiveMethodFn method;
    LitString* name;
};

struct LitCallFrame
{
    LitFunction* function;
    LitClosure* closure;
    uint8_t* ip;
    LitValue* slots;
    bool result_ignored;
    bool return_to_c;
};

struct LitMap
{
    LitObject object;
    LitTable values;
    LitMapIndexFn index_fn;
};

struct LitModule
{
    LitObject object;
    LitValue return_value;
    LitString* name;
    LitValue* privates;
    LitMap* private_names;
    size_t private_count;
    LitFunction* main_function;
    LitFiber* main_fiber;
    bool ran;
};

struct LitFiber
{
    LitObject object;
    LitFiber* parent;
    LitValue* stack;
    LitValue* stack_top;
    size_t stack_capacity;
    LitCallFrame* frames;
    size_t frame_capacity;
    size_t frame_count;
    size_t arg_count;
    LitUpvalue* open_upvalues;
    LitModule* module;
    LitValue error;
    bool abort;
    bool catcher;
};

struct LitClass
{
    LitObject object;
    LitString* name;
    LitObject* init_method;
    LitTable methods;
    LitTable static_fields;
    LitClass* super;
};

struct LitInstance
{
    LitObject object;
    LitClass* klass;
    LitTable fields;
};

struct LitBoundMethod
{
    LitObject object;
    LitValue receiver;
    LitValue method;
};

struct LitArray
{
    LitObject object;
    LitValues values;
};

struct LitUserdata
{
    LitObject object;
    void* data;
    size_t size;
    LitCleanupFn cleanup_fn;
};

struct LitRange
{
    LitObject object;
    double from;
    double to;
};

struct LitField
{
    LitObject object;
    LitObject* getter;
    LitObject* setter;
};

struct LitReference
{
    LitObject object;
    LitValue* slot;
};

struct LitState
{
    int64_t bytes_allocated;
    int64_t next_gc;
    bool allow_gc;
    LitErrorFn error_fn;
    LitPrintFn print_fn;
    LitValue* roots;
    size_t root_count;
    size_t root_capacity;
    LitPreprocessor* preprocessor;
    LitScanner* scanner;
    LitParser* parser;
    LitEmitter* emitter;
    LitOptimizer* optimizer;
    LitVm* vm;
    bool had_error;
    LitFunction* api_function;
    LitFiber* api_fiber;
    LitString* api_name;
    // Mental note:
    // When adding another class here, DO NOT forget to mark it in lit_mem.c or it will be GC-ed
    LitClass* class_class;
    LitClass* object_class;
    LitClass* number_class;
    LitClass* string_class;
    LitClass* bool_class;
    LitClass* function_class;
    LitClass* fiber_class;
    LitClass* module_class;
    LitClass* array_class;
    LitClass* map_class;
    LitClass* range_class;
    LitModule* last_module;
};

struct LitVm
{
    LitState* state;
    LitObject* objects;
    LitTable strings;
    LitMap* modules;
    LitMap* globals;
    LitFiber* fiber;
    // For garbage collection
    size_t gray_count;
    size_t gray_capacity;
    LitObject** gray_stack;
};

struct LitInterpretResult
{
    LitInterpretResultType type;
    LitValue result;
};

struct LitToken
{
    const char* start;
    LitTokenType type;
    size_t length;
    size_t line;
    LitValue value;
};

struct LitExpression
{
    LitExpressionType type;
    size_t line;
};

struct LitStatement
{
    LitStatementType type;
    size_t line;
};

/*
 * Expressions
 */
struct LitExpressions
{
    size_t capacity;
    size_t count;
    LitExpression** values;
};

struct LitLiteralExpression
{
    LitExpression expression;
    LitValue value;
};

struct LitBinaryExpression
{
    LitExpression expression;
    LitExpression* left;
    LitExpression* right;
    LitTokenType op;
    bool ignore_left;
};

struct LitUnaryExpression
{
    LitExpression expression;
    LitExpression* right;
    LitTokenType op;
};

struct LitVarExpression
{
    LitExpression expression;
    const char* name;
    size_t length;
};

struct LitAssignExpression
{
    LitExpression expression;
    LitExpression* to;
    LitExpression* value;
};

struct LitCallExpression
{
    LitExpression expression;
    LitExpression* callee;
    LitExpressions args;
    LitExpression* init;
};

struct LitGetExpression
{
    LitExpression expression;
    LitExpression* where;
    const char* name;
    size_t length;
    int jump;
    bool ignore_emit;
    bool ignore_result;
};

struct LitSetExpression
{
    LitExpression expression;
    LitExpression* where;
    const char* name;
    size_t length;
    LitExpression* value;
};

struct LitParameter
{
    const char* name;
    size_t length;
    LitExpression* default_value;
};

struct LitParameters
{
    size_t capacity;
    size_t count;
    LitParameter* values;
};


struct LitLambdaExpression
{
    LitExpression expression;
    LitParameters parameters;
    LitStatement* body;
};

struct LitArrayExpression
{
    LitExpression expression;
    LitExpressions values;
};

struct LitObjectExpression
{
    LitExpression expression;
    LitValues keys;
    LitExpressions values;
};
struct LitSubscriptExpression
{
    LitExpression expression;
    LitExpression* array;
    LitExpression* index;
};
struct LitThisExpression
{
    LitExpression expression;
};
struct LitSuperExpression
{
    LitExpression expression;
    LitString* method;
    bool ignore_emit;
    bool ignore_result;
};
struct LitRangeExpression
{
    LitExpression expression;
    LitExpression* from;
    LitExpression* to;
};
struct LitIfExpression
{
    LitStatement statement;
    LitExpression* condition;
    LitExpression* if_branch;
    LitExpression* else_branch;
};
struct LitInterpolationExpression
{
    LitExpression expression;
    LitExpressions expressions;
};
struct LitReferenceExpression
{
    LitExpression expression;
    LitExpression* to;
};
/*
 * Statements
 */
struct LitStatements
{
    size_t capacity;
    size_t count;
    LitStatement** values;
};


struct LitExpressionStatement
{
    LitStatement statement;
    LitExpression* expression;
    bool pop;
};
struct LitBlockStatement
{
    LitStatement statement;
    LitStatements statements;
};
struct LitVarStatement
{
    LitStatement statement;
    const char* name;
    size_t length;
    bool constant;
    LitExpression* init;
};
struct LitIfStatement
{
    LitStatement statement;
    LitExpression* condition;
    LitStatement* if_branch;
    LitStatement* else_branch;
    LitExpressions* elseif_conditions;
    LitStatements* elseif_branches;
};
struct LitWhileStatement
{
    LitStatement statement;
    LitExpression* condition;
    LitStatement* body;
};
struct LitForStatement
{
    LitStatement statement;
    LitExpression* init;
    LitStatement* var;
    LitExpression* condition;
    LitExpression* increment;
    LitStatement* body;
    bool c_style;
};
struct LitContinueStatement
{
    LitStatement statement;
};
struct LitBreakStatement
{
    LitStatement statement;
};
struct LitFunctionStatement
{
    LitStatement statement;
    const char* name;
    size_t length;
    LitParameters parameters;
    LitStatement* body;
    bool exported;
};
struct LitReturnStatement
{
    LitStatement statement;
    LitExpression* expression;
};
struct LitMethodStatement
{
    LitStatement statement;
    LitString* name;
    LitParameters parameters;
    LitStatement* body;
    bool is_static;
};
struct LitClassStatement
{
    LitStatement statement;
    LitString* name;
    LitString* parent;
    LitStatements fields;
};
struct LitFieldStatement
{
    LitStatement statement;
    LitString* name;
    LitStatement* getter;
    LitStatement* setter;
    bool is_static;
};
struct LitPrivate
{
    bool initialized;
    bool constant;
};
struct LitPrivates
{
    size_t capacity;
    size_t count;
    LitPrivate* values;
};


struct LitLocal
{
    const char* name;
    size_t length;
    int depth;
    bool captured;
    bool constant;
};
struct LitLocals
{
    size_t capacity;
    size_t count;
    LitLocal* values;
};

struct LitCompilerUpvalue
{
    uint8_t index;
    bool isLocal;
};
struct LitCompiler
{
    LitLocals locals;
    int scope_depth;
    LitFunction* function;
    LitFunctionType type;
    LitCompilerUpvalue upvalues[UINT8_COUNT];
    struct LitCompiler* enclosing;
    bool skip_return;
    size_t loop_depth;
    int slots;
    int max_slots;
};
struct LitEmitter
{
    LitState* state;
    LitChunk* chunk;
    LitCompiler* compiler;
    size_t last_line;
    size_t loop_start;
    LitPrivates privates;
    LitUInts breaks;
    LitUInts continues;
    LitModule* module;
    LitString* class_name;
    bool class_has_super;
    bool previous_was_expression_statement;
    int emit_reference;
};
struct LitParseRule
{
    LitPrefixParseFn prefix;
    LitInfixParseFn infix;
    LitPrecedence precedence;
};
struct LitParser
{
    LitState* state;
    bool had_error;
    bool panic_mode;
    LitToken previous;
    LitToken current;
    LitCompiler* compiler;
    uint8_t expression_root_count;
    uint8_t statement_root_count;
};
struct LitEmulatedFile
{
    const char* source;
    size_t position;
};
struct LitScanner
{
    size_t line;
    const char* start;
    const char* current;
    const char* file_name;
    LitState* state;
    size_t braces[LIT_MAX_INTERPOLATION_NESTING];
    size_t num_braces;
    bool had_error;
};
struct LitVariable
{
    const char* name;
    size_t length;
    int depth;
    bool constant;
    bool used;
    LitValue constant_value;
    LitStatement** declaration;
};

struct LitVariables
{
    size_t capacity;
    size_t count;
    LitVariable* values;
};


struct LitOptimizer
{
    LitState* state;
    LitVariables variables;
    int depth;
    bool mark_used;
};
struct LitPreprocessor
{
    LitState* state;
    LitTable defined;
    /*
	 * A little bit dirty hack:
	 * We need to store pointers (8 bytes in size),
	 * and so that we don't have to declare a new
	 * array type, that we will use only once,
	 * I'm using LitValues here, because LitValue
	 * also has the same size (8 bytes)
	 */
    LitValues open_ifs;
};


static const char* lit_object_type_names[] =
{
    "string",
    "function",
    "native_function",
    "native_primitive",
    "native_method",
    "primitive_method",
    "fiber",
    "module",
    "closure",
    "upvalue",
    "class",
    "instance",
    "bound_method",
    "array",
    "map",
    "userdata",
    "range",
    "field",
    "reference"
};

static inline double lit_value_to_number(LitValue value)
{
    return *((double*)&value);
}

static inline LitValue lit_number_to_value(double num)
{
    return *((LitValue*)&num);
}

static inline bool lit_is_falsey(LitValue value)
{
    return (IS_BOOL(value) && value == FALSE_VALUE) || IS_NULL(value) || (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

void lit_init_uints(LitUInts* array);
void lit_free_uints(LitState* state, LitUInts* array);
void lit_uints_write(LitState* state, LitUInts* array, size_t value);


void lit_init_bytes(LitBytes* array);
void lit_free_bytes(LitState* state, LitBytes* array);
void lit_bytes_write(LitState* state, LitBytes* array, uint8_t value);

void lit_init_values(LitValues* array);
void lit_free_values(LitState* state, LitValues* array);
void lit_values_write(LitState* state, LitValues* array, LitValue value);


void lit_init_expressions(LitExpressions* array);
void lit_free_expressions(LitState* state, LitExpressions* array);
void lit_expressions_write(LitState* state, LitExpressions* array, LitExpression* value);

void lit_init_parameters(LitParameters* array);
void lit_free_parameters(LitState* state, LitParameters* array);
void lit_parameters_write(LitState* state, LitParameters* array, LitParameter value);

void lit_init_stataments(LitStatements* array);
void lit_free_stataments(LitState* state, LitStatements* array);
void lit_stataments_write(LitState* state, LitStatements* array, LitStatement* value);


void lit_init_privates(LitPrivates* array);
void lit_free_privates(LitState* state, LitPrivates* array);
void lit_privates_write(LitState* state, LitPrivates* array, LitPrivate value);

void lit_init_locals(LitLocals* array);
void lit_free_locals(LitState* state, LitLocals* array);
void lit_locals_write(LitState* state, LitLocals* array, LitLocal value);

void lit_init_variables(LitVariables* array);
void lit_free_variables(LitState* state, LitVariables* array);
void lit_variables_write(LitState* state, LitVariables* array, LitVariable value);

LitState* lit_new_state();
int64_t lit_free_state(LitState* state);

void lit_push_root(LitState* state, LitObject* object);
void lit_push_value_root(LitState* state, LitValue value);
LitValue lit_peek_root(LitState* state, uint8_t distance);
void lit_pop_root(LitState* state);
void lit_pop_roots(LitState* state, uint8_t amount);

LitClass* lit_get_class_for(LitState* state, LitValue value);

char* lit_patch_file_name(char* file_name);

void lit_print_value(LitValue value);
void lit_values_ensure_size(LitState* state, LitValues* values, size_t size);
const char* lit_get_value_type(LitValue value);
void* lit_reallocate(LitState* state, void* pointer, size_t old_size, size_t new_size);
void lit_free_objects(LitState* state, LitObject* objects);
uint64_t lit_collect_garbage(LitVm* vm);
void lit_mark_object(LitVm* vm, LitObject* object);
void lit_mark_value(LitVm* vm, LitValue value);
void lit_free_object(LitState* state, LitObject* object);
int lit_closest_power_of_two(int n);
void lit_init_chunk(LitChunk* chunk);
void lit_free_chunk(LitState* state, LitChunk* chunk);
void lit_write_chunk(LitState* state, LitChunk* chunk, uint8_t byte, uint16_t line);
size_t lit_chunk_add_constant(LitState* state, LitChunk* chunk, LitValue constant);
size_t lit_chunk_get_line(LitChunk* chunk, size_t offset);
void lit_shrink_chunk(LitState* state, LitChunk* chunk);
void lit_emit_byte(LitState* state, LitChunk* chunk, uint8_t byte);
void lit_emit_bytes(LitState* state, LitChunk* chunk, uint8_t a, uint8_t b);
void lit_emit_short(LitState* state, LitChunk* chunk, uint16_t value);
void lit_init_table(LitTable* table);
void lit_free_table(LitState* state, LitTable* table);
bool lit_table_set(LitState* state, LitTable* table, LitString* key, LitValue value);
bool lit_table_get(LitTable* table, LitString* key, LitValue* value);
bool lit_table_get_slot(LitTable* table, LitString* key, LitValue** value);
bool lit_table_delete(LitTable* table, LitString* key);
LitString* lit_table_find_string(LitTable* table, const char* chars, size_t length, uint32_t hash);
void lit_table_add_all(LitState* state, LitTable* from, LitTable* to);
void lit_table_remove_white(LitTable* table);
void lit_mark_table(LitVm* vm, LitTable* table);
bool lit_is_callable_function(LitValue value);
LitObject* lit_allocate_object(LitState* state, size_t size, LitObjectType type);
LitString* lit_copy_string(LitState* state, const char* chars, size_t length);
LitString* lit_take_string(LitState* state, const char* chars, size_t length);
LitValue lit_string_format(LitState* state, const char* format, ...);
LitValue lit_number_to_string(LitState* state, double value);
void lit_register_string(LitState* state, LitString* string);
uint32_t lit_hash_string(const char* key, size_t length);
LitString* lit_allocate_empty_string(LitState* state, size_t length);
LitFunction* lit_create_function(LitState* state, LitModule* module);
LitValue lit_get_function_name(LitVm* vm, LitValue instance);
LitUpvalue* lit_create_upvalue(LitState* state, LitValue* slot);
LitClosure* lit_create_closure(LitState* state, LitFunction* function);
LitNativeFunction* lit_create_native_function(LitState* state, LitNativeFunctionFn function, LitString* name);
LitNativePrimitive* lit_create_native_primitive(LitState* state, LitNativePrimitiveFn function, LitString* name);
LitNativeMethod* lit_create_native_method(LitState* state, LitNativeMethodFn function, LitString* name);
LitPrimitiveMethod* lit_create_primitive_method(LitState* state, LitPrimitiveMethodFn method, LitString* name);
LitMap* lit_create_map(LitState* state);
bool lit_map_set(LitState* state, LitMap* map, LitString* key, LitValue value);
bool lit_map_get(LitMap* map, LitString* key, LitValue* value);
bool lit_map_delete(LitMap* map, LitString* key);
void lit_map_add_all(LitState* state, LitMap* from, LitMap* to);
LitModule* lit_create_module(LitState* state, LitString* name);
LitFiber* lit_create_fiber(LitState* state, LitModule* module, LitFunction* function);
void lit_ensure_fiber_stack(LitState* state, LitFiber* fiber, size_t needed);
LitClass* lit_create_class(LitState* state, LitString* name);
LitInstance* lit_create_instance(LitState* state, LitClass* klass);
LitBoundMethod* lit_create_bound_method(LitState* state, LitValue receiver, LitValue method);
LitArray* lit_create_array(LitState* state);
LitUserdata* lit_create_userdata(LitState* state, size_t size);
LitRange* lit_create_range(LitState* state, double from, double to);
LitField* lit_create_field(LitState* state, LitObject* getter, LitObject* setter);
LitReference* lit_create_reference(LitState* state, LitValue* slot);

/*
 * Please, do not provide a const string source to the compiler, because it will
 * get modified, if it has any macros in it!
 */
LitModule* lit_compile_module(LitState* state, LitString* module_name, char* code);
LitModule* lit_get_module(LitState* state, const char* name);

LitInterpretResult lit_internal_interpret(LitState* state, LitString* module_name, char* code);
LitInterpretResult lit_interpret(LitState* state, const char* module_name, char* code);
LitInterpretResult lit_interpret_file(LitState* state, const char* file);
LitInterpretResult lit_dump_file(LitState* state, const char* file);
bool lit_compile_and_save_files(LitState* state, char* files[], size_t num_files, const char* output_file);

void lit_error(LitState* state, LitErrorType type, const char* message, ...);
void lit_printf(LitState* state, const char* message, ...);
void lit_enable_compilation_time_measurement();

void lit_init_vm(LitState* state, LitVm* vm);
void lit_free_vm(LitVm* vm);
void lit_trace_vm_stack(LitVm* vm);

static inline void lit_push(LitVm* vm, LitValue value)
{
    *vm->fiber->stack_top++ = value;
}

static inline LitValue lit_pop(LitVm* vm)
{
    return *(--vm->fiber->stack_top);
}

LitInterpretResult lit_interpret_module(LitState* state, LitModule* module);
LitInterpretResult lit_interpret_fiber(LitState* state, LitFiber* fiber);
bool lit_handle_runtime_error(LitVm* vm, LitString* error_string);
bool lit_vruntime_error(LitVm* vm, const char* format, va_list args);
bool lit_runtime_error(LitVm* vm, const char* format, ...);
bool lit_runtime_error_exiting(LitVm* vm, const char* format, ...);

void lit_native_exit_jump();
bool lit_set_native_exit_jump();


LitInterpretResult lit_call_function(LitState* state, LitFunction* callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult lit_call_method(LitState* state, LitValue instance, LitValue callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult lit_call(LitState* state, LitValue callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult
lit_find_and_call_method(LitState* state, LitValue callee, LitString* method_name, LitValue* arguments, uint8_t argument_count);

LitString* lit_to_string(LitState* state, LitValue object);
LitValue lit_call_new(LitVm* vm, const char* name, LitValue* args, size_t arg_count);


void lit_init_api(LitState* state);
void lit_free_api(LitState* state);

LitValue lit_get_global(LitState* state, LitString* name);
LitFunction* lit_get_global_function(LitState* state, LitString* name);

void lit_set_global(LitState* state, LitString* name, LitValue value);
bool lit_global_exists(LitState* state, LitString* name);
void lit_define_native(LitState* state, const char* name, LitNativeFunctionFn native);
void lit_define_native_primitive(LitState* state, const char* name, LitNativePrimitiveFn native);

double lit_check_number(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id);
double lit_get_number(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id, double def);

bool lit_check_bool(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id);
bool lit_get_bool(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id, bool def);

const char* lit_check_string(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id);
const char* lit_get_string(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id, const char* def);

LitString* lit_check_object_string(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id);
LitInstance* lit_check_instance(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id);
LitValue* lit_check_reference(LitVm* vm, LitValue* args, uint8_t arg_count, uint8_t id);

void lit_ensure_bool(LitVm* vm, LitValue value, const char* error);
void lit_ensure_string(LitVm* vm, LitValue value, const char* error);
void lit_ensure_number(LitVm* vm, LitValue value, const char* error);
void lit_ensure_object_type(LitVm* vm, LitValue value, LitObjectType type, const char* error);


LitValue lit_get_field(LitState* state, LitTable* table, const char* name);
LitValue lit_get_map_field(LitState* state, LitMap* map, const char* name);

void lit_set_field(LitState* state, LitTable* table, const char* name, LitValue value);
void lit_set_map_field(LitState* state, LitMap* map, const char* name, LitValue value);
void lit_disassemble_module(LitModule* module, const char* source);
void lit_disassemble_chunk(LitChunk* chunk, const char* name, const char* source);
size_t lit_disassemble_instruction(LitChunk* chunk, size_t offset, const char* source);

void lit_trace_frame(LitFiber* fiber);


void lit_free_expression(LitState* state, LitExpression* expression);
LitLiteralExpression* lit_create_literal_expression(LitState* state, size_t line, LitValue value);

LitBinaryExpression*
lit_create_binary_expression(LitState* state, size_t line, LitExpression* left, LitExpression* right, LitTokenType op);
LitUnaryExpression* lit_create_unary_expression(LitState* state, size_t line, LitExpression* right, LitTokenType op);
LitVarExpression* lit_create_var_expression(LitState* state, size_t line, const char* name, size_t length);
LitAssignExpression* lit_create_assign_expression(LitState* state, size_t line, LitExpression* to, LitExpression* value);
LitCallExpression* lit_create_call_expression(LitState* state, size_t line, LitExpression* callee);
LitGetExpression*
lit_create_get_expression(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, bool questionable, bool ignore_result);
LitSetExpression*
lit_create_set_expression(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, LitExpression* value);
LitLambdaExpression* lit_create_lambda_expression(LitState* state, size_t line);
LitArrayExpression* lit_create_array_expression(LitState* state, size_t line);
LitObjectExpression* lit_create_object_expression(LitState* state, size_t line);
LitSubscriptExpression* lit_create_subscript_expression(LitState* state, size_t line, LitExpression* array, LitExpression* index);
LitThisExpression* lit_create_this_expression(LitState* state, size_t line);

LitSuperExpression* lit_create_super_expression(LitState* state, size_t line, LitString* method, bool ignore_result);
LitRangeExpression* lit_create_range_expression(LitState* state, size_t line, LitExpression* from, LitExpression* to);
LitIfExpression*
lit_create_if_experssion(LitState* state, size_t line, LitExpression* condition, LitExpression* if_branch, LitExpression* else_branch);

LitInterpolationExpression* lit_create_interpolation_expression(LitState* state, size_t line);
LitReferenceExpression* lit_create_reference_expression(LitState* state, size_t line, LitExpression* to);
void lit_free_statement(LitState* state, LitStatement* statement);
LitExpressionStatement* lit_create_expression_statement(LitState* state, size_t line, LitExpression* expression);
LitBlockStatement* lit_create_block_statement(LitState* state, size_t line);
LitVarStatement* lit_create_var_statement(LitState* state, size_t line, const char* name, size_t length, LitExpression* init, bool constant);

LitIfStatement* lit_create_if_statement(LitState* state,
                                        size_t line,
                                        LitExpression* condition,
                                        LitStatement* if_branch,
                                        LitStatement* else_branch,
                                        LitExpressions* elseif_conditions,
                                        LitStatements* elseif_branches);

LitWhileStatement* lit_create_while_statement(LitState* state, size_t line, LitExpression* condition, LitStatement* body);

LitForStatement* lit_create_for_statement(LitState* state,
                                          size_t line,
                                          LitExpression* init,
                                          LitStatement* var,
                                          LitExpression* condition,
                                          LitExpression* increment,
                                          LitStatement* body,
                                          bool c_style);
LitContinueStatement* lit_create_continue_statement(LitState* state, size_t line);
LitBreakStatement* lit_create_break_statement(LitState* state, size_t line);
LitFunctionStatement* lit_create_function_statement(LitState* state, size_t line, const char* name, size_t length);
LitReturnStatement* lit_create_return_statement(LitState* state, size_t line, LitExpression* expression);
LitMethodStatement* lit_create_method_statement(LitState* state, size_t line, LitString* name, bool is_static);
LitClassStatement* lit_create_class_statement(LitState* state, size_t line, LitString* name, LitString* parent);
LitFieldStatement*
lit_create_field_statement(LitState* state, size_t line, LitString* name, LitStatement* getter, LitStatement* setter, bool is_static);

LitExpressions* lit_allocate_expressions(LitState* state);
void lit_free_allocated_expressions(LitState* state, LitExpressions* expressions);

LitStatements* lit_allocate_statements(LitState* state);
void lit_free_allocated_statements(LitState* state, LitStatements* statements);
void lit_init_emitter(LitState* state, LitEmitter* emitter);
void lit_free_emitter(LitEmitter* emitter);

LitModule* lit_emit(LitEmitter* emitter, LitStatements* statements, LitString* module_name);

void lit_init_parser(LitState* state, LitParser* parser);
void lit_free_parser(LitParser* parser);

bool lit_parse(LitParser* parser, const char* file_name, const char* source, LitStatements* statements);
char* lit_read_file(const char* path);
bool lit_file_exists(const char* path);
bool lit_dir_exists(const char* path);

void lit_write_uint8_t(FILE* file, uint8_t byte);
void lit_write_uint16_t(FILE* file, uint16_t byte);
void lit_write_uint32_t(FILE* file, uint32_t byte);
void lit_write_double(FILE* file, double byte);
void lit_write_string(FILE* file, LitString* string);

uint8_t lit_read_uint8_t(FILE* file);
uint16_t lit_read_uint16_t(FILE* file);
uint32_t lit_read_uint32_t(FILE* file);
double lit_read_double(FILE* file);
LitString* lit_read_string(LitState* state, FILE* file);

void lit_init_emulated_file(LitEmulatedFile* file, const char* source);

uint8_t lit_read_euint8_t(LitEmulatedFile* file);
uint16_t lit_read_euint16_t(LitEmulatedFile* file);
uint32_t lit_read_euint32_t(LitEmulatedFile* file);
double lit_read_edouble(LitEmulatedFile* file);
LitString* lit_read_estring(LitState* state, LitEmulatedFile* file);

void lit_save_module(LitModule* module, FILE* file);
LitModule* lit_load_module(LitState* state, const char* input);
bool lit_generate_source_file(const char* file, const char* output);
void lit_build_native_runner(const char* bytecode_file);

void lit_open_libraries(LitState* state);
void lit_open_core_library(LitState* state);

void lit_open_math_library(LitState* state);

void lit_open_file_library(LitState* state);

void lit_open_gc_library(LitState* state);


int lit_decode_num_bytes(uint8_t byte);
int lit_ustring_length(LitString* string);
int lit_encode_num_bytes(int value);
int lit_ustring_decode(const uint8_t* bytes, uint32_t length);
int lit_ustring_encode(int value, uint8_t* bytes);

LitString* lit_ustring_code_point_at(LitState* state, LitString* string, uint32_t index);
LitString* lit_ustring_from_code_point(LitState* state, int value);
LitString* lit_ustring_from_range(LitState* state, LitString* source, int start, uint32_t count);

int lit_uchar_offset(char* str, int index);

static inline bool lit_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool lit_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}


LitString* lit_vformat_error(LitState* state, size_t line, LitError error, va_list args);
LitString* lit_format_error(LitState* state, size_t line, LitError error, ...);

void lit_init_scanner(LitState* state, LitScanner* scanner, const char* file_name, const char* source);
LitToken lit_scan_token(LitScanner* scanner);
void lit_init_optimizer(LitState* state, LitOptimizer* optimizer);
void lit_optimize(LitOptimizer* optimizer, LitStatements* statements);
const char* lit_get_optimization_level_description(LitOptimizationLevel level);

bool lit_is_optimization_enabled(LitOptimization optimization);
void lit_set_optimization_enabled(LitOptimization optimization, bool enabled);
void lit_set_all_optimization_enabled(bool enabled);
void lit_set_optimization_level(LitOptimizationLevel level);

const char* lit_get_optimization_name(LitOptimization optimization);
const char* lit_get_optimization_description(LitOptimization optimization);
void lit_init_preprocessor(LitState* state, LitPreprocessor* preprocessor);
void lit_free_preprocessor(LitPreprocessor* preprocessor);
void lit_add_definition(LitState* state, const char* name);

bool lit_preprocess(LitPreprocessor* preprocessor, char* source);
