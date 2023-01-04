
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


#define PUSH(value) (*fiber->stack_top++ = value)
#define RETURN_OK(r) return (LitInterpretResult){ LITRESULT_OK, r };

#define RETURN_RUNTIME_ERROR() return (LitInterpretResult){ LITRESULT_RUNTIME_ERROR, NULL_VALUE };



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
typedef struct /**/LitAssignExpr LitAssignExpr;
typedef struct /**/LitCallExpr LitCallExpr;
typedef struct /**/LitGetExpr LitGetExpr;
typedef struct /**/LitSetExpr LitSetExpr;
typedef struct /**/LitParameter LitParameter;
typedef struct /**/LitLambdaExpr LitLambdaExpr;
typedef struct /**/LitArrayExpr LitArrayExpr;
typedef struct /**/LitObjectExpr LitObjectExpr;
typedef struct /**/LitSubscriptExpr LitSubscriptExpr;
typedef struct /**/LitThisExpr LitThisExpr;
typedef struct /**/LitSuperExpr LitSuperExpr;
typedef struct /**/LitRangeExpr LitRangeExpr;
typedef struct /**/LitTernaryExpr LitTernaryExpr;
typedef struct /**/LitInterpolationExpr LitInterpolationExpr;
typedef struct /**/LitReferenceExpr LitReferenceExpr;
typedef struct /**/LitExpressionExpr LitExpressionExpr;
typedef struct /**/LitBlockExpr LitBlockExpr;
typedef struct /**/LitAssignVarExpr LitAssignVarExpr;
typedef struct /**/LitIfExpr LitIfExpr;
typedef struct /**/LitWhileExpr LitWhileExpr;
typedef struct /**/LitForExpr LitForExpr;
typedef struct /**/LitContinueExpr LitContinueExpr;
typedef struct /**/LitBreakExpr LitBreakExpr;
typedef struct /**/LitFunctionExpr LitFunctionExpr;
typedef struct /**/LitReturnExpr LitReturnExpr;
typedef struct /**/LitMethodExpr LitMethodExpr;
typedef struct /**/LitClassExpr LitClassExpr;
typedef struct /**/LitFieldExpr LitFieldExpr;
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

struct LitAssignExpr
{
    LitExpression exobj;
    LitExpression* to;
    LitExpression* value;
};

struct LitCallExpr
{
    LitExpression exobj;
    LitExpression* callee;
    LitExprList args;
    LitExpression* init;
};

struct LitGetExpr
{
    LitExpression exobj;
    LitExpression* where;
    const char* name;
    size_t length;
    int jump;
    bool ignore_emit;
    bool ignore_result;
};

struct LitSetExpr
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


struct LitLambdaExpr
{
    LitExpression exobj;
    LitParamList parameters;
    LitExpression* body;
};

struct LitArrayExpr
{
    LitExpression exobj;
    LitExprList values;
};

struct LitObjectExpr
{
    LitExpression exobj;
    LitValueList keys;
    LitExprList values;
};

struct LitSubscriptExpr
{
    LitExpression exobj;
    LitExpression* array;
    LitExpression* index;
};

struct LitThisExpr
{
    LitExpression exobj;
};

struct LitSuperExpr
{
    LitExpression exobj;
    LitString* method;
    bool ignore_emit;
    bool ignore_result;
};

struct LitRangeExpr
{
    LitExpression exobj;
    LitExpression* from;
    LitExpression* to;
};

struct LitTernaryExpr
{
    LitExpression exobj;
    LitExpression* condition;
    LitExpression* if_branch;
    LitExpression* else_branch;
};

struct LitInterpolationExpr
{
    LitExpression exobj;
    LitExprList expressions;
};

struct LitReferenceExpr
{
    LitExpression exobj;
    LitExpression* to;
};

/*
 * Statements
 */

struct LitExpressionExpr
{
    LitExpression exobj;
    LitExpression* expression;
    bool pop;
};

struct LitBlockExpr
{
    LitExpression exobj;
    LitExprList statements;
};

struct LitAssignVarExpr
{
    LitExpression exobj;
    const char* name;
    size_t length;
    bool constant;
    LitExpression* init;
};

struct LitIfExpr
{
    LitExpression exobj;
    LitExpression* condition;
    LitExpression* if_branch;
    LitExpression* else_branch;
    LitExprList* elseif_conditions;
    LitExprList* elseif_branches;
};

struct LitWhileExpr
{
    LitExpression exobj;
    LitExpression* condition;
    LitExpression* body;
};

struct LitForExpr
{
    LitExpression exobj;
    LitExpression* init;
    LitExpression* var;
    LitExpression* condition;
    LitExpression* increment;
    LitExpression* body;
    bool c_style;
};

struct LitContinueExpr
{
    LitExpression exobj;
};

struct LitBreakExpr
{
    LitExpression exobj;
};

struct LitFunctionExpr
{
    LitExpression exobj;
    const char* name;
    size_t length;
    LitParamList parameters;
    LitExpression* body;
    bool exported;
};

struct LitReturnExpr
{
    LitExpression exobj;
    LitExpression* expression;
};

