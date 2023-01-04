
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

#define UNREACHABLE assert(false);
#define UINT8_COUNT UINT8_MAX + 1
#define UINT16_COUNT UINT16_MAX + 1

#define TABLE_MAX_LOAD 0.75
// Do not change these, or old bytecode files will break!
#define LIT_BYTECODE_MAGIC_NUMBER 6932
#define LIT_BYTECODE_END_NUMBER 2942
#define LIT_STRING_KEY 48

#define SIGN_BIT ((uint64_t)1 << 63u)
#define QNAN ((uint64_t)0x7ffc000000000000u)

#define TAG_NULL 1u
#define TAG_FALSE 2u
#define TAG_TRUE 3u

#define LIT_TESTS_DIRECTORY "tests"

#define FALSE_VALUE ((LitValue)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VALUE ((LitValue)(uint64_t)(QNAN | TAG_TRUE))
#define NULL_VALUE ((LitValue)(uint64_t)(QNAN | TAG_NULL))

#define LIT_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity)*2)

#define LIT_GROW_ARRAY(state, previous, typesz, old_count, count) \
    lit_gcmem_memrealloc(state, previous, typesz * (old_count), typesz * (count))

#define LIT_FREE_ARRAY(state, typesz, pointer, old_count) \
    lit_gcmem_memrealloc(state, pointer, typesz * (old_count), 0)

#define LIT_ALLOCATE(state, typesz, count) \
    lit_gcmem_memrealloc(state, NULL, 0, typesz * (count))

#define LIT_FREE(state, typesz, pointer) \
    lit_gcmem_memrealloc(state, pointer, typesz, 0)


#define INTERPRET_RUNTIME_FAIL ((LitInterpretResult){ LITRESULT_INVALID, NULL_VALUE })

#define LIT_GET_FIELD(id) lit_state_getfield(vm->state, &lit_value_asinstance(instance)->fields, id)
#define LIT_GET_MAP_FIELD(id) lit_state_getmapfield(vm->state, &lit_value_asinstance(instance)->fields, id)
#define LIT_SET_FIELD(id, value) lit_state_setfield(vm->state, &lit_value_asinstance(instance)->fields, id, value)
#define LIT_SET_MAP_FIELD(id, value) lit_state_setmapfield(vm->state, &lit_value_asinstance(instance)->fields, id, value)

