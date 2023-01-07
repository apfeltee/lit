
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

#define LIT_ENSURE_ARGS(state, count)                                                   \
    if(argc != count)                                                       \
    {                                                                            \
        lit_vm_raiseerror(vm, "expected %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                       \
    }

#define LIT_ENSURE_MIN_ARGS(state, count)                                                       \
    if(argc < count)                                                                \
    {                                                                                    \
        lit_vm_raiseerror(state->vm, "expected minimum %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                               \
    }

#define LIT_ENSURE_MAX_ARGS(state, count)                                                       \
    if(argc > count)                                                                \
    {                                                                                    \
        lit_vm_raiseerror(state->vm, "expected maximum %i argument, got %i", count, argc); \
        return NULL_VALUE;                                                               \
    }


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


#define PUSH(value) (*fiber->stack_top++ = value)
#define RETURN_OK(r) return (LitInterpretResult){ LITRESULT_OK, r };

#define RETURN_RUNTIME_ERROR() return (LitInterpretResult){ LITRESULT_RUNTIME_ERROR, NULL_VALUE };

#if 1
    #define FALSE_VALUE ((LitValue)(uint64_t)(QNAN | TAG_FALSE))
    #define TRUE_VALUE ((LitValue)(uint64_t)(QNAN | TAG_TRUE))
    #define NULL_VALUE ((LitValue)(uint64_t)(QNAN | TAG_NULL))
#else
    #define FALSE_VALUE ((LitObject){.type=LITTYPE_BOOL, .boolval=false})
    #define TRUE_VALUE ((LitObject){.type=LITTYPE_BOOL, .boolval=true})
    #define NULL_VALUE ((LitObject){.type=LITTYPE_NULL})

#endif


typedef struct /**/LitObject LitObject;

typedef uint64_t LitValue;
//typedef LitObject LitValue;


#include "structs.h"

#include "prot.inc"

#define lit_value_objectvalue(obj) lit_value_objectvalue_actual((uintptr_t)obj)

#define lit_value_istype(value, t) \
    (lit_value_isobject(value) && (lit_value_asobject(value) != NULL) && (lit_value_asobject(value)->type == t))


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

static inline LitValue OBJECT_CONST_STRING(LitState* state, const char* text)
{
    return lit_value_objectvalue(lit_string_copy((state), (text), strlen(text)));
}

static inline LitString* CONST_STRING(LitState* state, const char* text)
{
    return lit_string_copy(state, text, strlen(text));
}

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


static inline bool lit_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool lit_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
