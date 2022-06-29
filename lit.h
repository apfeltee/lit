
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


#define BOOL_VALUE(boolean) \
    ((boolean) ? TRUE_VALUE : FALSE_VALUE)

#define FALSE_VALUE ((LitValue)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VALUE ((LitValue)(uint64_t)(QNAN | TAG_TRUE))
#define NULL_VALUE ((LitValue)(uint64_t)(QNAN | TAG_NULL))

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

#define OBJECT_TYPE(value) \
    (AS_OBJECT(value)->type)

#define IS_BOOL(v) \
    (((v)&FALSE_VALUE) == FALSE_VALUE)

#define IS_NULL(v) \
    ((v) == NULL_VALUE)

#define IS_NUMBER(v) \
    (((v)&QNAN) != QNAN)

#define IS_OBJECT(v) \
    (((v) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define IS_OBJECTS_TYPE(value, t) \
    (IS_OBJECT(value) && (AS_OBJECT(value) != NULL) && (AS_OBJECT(value)->type == t))

#define IS_STRING(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_STRING)

#define IS_FUNCTION(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_FUNCTION)

#define IS_NATIVE_FUNCTION(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_NATIVE_FUNCTION)

#define IS_NATIVE_PRIMITIVE(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_NATIVE_PRIMITIVE)

#define IS_NATIVE_METHOD(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_NATIVE_METHOD)

#define IS_PRIMITIVE_METHOD(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_PRIMITIVE_METHOD)

#define IS_MODULE(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_MODULE)

#define IS_CLOSURE(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_CLOSURE)

#define IS_UPVALUE(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_UPVALUE)

#define IS_CLASS(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_CLASS)

#define IS_INSTANCE(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_INSTANCE)

#define IS_ARRAY(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_ARRAY)

#define IS_MAP(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_MAP)

#define IS_BOUND_METHOD(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_BOUND_METHOD)

#define IS_USERDATA(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_USERDATA)

#define IS_RANGE(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_RANGE)

#define IS_FIELD(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_FIELD)

#define IS_REFERENCE(value) \
    IS_OBJECTS_TYPE(value, LITTYPE_REFERENCE)

#define IS_CALLABLE_FUNCTION(value) \
    lit_is_callable_function(value)

#define AS_BOOL(v) ((v) == TRUE_VALUE)
#define AS_OBJECT(v) ((LitObject*)(uintptr_t)((v) & ~(SIGN_BIT | QNAN)))
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

#define INTERPRET_RUNTIME_FAIL ((LitInterpretResult){ LITRESULT_INVALID, NULL_VALUE })

#define RETURN_RUNTIME_ERROR() return (LitInterpretResult){ LITRESULT_RUNTIME_ERROR, NULL_VALUE };
#define RETURN_OK(r) return (LitInterpretResult){ LITRESULT_OK, r };

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
        LIT_INHERIT_CLASS(state->objectvalue_class);             \
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

#define LIT_CHECK_STRING(vm, args, argc, id) lit_check_string(vm, args, argc, id)
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
        lit_runtime_error(vm, "expected %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                       \
    }

#define LIT_ENSURE_MIN_ARGS(count)                                                       \
    if(argc < count)                                                                \
    {                                                                                    \
        lit_runtime_error(vm, "expected minimum %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                               \
    }

#define LIT_ENSURE_MAX_ARGS(count)                                                       \
    if(argc > count)                                                                \
    {                                                                                    \
        lit_runtime_error(vm, "expected maximum %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                               \
    }


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
    LITEXPR_LITERAL,
    LITEXPR_BINARY,
    LITEXPR_UNARY,
    LITEXPR_VAR,
    LITEXPR_ASSIGN,
    LITEXPR_CALL,
    LITEXPR_SET,
    LITEXPR_GET,
    LITEXPR_LAMBDA,
    LITEXPR_ARRAY,
    LITEXPR_OBJECT,
    LITEXPR_SUBSCRIPT,
    LITEXPR_THIS,
    LITEXPR_SUPER,
    LITEXPR_RANGE,
    LITEXPR_IF,
    LITEXPR_INTERPOLATION,
    LITEXPR_REFERENCE
};

enum LitOptimizationLevel
{
    LITOPTLEVEL_NONE,
    LITOPTLEVEL_REPL,
    LITOPTLEVEL_DEBUG,
    LITOPTLEVEL_RELEASE,
    LITOPTLEVEL_EXTREME,

    LITOPTLEVEL_TOTAL
};

enum LitOptimization
{
    LITOPTSTATE_CONSTANT_FOLDING,
    LITOPTSTATE_LITERAL_FOLDING,
    LITOPTSTATE_UNUSED_VAR,
    LITOPTSTATE_UNREACHABLE_CODE,
    LITOPTSTATE_EMPTY_BODY,
    LITOPTSTATE_LINE_INFO,
    LITOPTSTATE_PRIVATE_NAMES,
    LITOPTSTATE_C_FOR,

    LITOPTSTATE_TOTAL
};

enum LitError
{
    // Preprocessor errors
    LITERROR_UNCLOSED_MACRO,
    LITERROR_UNKNOWN_MACRO,

    // Scanner errors
    LITERROR_UNEXPECTED_CHAR,
    LITERROR_UNTERMINATED_STRING,
    LITERROR_INVALID_ESCAPE_CHAR,
    LITERROR_INTERPOLATION_NESTING_TOO_DEEP,
    LITERROR_NUMBER_IS_TOO_BIG,
    LITERROR_CHAR_EXPECTATION_UNMET,

    // Parser errors
    LITERROR_EXPECTATION_UNMET,
    LITERROR_INVALID_ASSIGMENT_TARGET,
    LITERROR_TOO_MANY_FUNCTION_ARGS,
    LITERROR_MULTIPLE_ELSE_BRANCHES,
    LITERROR_VAR_MISSING_IN_FORIN,
    LITERROR_NO_GETTER_AND_SETTER,
    LITERROR_STATIC_OPERATOR,
    LITERROR_SELF_INHERITED_CLASS,
    LITERROR_STATIC_FIELDS_AFTER_METHODS,
    LITERROR_MISSING_STATEMENT,
    LITERROR_EXPECTED_EXPRESSION,
    LITERROR_DEFAULT_ARG_CENTRED,

    // Emitter errors
    LITERROR_TOO_MANY_CONSTANTS,
    LITERROR_TOO_MANY_PRIVATES,
    LITERROR_VAR_REDEFINED,
    LITERROR_TOO_MANY_LOCALS,
    LITERROR_TOO_MANY_UPVALUES,
    LITERROR_VARIABLE_USED_IN_INIT,
    LITERROR_JUMP_TOO_BIG,
    LITERROR_NO_SUPER,
    LITERROR_THIS_MISSUSE,
    LITERROR_SUPER_MISSUSE,
    LITERROR_UNKNOWN_EXPRESSION,
    LITERROR_UNKNOWN_STATEMENT,
    LITERROR_LOOP_JUMP_MISSUSE,
    LITERROR_RETURN_FROM_CONSTRUCTOR,
    LITERROR_STATIC_CONSTRUCTOR,
    LITERROR_CONSTANT_MODIFIED,
    LITERROR_INVALID_REFERENCE_TARGET,

    LITERROR_TOTAL
};

enum LitPrecedence
{
    LITPREC_NONE,
    LITPREC_ASSIGNMENT,// =
    LITPREC_OR,// ||
    LITPREC_AND,// &&
    LITPREC_BOR,// | ^
    LITPREC_BAND,// &
    LITPREC_SHIFT,// << >>
    LITPREC_EQUALITY,// == !=
    LITPREC_COMPARISON,// < > <= >=
    LITPREC_COMPOUND,// += -= *= /= ++ --
    LITPREC_TERM,// + -
    LITPREC_FACTOR,// * /
    LITPREC_IS,// is
    LITPREC_RANGE,// ..
    LITPREC_UNARY,// ! - ~
    LITPREC_NULL,// ??
    LITPREC_CALL,// . ()
    LITPREC_PRIMARY
};

enum LitStatementType
{
    LITSTMT_EXPRESSION,
    LITSTMT_BLOCK,
    LITSTMT_IF,
    LITSTMT_WHILE,
    LITSTMT_FOR,
    LITSTMT_VAR,
    LITSTMT_CONTINUE,
    LITSTMT_BREAK,
    LITSTMT_FUNCTION,
    LITSTMT_RETURN,
    LITSTMT_METHOD,
    LITSTMT_CLASS,
    LITSTMT_FIELD
};

enum LitTokenType
{
    LITTOK_NEW_LINE,

    // Single-character tokens.
    LITTOK_LEFT_PAREN,
    LITTOK_RIGHT_PAREN,
    LITTOK_LEFT_BRACE,
    LITTOK_RIGHT_BRACE,
    LITTOK_LEFT_BRACKET,
    LITTOK_RIGHT_BRACKET,
    LITTOK_COMMA,
    LITTOK_SEMICOLON,
    LITTOK_COLON,

    // One or two character tokens.
    LITTOK_BAR_EQUAL,
    LITTOK_BAR,
    LITTOK_BAR_BAR,
    LITTOK_AMPERSAND_EQUAL,
    LITTOK_AMPERSAND,
    LITTOK_AMPERSAND_AMPERSAND,
    LITTOK_BANG,
    LITTOK_BANG_EQUAL,
    LITTOK_EQUAL,
    LITTOK_EQUAL_EQUAL,
    LITTOK_GREATER,
    LITTOK_GREATER_EQUAL,
    LITTOK_GREATER_GREATER,
    LITTOK_LESS,
    LITTOK_LESS_EQUAL,
    LITTOK_LESS_LESS,
    LITTOK_PLUS,
    LITTOK_PLUS_EQUAL,
    LITTOK_PLUS_PLUS,
    LITTOK_MINUS,
    LITTOK_MINUS_EQUAL,
    LITTOK_MINUS_MINUS,
    LITTOK_STAR,
    LITTOK_STAR_EQUAL,
    LITTOK_STAR_STAR,
    LITTOK_SLASH,
    LITTOK_SLASH_EQUAL,
    LITTOK_QUESTION,
    LITTOK_QUESTION_QUESTION,
    LITTOK_PERCENT,
    LITTOK_PERCENT_EQUAL,
    LITTOK_ARROW,
    LITTOK_SMALL_ARROW,
    LITTOK_TILDE,
    LITTOK_CARET,
    LITTOK_CARET_EQUAL,
    LITTOK_DOT,
    LITTOK_DOT_DOT,
    LITTOK_DOT_DOT_DOT,
    LITTOK_SHARP,
    LITTOK_SHARP_EQUAL,

    // Literals.
    LITTOK_IDENTIFIER,
    LITTOK_STRING,
    LITTOK_INTERPOLATION,
    LITTOK_NUMBER,

    // Keywords.
    LITTOK_CLASS,
    LITTOK_ELSE,
    LITTOK_FALSE,
    LITTOK_FOR,
    LITTOK_FUNCTION,
    LITTOK_IF,
    LITTOK_NULL,
    LITTOK_RETURN,
    LITTOK_SUPER,
    LITTOK_THIS,
    LITTOK_TRUE,
    LITTOK_VAR,
    LITTOK_WHILE,
    LITTOK_CONTINUE,
    LITTOK_BREAK,
    LITTOK_NEW,
    LITTOK_EXPORT,
    LITTOK_IS,
    LITTOK_STATIC,
    LITTOK_OPERATOR,
    LITTOK_GET,
    LITTOK_SET,
    LITTOK_IN,
    LITTOK_CONST,
    LITTOK_REF,

    LITTOK_ERROR,
    LITTOK_EOF
};

enum LitInterpretResultType
{
    LITRESULT_OK,
    LITRESULT_COMPILE_ERROR,
    LITRESULT_RUNTIME_ERROR,
    LITRESULT_INVALID
};

enum LitErrorType
{
    COMPILE_ERROR,
    RUNTIME_ERROR
};

enum LitFunctionType
{
    LITFUNC_REGULAR,
    LITFUNC_SCRIPT,
    LITFUNC_METHOD,
    LITFUNC_STATIC_METHOD,
    LITFUNC_CONSTRUCTOR
};

enum LitObjectType
{
    LITTYPE_STRING,
    LITTYPE_FUNCTION,
    LITTYPE_NATIVE_FUNCTION,
    LITTYPE_NATIVE_PRIMITIVE,
    LITTYPE_NATIVE_METHOD,
    LITTYPE_PRIMITIVE_METHOD,
    LITTYPE_FIBER,
    LITTYPE_MODULE,
    LITTYPE_CLOSURE,
    LITTYPE_UPVALUE,
    LITTYPE_CLASS,
    LITTYPE_INSTANCE,
    LITTYPE_BOUND_METHOD,
    LITTYPE_ARRAY,
    LITTYPE_MAP,
    LITTYPE_USERDATA,
    LITTYPE_RANGE,
    LITTYPE_FIELD,
    LITTYPE_REFERENCE
};

typedef enum /**/LitOpCode LitOpCode;
typedef enum /**/LitExpressionType LitExpressionType;
typedef enum /**/LitOptimizationLevel LitOptimizationLevel;
typedef enum /**/LitOptimization LitOptimization;
typedef enum /**/LitError LitError;
typedef enum /**/LitPrecedence LitPrecedence;
typedef enum /**/LitStatementType LitStatementType;
typedef enum /**/LitTokenType LitTokenType;
typedef enum /**/LitInterpretResultType LitInterpretResultType;
typedef enum /**/LitErrorType LitErrorType;
typedef enum /**/LitFunctionType LitFunctionType;
typedef enum /**/LitObjectType LitObjectType;
typedef struct /**/LitScanner LitScanner;
typedef struct /**/LitPreprocessor LitPreprocessor;
typedef struct /**/LitVM LitVM;
typedef struct /**/LitParser LitParser;
typedef struct /**/LitEmitter LitEmitter;
typedef struct /**/LitOptimizer LitOptimizer;
typedef struct /**/LitState LitState;
typedef struct /**/LitInterpretResult LitInterpretResult;
typedef struct /**/LitObject LitObject;
typedef struct /**/LitMap LitMap;
typedef struct /**/LitString LitString;
typedef struct /**/LitModule LitModule;
typedef struct /**/LitFiber LitFiber;
typedef struct /**/LitUserdata LitUserdata;
typedef struct /**/LitChunk LitChunk;
typedef struct /**/LitTableEntry LitTableEntry;
typedef struct /**/LitTable LitTable;
typedef struct /**/LitFunction LitFunction;
typedef struct /**/LitUpvalue LitUpvalue;
typedef struct /**/LitClosure LitClosure;
typedef struct /**/LitNativeFunction LitNativeFunction;
typedef struct /**/LitNativePrimitive LitNativePrimitive;
typedef struct /**/LitNativeMethod LitNativeMethod;
typedef struct /**/LitPrimitiveMethod LitPrimitiveMethod;
typedef struct /**/LitCallFrame LitCallFrame;
typedef struct /**/LitClass LitClass;
typedef struct /**/LitInstance LitInstance;
typedef struct /**/LitBoundMethod LitBoundMethod;
typedef struct /**/LitArray LitArray;
typedef struct /**/LitRange LitRange;
typedef struct /**/LitField LitField;
typedef struct /**/LitReference LitReference;
typedef struct /**/LitToken LitToken;
typedef struct /**/LitExpression LitExpression;
typedef struct /**/LitStatement LitStatement;
typedef struct /**/LitLiteralExpression LitLiteralExpression;
typedef struct /**/LitBinaryExpression LitBinaryExpression;
typedef struct /**/LitUnaryExpression LitUnaryExpression;
typedef struct /**/LitVarExpression LitVarExpression;
typedef struct /**/LitAssignExpression LitAssignExpression;
typedef struct /**/LitCallExpression LitCallExpression;
typedef struct /**/LitGetExpression LitGetExpression;
typedef struct /**/LitSetExpression LitSetExpression;
typedef struct /**/LitParameter LitParameter;
typedef struct /**/LitLambdaExpression LitLambdaExpression;
typedef struct /**/LitArrayExpression LitArrayExpression;
typedef struct /**/LitObjectExpression LitObjectExpression;
typedef struct /**/LitSubscriptExpression LitSubscriptExpression;
typedef struct /**/LitThisExpression LitThisExpression;
typedef struct /**/LitSuperExpression LitSuperExpression;
typedef struct /**/LitRangeExpression LitRangeExpression;
typedef struct /**/LitIfExpression LitIfExpression;
typedef struct /**/LitInterpolationExpression LitInterpolationExpression;
typedef struct /**/LitReferenceExpression LitReferenceExpression;
typedef struct /**/LitExpressionStatement LitExpressionStatement;
typedef struct /**/LitBlockStatement LitBlockStatement;
typedef struct /**/LitVarStatement LitVarStatement;
typedef struct /**/LitIfStatement LitIfStatement;
typedef struct /**/LitWhileStatement LitWhileStatement;
typedef struct /**/LitForStatement LitForStatement;
typedef struct /**/LitContinueStatement LitContinueStatement;
typedef struct /**/LitBreakStatement LitBreakStatement;
typedef struct /**/LitFunctionStatement LitFunctionStatement;
typedef struct /**/LitReturnStatement LitReturnStatement;
typedef struct /**/LitMethodStatement LitMethodStatement;
typedef struct /**/LitClassStatement LitClassStatement;
typedef struct /**/LitFieldStatement LitFieldStatement;
typedef struct /**/LitPrivate LitPrivate;
typedef struct /**/LitLocal LitLocal;
typedef struct /**/LitCompilerUpvalue LitCompilerUpvalue;
typedef struct /**/LitCompiler LitCompiler;
typedef struct /**/LitParseRule LitParseRule;
typedef struct /**/LitEmulatedFile LitEmulatedFile;
typedef struct /**/LitVariable LitVariable;

/* ARRAYTYPES */
typedef struct /**/LitVariables LitVariables;
typedef struct /**/LitBytes LitBytes;
typedef struct /**/LitUInts LitUInts;
typedef struct /**/LitValueList LitValueList;
typedef struct /**/LitExprList LitExprList;
typedef struct /**/LitParameters LitParameters;
typedef struct /**/LitStmtList LitStmtList;
typedef struct /**/LitPrivates LitPrivates;
typedef struct /**/LitLocals LitLocals;

typedef LitValue (*LitNativeFunctionFn)(LitVM* vm, size_t arg_count, LitValue* args);
typedef bool (*LitNativePrimitiveFn)(LitVM* vm, size_t arg_count, LitValue* args);
typedef LitValue (*LitNativeMethodFn)(LitVM* vm, LitValue instance, size_t arg_count, LitValue* args);
typedef bool (*LitPrimitiveMethodFn)(LitVM* vm, LitValue instance, size_t arg_count, LitValue* args);
typedef LitValue (*LitMapIndexFn)(LitVM* vm, LitMap* map, LitString* index, LitValue* value);
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

struct LitValueList
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
    LitValueList constants;
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
    LitValueList values;
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
    LitVM* vm;
    bool had_error;
    LitFunction* api_function;
    LitFiber* api_fiber;
    LitString* api_name;
    // class class
    // Mental note:
    // When adding another class here, DO NOT forget to mark it in lit_mem.c or it will be GC-ed
    LitClass* classvalue_class;
    LitClass* objectvalue_class;
    LitClass* numbervalue_class;
    LitClass* stringvalue_class;
    LitClass* boolvalue_class;
    LitClass* functionvalue_class;
    LitClass* fibervalue_class;
    LitClass* modulevalue_class;
    LitClass* arrayvalue_class;
    LitClass* mapvalue_class;
    LitClass* rangevalue_class;
    LitModule* last_module;
};

struct LitVM
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
struct LitExprList
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
    LitExprList args;
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
    LitExprList values;
};

struct LitObjectExpression
{
    LitExpression expression;
    LitValueList keys;
    LitExprList values;
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
    LitExprList expressions;
};

struct LitReferenceExpression
{
    LitExpression expression;
    LitExpression* to;
};

/*
 * Statements
 */
struct LitStmtList
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
    LitStmtList statements;
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
    LitExprList* elseif_conditions;
    LitStmtList* elseif_branches;
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
    LitStmtList fields;
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
	 * I'm using LitValueList here, because LitValue
	 * also has the same size (8 bytes)
	 */
    LitValueList open_ifs;
};

void util_custom_quick_sort(LitVM *vm, LitValue *l, int length, LitValue callee);
int util_table_iterator(LitTable *table, int number);
LitValue util_table_iterator_key(LitTable *table, int index);
bool util_is_fiber_done(LitFiber *fiber);
void util_run_fiber(LitVM *vm, LitFiber *fiber, LitValue *argv, size_t argc, bool catcher);

int lit_array_indexof(LitArray *array, LitValue value);

LitValue lit_array_removeat(LitArray *array, size_t index);
void util_basic_quick_sort(LitState *state, LitValue *clist, int length);
bool util_interpret(LitVM *vm, LitModule *module);
bool util_test_file_exists(const char *filename);
bool util_attempt_to_require(LitVM *vm, LitValue *argv, size_t argc, const char *path, bool ignore_previous, bool folders);
bool util_attempt_to_require_combined(LitVM *vm, LitValue *argv, size_t argc, const char *a, const char *b, bool ignore_previous);
LitValue util_invalid_constructor(LitVM *vm, LitValue instance, size_t argc, LitValue *argv);


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
    return (IS_BOOL(value) && value == FALSE_VALUE) || IS_NULL(value) || (IS_NUMBER(value) && lit_value_to_number(value) == 0);
}


void lit_init_uints(LitUInts* array);
void lit_free_uints(LitState* state, LitUInts* array);
void lit_uints_write(LitState* state, LitUInts* array, size_t value);


void lit_init_bytes(LitBytes* array);
void lit_free_bytes(LitState* state, LitBytes* array);
void lit_bytes_write(LitState* state, LitBytes* array, uint8_t value);

void lit_init_values(LitValueList* array);
void lit_free_values(LitState* state, LitValueList* array);
void lit_values_write(LitState* state, LitValueList* array, LitValue value);


void lit_init_expressions(LitExprList* array);
void lit_free_expressions(LitState* state, LitExprList* array);
void lit_expressions_write(LitState* state, LitExprList* array, LitExpression* value);

void lit_init_parameters(LitParameters* array);
void lit_free_parameters(LitState* state, LitParameters* array);
void lit_parameters_write(LitState* state, LitParameters* array, LitParameter value);

void lit_init_statements(LitStmtList* array);
void lit_free_statements(LitState* state, LitStmtList* array);
void lit_statements_write(LitState* state, LitStmtList* array, LitStatement* value);


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
void lit_values_ensure_size(LitState* state, LitValueList* values, size_t size);
const char* lit_get_value_type(LitValue value);
void* lit_reallocate(LitState* state, void* pointer, size_t old_size, size_t new_size);
void lit_free_objects(LitState* state, LitObject* objects);
uint64_t lit_collect_garbage(LitVM* vm);
void lit_mark_object(LitVM* vm, LitObject* object);
void lit_mark_value(LitVM* vm, LitValue value);
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
void lit_mark_table(LitVM* vm, LitTable* table);
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
LitValue lit_get_function_name(LitVM* vm, LitValue instance);
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

void lit_array_push(LitState* state, LitArray* array, LitValue val);


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

void lit_init_vm(LitState* state, LitVM* vm);
void lit_free_vm(LitVM* vm);
void lit_trace_vm_stack(LitVM* vm);

static inline void lit_push(LitVM* vm, LitValue value)
{
    *(vm->fiber->stack_top) = value;
    vm->fiber->stack_top++;

}

static inline LitValue lit_pop(LitVM* vm)
{
    LitValue rt;
    rt = *(vm->fiber->stack_top);
    vm->fiber->stack_top--;
    return rt;
}

LitInterpretResult lit_interpret_module(LitState* state, LitModule* module);
LitInterpretResult lit_interpret_fiber(LitState* state, LitFiber* fiber);
bool lit_handle_runtime_error(LitVM* vm, LitString* error_string);
bool lit_vruntime_error(LitVM* vm, const char* format, va_list args);
bool lit_runtime_error(LitVM* vm, const char* format, ...);
bool lit_runtime_error_exiting(LitVM* vm, const char* format, ...);

void lit_native_exit_jump();
bool lit_set_native_exit_jump();


LitInterpretResult lit_call_function(LitState* state, LitFunction* callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult lit_call_method(LitState* state, LitValue instance, LitValue callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult lit_call(LitState* state, LitValue callee, LitValue* arguments, uint8_t argument_count);
LitInterpretResult lit_find_and_call_method(LitState* state, LitValue callee, LitString* method_name, LitValue* arguments, uint8_t argument_count);

LitString* lit_to_string(LitState* state, LitValue object);
LitValue lit_call_new(LitVM* vm, const char* name, LitValue* args, size_t arg_count);


void lit_init_api(LitState* state);
void lit_free_api(LitState* state);

LitValue lit_get_global(LitState* state, LitString* name);
LitFunction* lit_get_global_function(LitState* state, LitString* name);

void lit_set_global(LitState* state, LitString* name, LitValue value);
bool lit_global_exists(LitState* state, LitString* name);
void lit_define_native(LitState* state, const char* name, LitNativeFunctionFn native);
void lit_define_native_primitive(LitState* state, const char* name, LitNativePrimitiveFn native);

double lit_check_number(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
double lit_get_number(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, double def);

bool lit_check_bool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
bool lit_get_bool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, bool def);

const char* lit_check_string(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
const char* lit_get_string(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, const char* def);

LitString* lit_check_object_string(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
LitInstance* lit_check_instance(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
LitValue* lit_check_reference(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);

void lit_ensure_bool(LitVM* vm, LitValue value, const char* error);
void lit_ensure_string(LitVM* vm, LitValue value, const char* error);
void lit_ensure_number(LitVM* vm, LitValue value, const char* error);
void lit_ensure_object_type(LitVM* vm, LitValue value, LitObjectType type, const char* error);


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
                                        LitExprList* elseif_conditions,
                                        LitStmtList* elseif_branches);

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

LitExprList* lit_allocate_expressions(LitState* state);
void lit_free_allocated_expressions(LitState* state, LitExprList* expressions);

LitStmtList* lit_allocate_statements(LitState* state);
void lit_free_allocated_statements(LitState* state, LitStmtList* statements);
void lit_init_emitter(LitState* state, LitEmitter* emitter);
void lit_free_emitter(LitEmitter* emitter);

LitModule* lit_emit(LitEmitter* emitter, LitStmtList* statements, LitString* module_name);

void lit_init_parser(LitState* state, LitParser* parser);
void lit_free_parser(LitParser* parser);

bool lit_parse(LitParser* parser, const char* file_name, const char* source, LitStmtList* statements);
char* lit_read_file(const char* path);
bool lit_file_exists(const char* path);
bool lit_dir_exists(const char* path);

size_t lit_write_uint8_t(FILE* file, uint8_t byte);
size_t lit_write_uint16_t(FILE* file, uint16_t byte);
size_t lit_write_uint32_t(FILE* file, uint32_t byte);
size_t lit_write_double(FILE* file, double byte);
size_t lit_write_string(FILE* file, LitString* string);

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
void lit_optimize(LitOptimizer* optimizer, LitStmtList* statements);
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
