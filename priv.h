
#pragma once

#include "lit.h"

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


enum LitOpCode
{
#define OPCODE(name, effect) OP_##name,
#include "opcodes.inc"
#undef OPCODE
};

typedef enum /**/LitOpCode LitOpCode;
typedef struct /**/LitLiteralExpr LitLiteralExpr;
typedef struct /**/LitBinaryExpr LitBinaryExpr;
typedef struct /**/LitUnaryExpr LitUnaryExpr;
typedef struct /**/LitVarExpr LitVarExpr;
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

typedef LitExpression* (*LitPrefixParseFn)(LitParser*, bool);
typedef LitExpression* (*LitInfixParseFn)(LitParser*, LitExpression*, bool);

struct LitPrivate
{
    bool initialized;
    bool constant;
};

struct LitPrivList
{
    size_t capacity;
    size_t count;
    LitPrivate* values;
};

struct LitEmitter
{
    LitState* state;
    LitChunk* chunk;
    LitCompiler* compiler;
    size_t last_line;
    size_t loop_start;
    LitPrivList privates;
    LitUintList breaks;
    LitUintList continues;
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

/*
 * Expressions
 */
struct LitExprList
{
    size_t capacity;
    size_t count;
    LitExpression** values;
};

struct LitLiteralExpr
{
    LitExpression exobj;
    LitValue value;
};

struct LitBinaryExpr
{
    LitExpression exobj;
    LitExpression* left;
    LitExpression* right;
    LitTokType op;
    bool ignore_left;
};

struct LitUnaryExpr
{
    LitExpression exobj;
    LitExpression* right;
    LitTokType op;
};

struct LitVarExpr
{
    LitExpression exobj;
    const char* name;
    size_t length;
};

struct LitAssignExpression
{
    LitExpression exobj;
    LitExpression* to;
    LitExpression* value;
};

struct LitCallExpression
{
    LitExpression exobj;
    LitExpression* callee;
    LitExprList args;
    LitExpression* init;
};

struct LitGetExpression
{
    LitExpression exobj;
    LitExpression* where;
    const char* name;
    size_t length;
    int jump;
    bool ignore_emit;
    bool ignore_result;
};

struct LitSetExpression
{
    LitExpression exobj;
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

struct LitParamList
{
    size_t capacity;
    size_t count;
    LitParameter* values;
};


struct LitLambdaExpression
{
    LitExpression exobj;
    LitParamList parameters;
    LitExpression* body;
};

struct LitArrayExpression
{
    LitExpression exobj;
    LitExprList values;
};

struct LitObjectExpression
{
    LitExpression exobj;
    LitValueList keys;
    LitExprList values;
};

struct LitSubscriptExpression
{
    LitExpression exobj;
    LitExpression* array;
    LitExpression* index;
};

struct LitThisExpression
{
    LitExpression exobj;
};

struct LitSuperExpression
{
    LitExpression exobj;
    LitString* method;
    bool ignore_emit;
    bool ignore_result;
};

struct LitRangeExpression
{
    LitExpression exobj;
    LitExpression* from;
    LitExpression* to;
};

struct LitIfExpression
{
    LitExpression exobj;
    LitExpression* condition;
    LitExpression* if_branch;
    LitExpression* else_branch;
};

struct LitInterpolationExpression
{
    LitExpression exobj;
    LitExprList expressions;
};

struct LitReferenceExpression
{
    LitExpression exobj;
    LitExpression* to;
};

/*
 * Statements
 */

struct LitExpressionStatement
{
    LitExpression exobj;
    LitExpression* expression;
    bool pop;
};

struct LitBlockStatement
{
    LitExpression exobj;
    LitExprList statements;
};

struct LitVarStatement
{
    LitExpression exobj;
    const char* name;
    size_t length;
    bool constant;
    LitExpression* init;
};

struct LitIfStatement
{
    LitExpression exobj;
    LitExpression* condition;
    LitExpression* if_branch;
    LitExpression* else_branch;
    LitExprList* elseif_conditions;
    LitExprList* elseif_branches;
};

struct LitWhileStatement
{
    LitExpression exobj;
    LitExpression* condition;
    LitExpression* body;
};

struct LitForStatement
{
    LitExpression exobj;
    LitExpression* init;
    LitExpression* var;
    LitExpression* condition;
    LitExpression* increment;
    LitExpression* body;
    bool c_style;
};

struct LitContinueStatement
{
    LitExpression exobj;
};

struct LitBreakStatement
{
    LitExpression exobj;
};

struct LitFunctionStatement
{
    LitExpression exobj;
    const char* name;
    size_t length;
    LitParamList parameters;
    LitExpression* body;
    bool exported;
};

struct LitReturnStatement
{
    LitExpression exobj;
    LitExpression* expression;
};

struct LitMethodStatement
{
    LitExpression exobj;
    LitString* name;
    LitParamList parameters;
    LitExpression* body;
    bool is_static;
};

struct LitClassStatement
{
    LitExpression exobj;
    LitString* name;
    LitString* parent;
    LitExprList fields;
};

struct LitFieldStatement
{
    LitExpression exobj;
    LitString* name;
    LitExpression* getter;
    LitExpression* setter;
    bool is_static;
};

void lit_exprlist_init(LitExprList* array);
void lit_exprlist_destroy(LitState* state, LitExprList* array);
void lit_exprlist_push(LitState* state, LitExprList* array, LitExpression* value);

void lit_paramlist_init(LitParamList* array);
void lit_paramlist_destroy(LitState* state, LitParamList* array);
void lit_paramlist_push(LitState* state, LitParamList* array, LitParameter value);

void lit_privlist_init(LitPrivList* array);
void lit_privlist_destroy(LitState* state, LitPrivList* array);
void lit_privlist_push(LitState* state, LitPrivList* array, LitPrivate value);

void lit_loclist_init(LitLocList* array);
void lit_loclist_destroy(LitState* state, LitLocList* array);
void lit_loclist_push(LitState* state, LitLocList* array, LitLocal value);

void lit_varlist_init(LitVarList* array);
void lit_varlist_destroy(LitState* state, LitVarList* array);
void lit_varlist_push(LitState* state, LitVarList* array, LitVariable value);

void lit_free_expression(LitState* state, LitExpression* expression);
LitLiteralExpr* lit_create_literal_expression(LitState* state, size_t line, LitValue value);

LitBinaryExpr*
lit_create_binary_expression(LitState* state, size_t line, LitExpression* left, LitExpression* right, LitTokType op);
LitUnaryExpr* lit_create_unary_expression(LitState* state, size_t line, LitExpression* right, LitTokType op);
LitVarExpr* lit_create_var_expression(LitState* state, size_t line, const char* name, size_t length);
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
void lit_free_statement(LitState* state, LitExpression* statement);
LitExpressionStatement* lit_create_expression_statement(LitState* state, size_t line, LitExpression* expression);
LitBlockStatement* lit_create_block_statement(LitState* state, size_t line);
LitVarStatement* lit_create_var_statement(LitState* state, size_t line, const char* name, size_t length, LitExpression* init, bool constant);

LitIfStatement* lit_create_if_statement(LitState* state,
                                        size_t line,
                                        LitExpression* condition,
                                        LitExpression* if_branch,
                                        LitExpression* else_branch,
                                        LitExprList* elseif_conditions,
                                        LitExprList* elseif_branches);

LitWhileStatement* lit_create_while_statement(LitState* state, size_t line, LitExpression* condition, LitExpression* body);

LitForStatement* lit_create_for_statement(LitState* state,
                                          size_t line,
                                          LitExpression* init,
                                          LitExpression* var,
                                          LitExpression* condition,
                                          LitExpression* increment,
                                          LitExpression* body,
                                          bool c_style);
LitContinueStatement* lit_create_continue_statement(LitState* state, size_t line);
LitBreakStatement* lit_create_break_statement(LitState* state, size_t line);
LitFunctionStatement* lit_create_function_statement(LitState* state, size_t line, const char* name, size_t length);
LitReturnStatement* lit_create_return_statement(LitState* state, size_t line, LitExpression* expression);
LitMethodStatement* lit_create_method_statement(LitState* state, size_t line, LitString* name, bool is_static);
LitClassStatement* lit_create_class_statement(LitState* state, size_t line, LitString* name, LitString* parent);
LitFieldStatement*
lit_create_field_statement(LitState* state, size_t line, LitString* name, LitExpression* getter, LitExpression* setter, bool is_static);

LitExprList* lit_allocate_expressions(LitState* state);
void lit_free_allocated_expressions(LitState* state, LitExprList* expressions);

LitExprList* lit_allocate_statements(LitState* state);
void lit_free_allocated_statements(LitState* state, LitExprList* statements);
void lit_init_emitter(LitState* state, LitEmitter* emitter);
void lit_free_emitter(LitEmitter* emitter);

LitModule* lit_emit(LitEmitter* emitter, LitExprList* statements, LitString* module_name);

void lit_init_parser(LitState* state, LitParser* parser);
void lit_free_parser(LitParser* parser);