struct LitMethodExpr
{
    LitExpression exobj;
    LitString* name;
    LitParamList parameters;
    LitExpression* body;
    bool is_static;
};

struct LitClassExpr
{
    LitExpression exobj;
    LitString* name;
    LitString* parent;
    LitExprList fields;
};

struct LitFieldExpr
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

void lit_ast_destroyexpression(LitState* state, LitExpression* expression);
LitLiteralExpr* lit_ast_make_literalexpr(LitState* state, size_t line, LitValue value);

LitBinaryExpr*
lit_ast_make_binaryexpr(LitState* state, size_t line, LitExpression* left, LitExpression* right, LitTokType op);
LitUnaryExpr* lit_ast_make_unaryexpr(LitState* state, size_t line, LitExpression* right, LitTokType op);
LitVarExpr* lit_ast_make_varexpr(LitState* state, size_t line, const char* name, size_t length);
LitAssignExpr* lit_ast_make_assignexpr(LitState* state, size_t line, LitExpression* to, LitExpression* value);
LitCallExpr* lit_ast_make_callexpr(LitState* state, size_t line, LitExpression* callee);
LitGetExpr*
lit_ast_make_getexpr(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, bool questionable, bool ignore_result);
LitSetExpr*
lit_ast_make_setexpr(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, LitExpression* value);
LitLambdaExpr* lit_ast_make_lambdaexpr(LitState* state, size_t line);
LitArrayExpr* lit_ast_make_arrayexpr(LitState* state, size_t line);
LitObjectExpr* lit_ast_make_objectexpr(LitState* state, size_t line);
LitSubscriptExpr* lit_ast_make_subscriptexpr(LitState* state, size_t line, LitExpression* array, LitExpression* index);
LitThisExpr* lit_ast_make_thisexpr(LitState* state, size_t line);

LitSuperExpr* lit_ast_make_superexpr(LitState* state, size_t line, LitString* method, bool ignore_result);
LitRangeExpr* lit_ast_make_rangeexpr(LitState* state, size_t line, LitExpression* from, LitExpression* to);
LitTernaryExpr*
lit_ast_make_ternaryexpr(LitState* state, size_t line, LitExpression* condition, LitExpression* if_branch, LitExpression* else_branch);

LitInterpolationExpr* lit_ast_make_strinterpolexpr(LitState* state, size_t line);
LitReferenceExpr* lit_ast_make_referenceexpr(LitState* state, size_t line, LitExpression* to);
void lit_free_statement(LitState* state, LitExpression* statement);
LitExpressionExpr* lit_ast_make_exprstmt(LitState* state, size_t line, LitExpression* expression);
LitBlockExpr* lit_ast_make_blockexpr(LitState* state, size_t line);
LitAssignVarExpr* lit_ast_make_assignvarexpr(LitState* state, size_t line, const char* name, size_t length, LitExpression* init, bool constant);

LitIfExpr* lit_ast_make_ifexpr(LitState* state,
                                        size_t line,
                                        LitExpression* condition,
                                        LitExpression* if_branch,
                                        LitExpression* else_branch,
                                        LitExprList* elseif_conditions,
                                        LitExprList* elseif_branches);

LitWhileExpr* lit_ast_make_whileexpr(LitState* state, size_t line, LitExpression* condition, LitExpression* body);

LitForExpr* lit_ast_make_forexpr(LitState* state,
                                          size_t line,
                                          LitExpression* init,
                                          LitExpression* var,
                                          LitExpression* condition,
                                          LitExpression* increment,
                                          LitExpression* body,
                                          bool c_style);
LitContinueExpr* lit_ast_make_continueexpr(LitState* state, size_t line);
LitBreakExpr* lit_ast_make_breakexpr(LitState* state, size_t line);
LitFunctionExpr* lit_ast_make_funcexpr(LitState* state, size_t line, const char* name, size_t length);
LitReturnExpr* lit_ast_make_returnexpr(LitState* state, size_t line, LitExpression* expression);
LitMethodExpr* lit_ast_make_methodexpr(LitState* state, size_t line, LitString* name, bool is_static);
LitClassExpr* lit_ast_make_classexpr(LitState* state, size_t line, LitString* name, LitString* parent);
LitFieldExpr*
lit_ast_make_fieldexpr(LitState* state, size_t line, LitString* name, LitExpression* getter, LitExpression* setter, bool is_static);

LitExprList* lit_ast_allocexprlist(LitState* state);
void lit_ast_destroy_allocdexprlist(LitState* state, LitExprList* expressions);

LitExprList* lit_ast_allocate_stmtlist(LitState* state);
void lit_ast_destry_allocdstmtlist(LitState* state, LitExprList* statements);
void lit_emitter_init(LitState* state, LitEmitter* emitter);
void lit_emitter_destroy(LitEmitter* emitter);

LitModule* lit_emitter_modemit(LitEmitter* emitter, LitExprList* statements, LitString* module_name);

void lit_init_parser(LitState* state, LitParser* parser);
void lit_free_parser(LitParser* parser);