#define LIT_ENSURE_ARGS(count)                                                   \
    if(argc != count)                                                       \
    {                                                                            \
        lit_vm_raiseerror(vm, "expected %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                       \
    }

#define LIT_ENSURE_MIN_ARGS(count)                                                       \
    if(argc < count)                                                                \
    {                                                                                    \
        lit_vm_raiseerror(vm, "expected minimum %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                               \
    }

#define LIT_ENSURE_MAX_ARGS(count)                                                       \
    if(argc > count)                                                                \
    {                                                                                    \
        lit_vm_raiseerror(vm, "expected maximum %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                               \
    }

enum LitExprType
{
    LITEXPR_LITERAL,
    LITEXPR_BINARY,
    LITEXPR_UNARY,
    LITEXPR_VAREXPR,
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
    LITEXPR_TERNARY,
    LITEXPR_INTERPOLATION,
    LITEXPR_REFERENCE,

    LITEXPR_EXPRESSION,
    LITEXPR_BLOCK,
    LITEXPR_IFSTMT,
    LITEXPR_WHILE,
    LITEXPR_FOR,
    LITEXPR_VARSTMT,
    LITEXPR_CONTINUE,
    LITEXPR_BREAK,
    LITEXPR_FUNCTION,
    LITEXPR_RETURN,
    LITEXPR_METHOD,
    LITEXPR_CLASS,
    LITEXPR_FIELD
};

enum LitOptLevel
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

enum LitTokType
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

enum LitResult
{
    LITRESULT_OK,
    LITRESULT_COMPILE_ERROR,
    LITRESULT_RUNTIME_ERROR,
    LITRESULT_INVALID
};

enum LitErrType
{
    COMPILE_ERROR,
    RUNTIME_ERROR
};

enum LitFuncType
{
    LITFUNC_REGULAR,
    LITFUNC_SCRIPT,
    LITFUNC_METHOD,
    LITFUNC_STATIC_METHOD,
    LITFUNC_CONSTRUCTOR
};

enum LitObjType
{
    LITTYPE_UNDEFINED,
    LITTYPE_NULL,
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
    LITTYPE_REFERENCE,
    LITTYPE_NUMBER,
};

typedef uint64_t LitValue;
typedef enum /**/LitExprType LitExprType;
typedef enum /**/LitOptLevel LitOptLevel;
typedef enum /**/LitOptimization LitOptimization;
typedef enum /**/LitError LitError;
typedef enum /**/LitPrecedence LitPrecedence;
typedef enum /**/LitTokType LitTokType;
typedef enum /**/LitResult LitResult;
typedef enum /**/LitErrType LitErrType;
typedef enum /**/LitFuncType LitFuncType;
typedef enum /**/LitObjType LitObjType;
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
typedef struct /**/LitNumber LitNumber;
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
typedef struct /**/LitNativePrimFunction LitNativePrimFunction;
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
typedef struct /**/LitCompilerUpvalue LitCompilerUpvalue;
typedef struct /**/LitCompiler LitCompiler;
typedef struct /**/LitParseRule LitParseRule;
typedef struct /**/LitEmulatedFile LitEmulatedFile;
typedef struct /**/LitVariable LitVariable;
typedef struct /**/LitWriter LitWriter;
typedef struct /**/LitLocal LitLocal;
typedef struct /**/LitConfig LitConfig;

/* ARRAYTYPES */
typedef struct /**/LitVarList LitVarList;
typedef struct /**/LitUintList LitUintList;
typedef struct /**/LitValueList LitValueList;
typedef struct /**/LitExprList LitExprList;
typedef struct /**/LitParamList LitParamList;
typedef struct /**/LitPrivList LitPrivList;
typedef struct /**/LitLocList LitLocList;
typedef struct /**/LitDataList LitDataList;


typedef LitValue (*LitNativeFunctionFn)(LitVM*, size_t, LitValue*);
typedef bool (*LitNativePrimitiveFn)(LitVM*, size_t, LitValue*);
typedef LitValue (*LitNativeMethodFn)(LitVM*, LitValue, size_t arg_count, LitValue*);
typedef bool (*LitPrimitiveMethodFn)(LitVM*, LitValue, size_t, LitValue*);
typedef LitValue (*LitMapIndexFn)(LitVM*, LitMap*, LitString*, LitValue*);
typedef void (*LitCleanupFn)(LitState*, LitUserdata*, bool mark);
typedef void (*LitErrorFn)(LitState*, const char*);
typedef void (*LitPrintFn)(LitState*, const char*);

typedef void(*LitWriterByteFN)(LitWriter*, int);
typedef void(*LitWriterStringFN)(LitWriter*, const char*, size_t);
typedef void(*LitWriterFormatFN)(LitWriter*, const char*, va_list);

struct LitDataList
{
    /* how many values *could* this list hold? */
    size_t capacity;

    /* actual amount of values in this list */
    size_t count;

    size_t rawelemsz;
    size_t elemsz;

    /* the actual values */
    intptr_t* values;
};

struct LitWriter
{
    LitState* state;

    /* the main pointer, that either holds a pointer to a LitString, or a FILE */
    void* uptr;

    /* if true, then uptr is a LitString, otherwise it's a FILE */
    bool stringmode;

    /* if true, and !stringmode, then calls fflush() after each i/o operations */
    bool forceflush;

    /* the callback that emits a single character */
    LitWriterByteFN fnbyte;

    /* the callback that emits a string */
    LitWriterStringFN fnstring;

    /* the callback that emits a format string (printf-style) */
    LitWriterFormatFN fnformat;
};


struct LitVariable
{
    const char* name;
    size_t length;
    int depth;
    bool constant;
    bool used;
    LitValue constant_value;
    LitExpression** declaration;
};

struct LitVarList
{
    size_t capacity;
    size_t count;
    LitVariable* values;
};

struct LitExpression
{
    LitExprType type;
    size_t line;
};

struct LitUintList
{
    LitDataList list;
};

struct LitValueList
{
    LitDataList list;
};

struct LitChunk
{
    /* how many items this chunk holds */
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
    /* the key of this entry. can be NULL! */
    LitString* key;

    /* the associated value */
    LitValue value;
};

struct LitTable
{
    LitState* state;

    /* how many entries are in this table */
    int count;

    /* how many entries could be held */
    int capacity;

    /* the actual entries */
    LitTableEntry* entries;
};

struct LitObject
{
    /* the type of this object */
    LitObjType type;
    LitObject* next;
    bool marked;
    bool mustfree;
};

struct LitNumber
{
    LitObject object;
    double num;
};

struct LitString
{
    LitObject object;
    /* the hash of this string - note that it is only unique to the context! */
    uint32_t hash;
    /* this is handled by sds - use lit_string_getlength to get the length! */
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
    /* the native callback for this function */
    LitNativeFunctionFn function;
    /* the name of this function */
    LitString* name;
};

struct LitNativePrimFunction
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
    /* the table that holds the actual entries */
    LitTable values;
    /* the index function corresponding to operator[] */
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
    LitValue lit_emitter_raiseerror;
    bool abort;
    bool catcher;
};

struct LitClass
{
    LitObject object;
    /* the name of this class */
    LitString* name;
    /* the constructor object */
    LitObject* init_method;
    /* runtime methods */
    LitTable methods;
    /* static fields, which include functions, and variables */
    LitTable static_fields;
    /*
    * the parent class - the topmost is always LitClass, followed by LitObject.
    * that is, eg for LitString: LitString <- LitObject <- LitClass
    */
    LitClass* super;
};

struct LitInstance
{
    LitObject object;
    /* the class that corresponds to this instance */
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
    LitValueList list;
};

struct LitUserdata
{
    LitObject object;
    void* data;
    size_t size;
    LitCleanupFn cleanup_fn;
    bool canfree;
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

struct LitConfig
{
    bool dumpbytecode;
    bool dumpast;
    bool runafterdump;
};

struct LitState
{
    LitConfig config;
    LitWriter stdoutwriter;
    /* how much was allocated in total? */
    int64_t bytes_allocated;
    int64_t next_gc;
    bool allow_gc;
    LitValueList lightobjects;
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
    /*
    * recursive pointer to the current VM instance.
    * using 'state->vm->state' will in turn mean this instance, etc.
    */
    LitVM* vm;
    bool had_error;
    LitFunction* api_function;
    LitFiber* api_fiber;
    LitString* api_name;
    /* when using debug routines, this is the writer that output is called on */
    LitWriter debugwriter;
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
    /* the current state */
    LitState* state;
    /* currently held objects */
    LitObject* objects;
    /* currently cached strings */
    LitTable strings;
    /* currently loaded/defined modules */
    LitMap* modules;
    /* currently defined globals */
    LitMap* globals;
    LitFiber* fiber;
    // For garbage collection
    size_t gray_count;
    size_t gray_capacity;
    LitObject** gray_stack;
};

struct LitInterpretResult
{
    /* the result of this interpret/lit_vm_callcallable attempt */
    LitResult type;
    /* the value returned from this interpret/lit_vm_callcallable attempt */
    LitValue result;
};

struct LitToken
{
    const char* start;
    LitTokType type;
    size_t length;
    size_t line;
    LitValue value;
};

struct LitLocal
{
    const char* name;
    size_t length;
    int depth;
    bool captured;
    bool constant;
};

struct LitLocList
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
    LitLocList locals;
    int scope_depth;
    LitFunction* function;
    LitFuncType type;
    LitCompilerUpvalue upvalues[UINT8_COUNT];
    LitCompiler* enclosing;
    bool skip_return;
    size_t loop_depth;
    int slots;
    int max_slots;
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
    size_t length;
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

struct LitOptimizer
{
    LitState* state;
    LitVarList variables;
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



LitClass* lit_create_classobject(LitState* state, const char* name);
void lit_class_inheritfrom(LitState* state, LitClass* current, LitClass* other);
void lit_class_bindconstructor(LitState* state, LitClass* cl, LitNativeMethodFn fn);
LitNativeMethod* lit_class_bindmethod(LitState* state, LitClass* cl, const char* name, LitNativeMethodFn fn);
LitPrimitiveMethod* lit_class_bindprimitive(LitState* state, LitClass* cl, const char* name, LitPrimitiveMethodFn fn);
LitNativeMethod* lit_class_bindstaticmethod(LitState* state, LitClass* cl, const char* name, LitNativeMethodFn fn);
LitPrimitiveMethod* lit_class_bindstaticprimitive(LitState* state, LitClass* cl, const char* name, LitPrimitiveMethodFn fn);
void lit_class_setstaticfield(LitState* state, LitClass* cl, const char* name, LitValue val);
LitField* lit_class_bindgetset(LitState* state, LitClass* cl, const char* name, LitNativeMethodFn getfn, LitNativeMethodFn setfn, bool isstatic);


/**
* LitWriter stuff
*/
/*
* creates a LitWriter that writes to the given FILE.
* if $forceflush is true, then fflush() is called after each i/o operation.
*/
void lit_writer_init_file(LitState* state, LitWriter* wr, FILE* fh, bool forceflush);

/*
* creates a LitWriter that writes to a buffer.
*/
void lit_writer_init_string(LitState* state, LitWriter* wr);

/* emit a printf-style formatted string */
void lit_writer_writeformat(LitWriter* wr, const char* fmt, ...);

/* emit a string */
void lit_writer_writestringl(LitWriter* wr, const char* str, size_t len);

/* like lit_writer_writestringl, but calls strlen() on str */
void lit_writer_writestring(LitWriter* wr, const char* str);

/* emit a single byte */
void lit_writer_writebyte(LitWriter* wr, int byte);

/*
* returns a LitString* if this LitWriter was initiated via lit_writer_init_string, otherwise NULL.
*/
LitString* lit_writer_get_string(LitWriter* wr);

/**
* utility functions
*/
double lit_util_uinttofloat(unsigned int val);
unsigned int lit_util_floattouint(double val);
int lit_util_numbertoint32(double n);
int lit_util_doubletoint(double n);
unsigned int lit_util_numbertouint32(double n);
int lit_util_ucharoffset(char* str, int index);
int lit_util_encodenumbytes(int value);
int lit_util_decodenumbytes(uint8_t byte);
char* lit_util_readfile(const char* path, size_t* dlen);
/* get hash sum of given string */
uint32_t lit_util_hashstring(const char* key, size_t length);
int lit_util_closestpowof2(int n);
char* lit_util_copystring(const char* string);
char* lit_util_patchfilename(char* file_name);


void util_custom_quick_sort(LitVM *vm, LitValue *l, int length, LitValue callee);
int util_table_iterator(LitTable *table, int number);
LitValue util_table_iterator_key(LitTable *table, int index);
bool util_is_fiber_done(LitFiber *fiber);
void util_run_fiber(LitVM *vm, LitFiber *fiber, LitValue *argv, size_t argc, bool catcher);
void util_basic_quick_sort(LitState *state, LitValue *clist, int length);
bool util_interpret(LitVM *vm, LitModule *module);
bool util_test_file_exists(const char *filename);
bool util_attempt_to_require(LitVM *vm, LitValue *argv, size_t argc, const char *path, bool ignore_previous, bool folders);
bool util_attempt_to_require_combined(LitVM *vm, LitValue *argv, size_t argc, const char *a, const char *b, bool ignore_previous);
LitValue util_invalid_constructor(LitVM *vm, LitValue instance, size_t argc, LitValue *argv);


/**
* array functions 
*/
/* retrieve the index of $value in this array */
int lit_array_indexof(LitArray *array, LitValue value);

/* remove the value at $index */
LitValue lit_array_removeat(LitArray *array, size_t index);

LitObject* lit_gcmem_allocobject(LitState* state, size_t size, LitObjType type, bool islight);

bool lit_value_compare(LitState* state, const LitValue a, const LitValue b);
#define lit_value_objectvalue(obj) lit_value_objectvalue_actual((uintptr_t)obj)

LitValue lit_value_objectvalue_actual(uintptr_t obj);
LitObject* lit_value_asobject(LitValue v);
LitValue lit_bool_to_value(LitState* state, bool b);

/* turn the given value to a number */
double lit_value_asnumber(LitValue v);

/* turn a number into a value*/
LitValue lit_value_numbertovalue(LitState* state, double num);

bool lit_value_isbool(LitValue v);
bool lit_value_isobject(LitValue v);

#define lit_value_istype(value, t) \
    (lit_value_isobject(value) && (lit_value_asobject(value) != NULL) && (lit_value_asobject(value)->type == t))

#define lit_value_isnull(v) \
    ((v) == NULL_VALUE)

bool lit_value_isnumber(LitValue v);

#define lit_value_isstring(value) \
    lit_value_istype(value, LITTYPE_STRING)

#define lit_value_isfunction(value) \
    lit_value_istype(value, LITTYPE_FUNCTION)

#define lit_value_isnatfunction(value) \
    lit_value_istype(value, LITTYPE_NATIVE_FUNCTION)

#define lit_value_isnatprim(value) \
    lit_value_istype(value, LITTYPE_NATIVE_PRIMITIVE)

#define lit_value_isnatmethod(value) \
    lit_value_istype(value, LITTYPE_NATIVE_METHOD)

#define lit_value_isprimmethod(value) \
    lit_value_istype(value, LITTYPE_PRIMITIVE_METHOD)

#define lit_value_ismodule(value) \
    lit_value_istype(value, LITTYPE_MODULE)

#define lit_value_isclosure(value) \
    lit_value_istype(value, LITTYPE_CLOSURE)

#define lit_value_isupvalue(value) \
    lit_value_istype(value, LITTYPE_UPVALUE)

#define lit_value_isclass(value) \
    lit_value_istype(value, LITTYPE_CLASS)

#define lit_value_isinstance(value) \
    lit_value_istype(value, LITTYPE_INSTANCE)

#define lit_value_isarray(value) \
    lit_value_istype(value, LITTYPE_ARRAY)

static inline bool lit_value_ismap(LitValue value)
{
    return lit_value_istype(value, LITTYPE_MAP);
}

static inline bool lit_value_isboundmethod(LitValue value)
{
    return lit_value_istype(value, LITTYPE_BOUND_METHOD);
}

static inline bool lit_value_isuserdata(LitValue value)
{
    return lit_value_istype(value, LITTYPE_USERDATA);
}

static inline bool lit_value_isrange(LitValue value)
{
    return lit_value_istype(value, LITTYPE_RANGE);
}

static inline bool lit_value_isfield(LitValue value)
{
    return lit_value_istype(value, LITTYPE_FIELD);
}

static inline bool lit_value_isreference(LitValue value)
{
    return lit_value_istype(value, LITTYPE_REFERENCE);
}

/* is this value falsey? */
static inline bool lit_value_isfalsey(LitValue v)
{
    return (lit_value_isbool(v) && (v == FALSE_VALUE)) || lit_value_isnull(v) || (lit_value_isnumber(v) && lit_value_asnumber(v) == 0);
}

LitObjType lit_value_type(LitValue v);

static inline bool lit_value_asbool(LitValue v)
{
    return (v == TRUE_VALUE);
}

static inline LitString* lit_value_asstring(LitValue v)
{
    return (LitString*)lit_value_asobject(v);
}

static inline char* lit_value_ascstring(LitValue v)
{
    return (lit_value_asstring(v)->chars);
}

static inline LitFunction* lit_value_asfunction(LitValue v)
{
    return (LitFunction*)lit_value_asobject(v);
}

static inline LitNativeFunction* lit_value_asnativefunction(LitValue v)
{
    return (LitNativeFunction*)lit_value_asobject(v);
}

static inline LitNativePrimFunction* lit_value_asnativeprimitive(LitValue v)
{
    return (LitNativePrimFunction*)lit_value_asobject(v);
}

static inline LitNativeMethod* lit_value_asnativemethod(LitValue v)
{
    return (LitNativeMethod*)lit_value_asobject(v);
}

static inline LitPrimitiveMethod* lit_value_asprimitivemethod(LitValue v)
{
    return (LitPrimitiveMethod*)lit_value_asobject(v);
}

static inline LitModule* lit_value_asmodule(LitValue v)
{
    return (LitModule*)lit_value_asobject(v);
}

static inline LitClosure* lit_value_asclosure(LitValue v)
{
    return (LitClosure*)lit_value_asobject(v);
}

static inline LitUpvalue* lit_value_asupvalue(LitValue v)
{
    return (LitUpvalue*)lit_value_asobject(v);
}

static inline LitClass* lit_value_asclass(LitValue v)
{
    return (LitClass*)lit_value_asobject(v);
}

static inline LitInstance* lit_value_asinstance(LitValue v)
{
    return (LitInstance*)lit_value_asobject(v);
}

static inline LitArray* lit_value_asarray(LitValue v)
{
    return (LitArray*)lit_value_asobject(v);
}

static inline LitMap* lit_value_asmap(LitValue v)
{
    return (LitMap*)lit_value_asobject(v);
}

static inline LitBoundMethod* lit_value_asboundmethod(LitValue v)
{
    return (LitBoundMethod*)lit_value_asobject(v);
}

static inline LitUserdata* lit_value_asuserdata(LitValue v)
{
    return (LitUserdata*)lit_value_asobject(v);
}

static inline LitRange* lit_value_asrange(LitValue v)
{
    return (LitRange*)lit_value_asobject(v);
}

static inline LitField* lit_value_asfield(LitValue v)
{
    return (LitField*)lit_value_asobject(v);
}

static inline LitFiber* lit_value_asfiber(LitValue v)
{
    return (LitFiber*)lit_value_asobject(v);
}

static inline LitReference* lit_value_asreference(LitValue v)
{
    return (LitReference*)lit_value_asobject(v);
}


/* copy a string, creating a full newly allocated LitString. */
LitString* lit_string_copy(LitState* state, const char* chars, size_t length);


static inline LitValue OBJECT_CONST_STRING(LitState* state, const char* text)
{
    return lit_value_objectvalue(lit_string_copy((state), (text), strlen(text)));
}

static inline LitString* CONST_STRING(LitState* state, const char* text)
{
    return lit_string_copy(state, text, strlen(text));
}

/**
* memory data functions
*/


void lit_datalist_init(LitDataList *array, size_t typsz);
void lit_datalist_destroy(LitState *state, LitDataList *array);
size_t lit_datalist_count(LitDataList *arr);
size_t lit_datalist_size(LitDataList *arr);
size_t lit_datalist_capacity(LitDataList *arr);
void lit_datalist_clear(LitDataList *arr);
void lit_datalist_setcount(LitDataList *arr, size_t nc);
void lit_datalist_deccount(LitDataList *arr);
intptr_t lit_datalist_get(LitDataList *arr, size_t idx);
intptr_t lit_datalist_set(LitDataList *arr, size_t idx, intptr_t val);
void lit_datalist_push(LitState *state, LitDataList *array, intptr_t value);
void lit_datalist_ensuresize(LitState *state, LitDataList *array, size_t size);

void lit_vallist_init(LitValueList *array);
void lit_vallist_destroy(LitState *state, LitValueList *array);
size_t lit_vallist_size(LitValueList *arr);
size_t lit_vallist_count(LitValueList *arr);
size_t lit_vallist_capacity(LitValueList *arr);
void lit_vallist_setcount(LitValueList *arr, size_t nc);
void lit_vallist_clear(LitValueList *arr);
void lit_vallist_deccount(LitValueList *arr);
LitValue lit_vallist_set(LitValueList *arr, size_t idx, LitValue val);
LitValue lit_vallist_get(LitValueList *arr, size_t idx);
void lit_vallist_push(LitState *state, LitValueList *array, LitValue value);
void lit_vallist_ensuresize(LitState *state, LitValueList *values, size_t size);

/* ------ */

void lit_init_chunk(LitChunk* chunk);
void lit_free_chunk(LitState* state, LitChunk* chunk);
void lit_write_chunk(LitState* state, LitChunk* chunk, uint8_t byte, uint16_t line);
size_t lit_chunk_add_constant(LitState* state, LitChunk* chunk, LitValue constant);
size_t lit_chunk_get_line(LitChunk* chunk, size_t offset);
void lit_shrink_chunk(LitState* state, LitChunk* chunk);
void lit_emit_byte(LitState* state, LitChunk* chunk, uint8_t byte);
void lit_emit_bytes(LitState* state, LitChunk* chunk, uint8_t a, uint8_t b);
void lit_emit_short(LitState* state, LitChunk* chunk, uint16_t value);

/**
* state functions
*/
/* creates a new state. */
LitState* lit_make_state();

/* frees a state, releasing associated memory. */
int64_t lit_destroy_state(LitState* state);

bool lit_state_ensurefiber(LitVM* vm, LitFiber* fiber);
void lit_state_pushroot(LitState* state, LitObject* object);
void lit_state_pushvalueroot(LitState* state, LitValue value);
LitValue lit_state_peekroot(LitState* state, uint8_t distance);
void lit_state_poproot(LitState* state);
void lit_state_poproots(LitState* state, uint8_t amount);

LitClass* lit_state_getclassfor(LitState* state, LitValue value);


/* lit_vm_callcallable a function in an instance */
LitInterpretResult lit_state_callinstancemethod(LitState* state, LitValue callee, LitString* mthname, LitValue* argv, size_t argc);
LitValue lit_state_getinstancemethod(LitState* state, LitValue callee, LitString* mthname);

/* print a value to LitWriter */
void lit_towriter_object(LitState* state, LitWriter* wr, LitValue value);
void lit_towriter_value(LitState* state, LitWriter* wr, LitValue value);
void lit_towriter_ast(LitState* state, LitWriter* wr, LitExprList* exlist);
/* returns the static string name of this type. does *not* represent class name, et al. just the LitValueType name! */
const char* lit_tostring_typename(LitValue value);

/* allocate/reallocate memory. if new_size is 0, frees the pointer, and returns NULL. */
void* lit_gcmem_memrealloc(LitState* state, void* pointer, size_t old_size, size_t new_size);

/* releases given objects, and their subobjects. */
void lit_object_destroylistof(LitState* state, LitObject* objects);

/* run garbage collector */
uint64_t lit_gcmem_collectgarbage(LitVM* vm);

/* mark an object for collection. */
void lit_gcmem_markobject(LitVM* vm, LitObject* object);

/* mark a value for collection. */
void lit_gcmem_markvalue(LitVM* vm, LitValue value);

/* free a object */
void lit_object_destroy(LitState* state, LitObject* object);



void lit_table_init(LitState* state, LitTable* table);
void lit_table_destroy(LitState* state, LitTable* table);
bool lit_table_set(LitState* state, LitTable* table, LitString* key, LitValue value);
bool lit_table_get(LitTable* table, LitString* key, LitValue* value);
bool lit_table_get_slot(LitTable* table, LitString* key, LitValue** value);
bool lit_table_delete(LitTable* table, LitString* key);
LitString* lit_table_find_string(LitTable* table, const char* chars, size_t length, uint32_t hash);
void lit_table_add_all(LitState* state, LitTable* from, LitTable* to);
void lit_table_removewhite(LitTable* table);
void lit_gcmem_marktable(LitVM* vm, LitTable* table);
bool lit_is_callable_function(LitValue value);

/**
* string methods
*/

/*
* create a LitString by reusing $chars. ONLY use this if you're certain that $chars isn't being freed.
* if $wassds is true, then the sds instance is reused as-is.
*/
LitString* lit_string_take(LitState* state, char* chars, size_t length, bool wassds);

/*
* creates a formatted string. is NOT printf-compatible!
*
* #: assume number, or use toString()
* $: assume string, or use toString()
* @: value toString()-ified
*
* e.g.:
*   foo = (LitString instance) "bar"
*   lit_string_format("foo=@", foo)
*   => "foo=bar"
*
* it's extremely rudimentary, and the idea is to quickly join values.
*/
LitValue lit_string_format(LitState* state, const char* format, ...);

/* turn a given number to LitValue'd LitString. */
LitValue lit_string_numbertostring(LitState* state, double value);

/* registers a string in the string table. */
void lit_state_regstring(LitState* state, LitString* string);


/*
* create a new string instance.
* if $reuse is false, then a new sds-string is created, otherwise $chars is set to NULL.
* this is to avoid double-allocating, which would create a sds-instance that cannot be freed.
*/
LitString* lit_string_makeempty(LitState* state, size_t length, bool reuse);

/* get length of this string */
size_t lit_string_getlength(LitString* ls);
const char* lit_string_getdata(LitString* ls);
void lit_string_appendlen(LitString* ls, const char* s, size_t len);
void lit_string_appendobj(LitString* ls, LitString* other);
void lit_string_appendchar(LitString* ls, char ch);
bool lit_string_equal(LitState* state, LitString* a, LitString* b);


LitFunction* lit_create_function(LitState* state, LitModule* module);
LitValue lit_get_function_name(LitVM* vm, LitValue instance);
LitUpvalue* lit_create_upvalue(LitState* state, LitValue* slot);
LitClosure* lit_create_closure(LitState* state, LitFunction* function);
LitNativeFunction* lit_create_native_function(LitState* state, LitNativeFunctionFn function, LitString* name);
LitNativePrimFunction* lit_create_native_primitive(LitState* state, LitNativePrimitiveFn function, LitString* name);
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
LitUserdata* lit_create_userdata(LitState* state, size_t size, bool ispointeronly);
LitRange* lit_create_range(LitState* state, double from, double to);
LitField* lit_create_field(LitState* state, LitObject* getter, LitObject* setter);
LitReference* lit_create_reference(LitState* state, LitValue* slot);

void lit_array_push(LitState* state, LitArray* array, LitValue val);


/*
 * Please, do not provide a const string source to the compiler, because it will
 * get modified, if it has any macros in it!
 */
LitModule* lit_state_compilemodule(LitState* state, LitString* module_name, const char* code, size_t len);
LitModule* lit_state_getmodule(LitState* state, const char* name);

LitInterpretResult lit_state_internexecsource(LitState* state, LitString* module_name, const char* code, size_t len);
LitInterpretResult lit_state_execsource(LitState* state, const char* module_name, const char* code, size_t len);
LitInterpretResult lit_state_execfile(LitState* state, const char* file);
LitInterpretResult lit_state_dumpfile(LitState* state, const char* file);
bool lit_state_compileandsave(LitState* state, char* files[], size_t num_files, const char* output_file);

void lit_state_raiseerror(LitState* state, LitErrType type, const char* message, ...);
void lit_state_printf(LitState* state, const char* message, ...);
void lit_enable_compilation_time_measurement();

void lit_vm_init(LitState* state, LitVM* vm);
void lit_vm_destroy(LitVM* vm);
void lit_vm_tracestack(LitVM* vm, LitWriter* wr);

static inline void lit_vm_push(LitVM* vm, LitValue value)
{
    *(vm->fiber->stack_top) = value;
    vm->fiber->stack_top++;

}

static inline LitValue lit_vm_pop(LitVM* vm)
{
    LitValue rt;
    rt = *(vm->fiber->stack_top);
    vm->fiber->stack_top--;
    return rt;
}

LitInterpretResult lit_vm_execmodule(LitState* state, LitModule* module);
LitInterpretResult lit_vm_execfiber(LitState* state, LitFiber* fiber);
bool lit_vm_handleruntimeerror(LitVM* vm, LitString* error_string);
bool lit_vm_vraiseerror(LitVM* vm, const char* format, va_list args);
bool lit_vm_raiseerror(LitVM* vm, const char* format, ...);
bool lit_vm_raiseexitingerror(LitVM* vm, const char* format, ...);

void lit_vmutil_callexitjump();
bool lit_vmutil_setexitjump();


LitInterpretResult lit_state_callfunction(LitState* state, LitFunction* callee, LitValue* arguments, uint8_t argument_count, bool ignfiber);
LitInterpretResult lit_state_callmethod(LitState* state, LitValue instance, LitValue callee, LitValue* arguments, uint8_t argument_count, bool ignfiber);
LitInterpretResult lit_state_callvalue(LitState* state, LitValue callee, LitValue* arguments, uint8_t argument_count, bool ignfiber);
LitInterpretResult lit_state_findandcallmethod(LitState* state, LitValue callee, LitString* method_name, LitValue* arguments, uint8_t argument_count, bool ignfiber);

LitString* lit_value_tostring(LitState* state, LitValue object);
LitValue lit_value_callnew(LitVM* vm, const char* name, LitValue* args, size_t arg_count, bool ignfiber);


void lit_api_init(LitState* state);
void lit_api_destroy(LitState* state);

LitValue lit_state_getglobalvalue(LitState* state, LitString* name);
LitFunction* lit_state_getglobalfunction(LitState* state, LitString* name);

void lit_state_setglobal(LitState* state, LitString* name, LitValue value);
bool lit_state_hasglobal(LitState* state, LitString* name);
void lit_state_defnativefunc(LitState* state, const char* name, LitNativeFunctionFn native);
void lit_state_defnativeprimitive(LitState* state, const char* name, LitNativePrimitiveFn native);

double lit_value_checknumber(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
double lit_value_getnumber(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, double def);

bool lit_value_checkbool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
bool lit_value_getbool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, bool def);

const char* lit_value_checkstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
const char* lit_value_getstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, const char* def);

LitString* lit_value_checkobjstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
LitInstance* lit_value_checkinstance(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);
LitValue* lit_value_checkreference(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id);

void lit_value_ensurebool(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror);
void lit_value_ensurestring(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror);
void lit_value_ensurenumber(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror);
void lit_value_ensureobjtype(LitVM* vm, LitValue value, LitObjType type, const char* lit_emitter_raiseerror);


LitValue lit_state_getfield(LitState* state, LitTable* table, const char* name);
LitValue lit_state_getmapfield(LitState* state, LitMap* map, const char* name);

void lit_state_setfield(LitState* state, LitTable* table, const char* name, LitValue value);
void lit_state_setmapfield(LitState* state, LitMap* map, const char* name, LitValue value);
void lit_disassemble_module(LitState* state, LitModule* module, const char* source);
void lit_disassemble_chunk(LitState* state, LitChunk* chunk, const char* name, const char* source);
size_t lit_disassemble_instruction(LitState* state, LitChunk* chunk, size_t offset, const char* source);

void lit_trace_frame(LitFiber* fiber, LitWriter* wr);



bool lit_parse(LitParser* parser, const char* file_name, const char* source, LitExprList* statements);

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

void lit_init_emulated_file(LitEmulatedFile* file, const char* source, size_t len);

uint8_t lit_read_euint8_t(LitEmulatedFile* file);
uint16_t lit_read_euint16_t(LitEmulatedFile* file);
uint32_t lit_read_euint32_t(LitEmulatedFile* file);
double lit_read_edouble(LitEmulatedFile* file);
LitString* lit_read_estring(LitState* state, LitEmulatedFile* file);

void lit_save_module(LitModule* module, FILE* file);
LitModule* lit_load_module(LitState* state, const char* input, size_t len);
bool lit_generate_source_file(const char* file, const char* output);
void lit_build_native_runner(const char* bytecode_file);

void lit_open_libraries(LitState* state);
void lit_open_core_library(LitState* state);

void lit_open_math_library(LitState* state);

void lit_open_file_library(LitState* state);

void lit_open_gc_library(LitState* state);



int lit_ustring_length(LitString* string);

int lit_ustring_decode(const uint8_t* bytes, uint32_t length);
int lit_ustring_encode(int value, uint8_t* bytes);

LitString* lit_ustring_codepointat(LitState* state, LitString* string, uint32_t index);
LitString* lit_ustring_fromcodepoint(LitState* state, int value);
LitString* lit_ustring_fromrange(LitState* state, LitString* source, int start, uint32_t count);


static inline bool lit_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool lit_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}


LitString* lit_vformat_error(LitState* state, size_t line, LitError lit_emitter_raiseerror, va_list args);
LitString* lit_format_error(LitState* state, size_t line, LitError lit_emitter_raiseerror, ...);

void lit_init_scanner(LitState* state, LitScanner* scanner, const char* file_name, const char* source);
LitToken lit_scan_token(LitScanner* scanner);
LitToken lit_scan_rollback(LitScanner* scanner);
void lit_init_optimizer(LitState* state, LitOptimizer* optimizer);
void lit_optimize(LitOptimizer* optimizer, LitExprList* statements);
const char* lit_get_optimization_level_description(LitOptLevel level);

bool lit_is_optimization_enabled(LitOptimization optimization);
void lit_set_optimization_enabled(LitOptimization optimization, bool enabled);
void lit_set_all_optimization_enabled(bool enabled);
void lit_set_optimization_level(LitOptLevel level);

const char* lit_get_optimization_name(LitOptimization optimization);
const char* lit_get_optimization_description(LitOptimization optimization);
void lit_preproc_init(LitState* state, LitPreprocessor* preprocessor);
void lit_preproc_destroy(LitPreprocessor* preprocessor);
void lit_preproc_setdef(LitState* state, const char* name);

bool lit_preproc_run(LitPreprocessor* preprocessor, char* source);
