
#pragma once

#include "lit.h"


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

struct LitPrivates
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
    LitExpression expression;
    LitValue value;
};

struct LitBinaryExpr
{
    LitExpression expression;
    LitExpression* left;
    LitExpression* right;
    LitTokenType op;
    bool ignore_left;
};

struct LitUnaryExpr
{
    LitExpression expression;
    LitExpression* right;
    LitTokenType op;
};

struct LitVarExpr
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
    LitExpression* body;
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
    LitExpression statement;
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
    LitExpression** values;
};

struct LitExpressionStatement
{
    LitExpression statement;
    LitExpression* expression;
    bool pop;
};

struct LitBlockStatement
{
    LitExpression statement;
    LitStmtList statements;
};

struct LitVarStatement
{
    LitExpression statement;
    const char* name;
    size_t length;
    bool constant;
    LitExpression* init;
};

struct LitIfStatement
{
    LitExpression statement;
    LitExpression* condition;
    LitExpression* if_branch;
    LitExpression* else_branch;
    LitExprList* elseif_conditions;
    LitStmtList* elseif_branches;
};

struct LitWhileStatement
{
    LitExpression statement;
    LitExpression* condition;
    LitExpression* body;
};

struct LitForStatement
{
    LitExpression statement;
    LitExpression* init;
    LitExpression* var;
    LitExpression* condition;
    LitExpression* increment;
    LitExpression* body;
    bool c_style;
};

struct LitContinueStatement
{
    LitExpression statement;
};

struct LitBreakStatement
{
    LitExpression statement;
};

struct LitFunctionStatement
{
    LitExpression statement;
    const char* name;
    size_t length;
    LitParameters parameters;
    LitExpression* body;
    bool exported;
};

struct LitReturnStatement
{
    LitExpression statement;
    LitExpression* expression;
};

struct LitMethodStatement
{
    LitExpression statement;
    LitString* name;
    LitParameters parameters;
    LitExpression* body;
    bool is_static;
};

struct LitClassStatement
{
    LitExpression statement;
    LitString* name;
    LitString* parent;
    LitStmtList fields;
};

struct LitFieldStatement
{
    LitExpression statement;
    LitString* name;
    LitExpression* getter;
    LitExpression* setter;
    bool is_static;
};

void lit_init_expressions(LitExprList* array);
void lit_free_expressions(LitState* state, LitExprList* array);
void lit_expressions_write(LitState* state, LitExprList* array, LitExpression* value);

void lit_init_parameters(LitParameters* array);
void lit_free_parameters(LitState* state, LitParameters* array);
void lit_parameters_write(LitState* state, LitParameters* array, LitParameter value);

void lit_init_statements(LitStmtList* array);
void lit_free_statements(LitState* state, LitStmtList* array);
void lit_statements_write(LitState* state, LitStmtList* array, LitExpression* value);


void lit_init_privates(LitPrivates* array);
void lit_free_privates(LitState* state, LitPrivates* array);
void lit_privates_write(LitState* state, LitPrivates* array, LitPrivate value);

void lit_init_locals(LitLocals* array);
void lit_free_locals(LitState* state, LitLocals* array);
void lit_locals_write(LitState* state, LitLocals* array, LitLocal value);

void lit_init_variables(LitVarList* array);
void lit_free_variables(LitState* state, LitVarList* array);
void lit_variables_write(LitState* state, LitVarList* array, LitVariable value);

void lit_free_expression(LitState* state, LitExpression* expression);
LitLiteralExpr* lit_create_literal_expression(LitState* state, size_t line, LitValue value);

LitBinaryExpr*
lit_create_binary_expression(LitState* state, size_t line, LitExpression* left, LitExpression* right, LitTokenType op);
LitUnaryExpr* lit_create_unary_expression(LitState* state, size_t line, LitExpression* right, LitTokenType op);
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
                                        LitStmtList* elseif_branches);

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

LitStmtList* lit_allocate_statements(LitState* state);
void lit_free_allocated_statements(LitState* state, LitStmtList* statements);
void lit_init_emitter(LitState* state, LitEmitter* emitter);
void lit_free_emitter(LitEmitter* emitter);

LitModule* lit_emit(LitEmitter* emitter, LitStmtList* statements, LitString* module_name);

void lit_init_parser(LitState* state, LitParser* parser);
void lit_free_parser(LitParser* parser);



