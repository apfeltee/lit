#include "lit.h"
#include <stdio.h>

static const char* error_messages[ERROR_TOTAL] =
{
    "unclosed macro.",
    "unknown macro '%.*s'.",
    "unexpected character '%c'",
    "unterminated string",
    "invalid escape character '%c'",
    "interpolation nesting is too deep, maximum is %i",
    "number is too big to be represented by a single literal",
    "expected '%c' after '%c', got '%c'",
    "expected %s, got '%.*s'",
    "invalid assigment target",
    "function cannot have more than 255 arguments, got %i",
    "if-statement can have only one else-branch",
    "for-loops using in-iteration must declare a new variable",
    "expected declaration of either getter or setter, got none",
    "operator methods cannot be static or defined in static classes",
    "class cannot inherit itself",
    "all static fields must be defined before the methods",
    "expected statement but got nothing",
    "expected expression after '%.*s', got '%.*s'",
    "default arguments must always be in the end of the argument list.",
    "too many constants for one chunk",
    "too many private locals for one module",
    "variable '%.*s' was already declared in this scope",
    "too many local variables for one function",
    "too many upvalues for one function",
    "variable '%.*s' cannot use itself in its initializer",
    "too much code to jump over",
    "'super' cannot be used in class '%s', because it does not have a super class",
    "'this' cannot be used %s",
    "'super' cannot be used %s",
    "unknown expression with id '%i'",
    "unknown statement with id '%i'",
    "cannot use '%s' outside of loops",
    "cannot use 'return' in constructors",
    "constructors cannot be static (at least for now)",
    "attempt to modify constant '%.*s'",
    "invalid refence target"
};

LitString* lit_vformat_error(LitState* state, size_t line, LitError error, va_list args)
{
    int error_id = (int)error;
    const char* error_message = error_messages[error_id];

    va_list args_copy;
    va_copy(args_copy, args);
    size_t buffer_size = vsnprintf(NULL, 0, error_message, args_copy) + 1;
    va_end(args_copy);

    char buffer[buffer_size];

    vsnprintf(buffer, buffer_size, error_message, args);
    buffer[buffer_size - 1] = '\0';

    if(line != 0)
    {
        return AS_STRING(lit_string_format(state, "[err # line #]: $", (double)error_id, (double)line, (const char*)buffer));
    }

    return AS_STRING(lit_string_format(state, "[err #]: $", (double)error_id, (const char*)buffer));
}

LitString* lit_format_error(LitState* state, size_t line, LitError error, ...)
{
    va_list args;
    va_start(args, error);
    LitString* result = lit_vformat_error(state, line, error, args);
    va_end(args);

    return result;
}