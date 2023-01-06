
#include "lit.h"

void lit_exprlist_init(LitAstExprList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_exprlist_destroy(LitState* state, LitAstExprList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitAstExpression*), array->values, array->capacity);
    lit_exprlist_init(array);
}

void lit_exprlist_push(LitState* state, LitAstExprList* array, LitAstExpression* value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitAstExpression*), old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void lit_paramlist_init(LitAstParamList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_paramlist_destroy(LitState* state, LitAstParamList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitAstParameter), array->values, array->capacity);
    lit_paramlist_init(array);
}

void lit_paramlist_push(LitState* state, LitAstParamList* array, LitAstParameter value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitAstParameter), old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void lit_paramlist_destroyvalues(LitState* state, LitAstParamList* parameters)
{
    size_t i;
    for(i = 0; i < parameters->count; i++)
    {
        lit_ast_destroyexpression(state, parameters->values[i].default_value);
    }

    lit_paramlist_destroy(state, parameters);
}

void lit_ast_destroyexprlist(LitState* state, LitAstExprList* expressions)
{
    if(expressions == NULL)
    {
        return;
    }

    for(size_t i = 0; i < expressions->count; i++)
    {
        lit_ast_destroyexpression(state, expressions->values[i]);
    }

    lit_exprlist_destroy(state, expressions);
}

void lit_ast_destroystmtlist(LitState* state, LitAstExprList* statements)
{
    if(statements == NULL)
    {
        return;
    }

    for(size_t i = 0; i < statements->count; i++)
    {
        lit_ast_destroyexpression(state, statements->values[i]);
    }

    lit_exprlist_destroy(state, statements);
}

void lit_ast_destroyexpression(LitState* state, LitAstExpression* expression)
{
    if(expression == NULL)
    {
        return;
    }

    switch(expression->type)
    {
        case LITEXPR_LITERAL:
        {
            lit_gcmem_memrealloc(state, expression, sizeof(LitAstLiteralExpr), 0);
            break;
        }

        case LITEXPR_BINARY:
        {
            LitAstBinaryExpr* expr = (LitAstBinaryExpr*)expression;

            if(!expr->ignore_left)
            {
                lit_ast_destroyexpression(state, expr->left);
            }

            lit_ast_destroyexpression(state, expr->right);

            lit_gcmem_memrealloc(state, expression, sizeof(LitAstBinaryExpr), 0);
            break;
        }

        case LITEXPR_UNARY:
        {
            lit_ast_destroyexpression(state, ((LitAstUnaryExpr*)expression)->right);
            lit_gcmem_memrealloc(state, expression, sizeof(LitAstUnaryExpr), 0);

            break;
        }

        case LITEXPR_VAREXPR:
        {
            lit_gcmem_memrealloc(state, expression, sizeof(LitAstVarExpr), 0);
            break;
        }

        case LITEXPR_ASSIGN:
        {
            LitAstAssignExpr* expr = (LitAstAssignExpr*)expression;

            lit_ast_destroyexpression(state, expr->to);
            lit_ast_destroyexpression(state, expr->value);

            lit_gcmem_memrealloc(state, expression, sizeof(LitAstAssignExpr), 0);
            break;
        }

        case LITEXPR_CALL:
        {
            LitAstCallExpr* expr = (LitAstCallExpr*)expression;

            lit_ast_destroyexpression(state, expr->callee);
            lit_ast_destroyexpression(state, expr->init);

            lit_ast_destroyexprlist(state, &expr->args);

            lit_gcmem_memrealloc(state, expression, sizeof(LitAstCallExpr), 0);
            break;
        }

        case LITEXPR_GET:
        {
            lit_ast_destroyexpression(state, ((LitAstGetExpr*)expression)->where);
            lit_gcmem_memrealloc(state, expression, sizeof(LitAstGetExpr), 0);
            break;
        }

        case LITEXPR_SET:
        {
            LitAstSetExpr* expr = (LitAstSetExpr*)expression;

            lit_ast_destroyexpression(state, expr->where);
            lit_ast_destroyexpression(state, expr->value);

            lit_gcmem_memrealloc(state, expression, sizeof(LitAstSetExpr), 0);
            break;
        }

        case LITEXPR_LAMBDA:
            {
                LitAstLambdaExpr* expr = (LitAstLambdaExpr*)expression;
                lit_paramlist_destroyvalues(state, &expr->parameters);
                lit_ast_destroyexpression(state, expr->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstLambdaExpr), 0);
            }
            break;
        case LITEXPR_ARRAY:
            {
                lit_ast_destroyexprlist(state, &((LitAstArrayExpr*)expression)->values);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstArrayExpr), 0);
            }
            break;
        case LITEXPR_OBJECT:
            {
                LitAstObjectExpr* map = (LitAstObjectExpr*)expression;
                lit_vallist_destroy(state, &map->keys);
                lit_ast_destroyexprlist(state, &map->values);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstObjectExpr), 0);
            }
            break;
        case LITEXPR_SUBSCRIPT:
            {
                LitAstIndexExpr* expr = (LitAstIndexExpr*)expression;
                lit_ast_destroyexpression(state, expr->array);
                lit_ast_destroyexpression(state, expr->index);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstIndexExpr), 0);
            }
            break;
        case LITEXPR_THIS:
            {
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstThisExpr), 0);
            }
            break;
        case LITEXPR_SUPER:
            {
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstSuperExpr), 0);
            }
            break;
        case LITEXPR_RANGE:
            {
                LitAstRangeExpr* expr = (LitAstRangeExpr*)expression;
                lit_ast_destroyexpression(state, expr->from);
                lit_ast_destroyexpression(state, expr->to);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstRangeExpr), 0);
            }
            break;
        case LITEXPR_TERNARY:
            {
                LitAstTernaryExpr* expr = (LitAstTernaryExpr*)expression;
                lit_ast_destroyexpression(state, expr->condition);
                lit_ast_destroyexpression(state, expr->if_branch);
                lit_ast_destroyexpression(state, expr->else_branch);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstTernaryExpr), 0);
            }
            break;
        case LITEXPR_INTERPOLATION:
            {
                lit_ast_destroyexprlist(state, &((LitAstStrInterExpr*)expression)->expressions);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstStrInterExpr), 0);
            }
            break;
        case LITEXPR_REFERENCE:
            {
                lit_ast_destroyexpression(state, ((LitAstRefExpr*)expression)->to);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstRefExpr), 0);
            }
            break;
        case LITEXPR_EXPRESSION:
            {
                lit_ast_destroyexpression(state, ((LitAstExprExpr*)expression)->expression);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstExprExpr), 0);
            }
            break;
        case LITEXPR_BLOCK:
            {
                lit_ast_destroystmtlist(state, &((LitAstBlockExpr*)expression)->statements);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstBlockExpr), 0);
            }
            break;
        case LITEXPR_VARSTMT:
            {
                lit_ast_destroyexpression(state, ((LitAstAssignVarExpr*)expression)->init);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstAssignVarExpr), 0);
            }
            break;
        case LITEXPR_IFSTMT:
            {
                LitAstIfExpr* stmt = (LitAstIfExpr*)expression;
                lit_ast_destroyexpression(state, stmt->condition);
                lit_ast_destroyexpression(state, stmt->if_branch);
                lit_ast_destroy_allocdexprlist(state, stmt->elseif_conditions);
                lit_ast_destry_allocdstmtlist(state, stmt->elseif_branches);
                lit_ast_destroyexpression(state, stmt->else_branch);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstIfExpr), 0);
            }
            break;
        case LITEXPR_WHILE:
            {
                LitAstWhileExpr* stmt = (LitAstWhileExpr*)expression;
                lit_ast_destroyexpression(state, stmt->condition);
                lit_ast_destroyexpression(state, stmt->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstWhileExpr), 0);
            }
            break;
        case LITEXPR_FOR:
            {
                LitAstForExpr* stmt = (LitAstForExpr*)expression;
                lit_ast_destroyexpression(state, stmt->increment);
                lit_ast_destroyexpression(state, stmt->condition);
                lit_ast_destroyexpression(state, stmt->init);

                lit_ast_destroyexpression(state, stmt->var);
                lit_ast_destroyexpression(state, stmt->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstForExpr), 0);
            }
            break;
        case LITEXPR_CONTINUE:
            {
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstContinueExpr), 0);
            }
            break;
        case LITEXPR_BREAK:
            {
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstBreakExpr), 0);
            }
            break;
        case LITEXPR_FUNCTION:
            {
                LitAstFunctionExpr* stmt = (LitAstFunctionExpr*)expression;
                lit_ast_destroyexpression(state, stmt->body);
                lit_paramlist_destroyvalues(state, &stmt->parameters);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstFunctionExpr), 0);
            }
            break;
        case LITEXPR_RETURN:
            {
                lit_ast_destroyexpression(state, ((LitAstReturnExpr*)expression)->expression);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstReturnExpr), 0);
            }
            break;
        case LITEXPR_METHOD:
            {
                LitAstMethodExpr* stmt = (LitAstMethodExpr*)expression;
                lit_paramlist_destroyvalues(state, &stmt->parameters);
                lit_ast_destroyexpression(state, stmt->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstMethodExpr), 0);
            }
            break;
        case LITEXPR_CLASS:
            {
                lit_ast_destroystmtlist(state, &((LitAstClassExpr*)expression)->fields);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstClassExpr), 0);
            }
            break;
        case LITEXPR_FIELD:
            {
                LitAstFieldExpr* stmt = (LitAstFieldExpr*)expression;
                lit_ast_destroyexpression(state, stmt->getter);
                lit_ast_destroyexpression(state, stmt->setter);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAstFieldExpr), 0);
            }
            break;
        default:
            {
                lit_state_raiseerror(state, COMPILE_ERROR, "Unknown expression type %d", (int)expression->type);
            }
            break;
    }
}

static LitAstExpression* lit_ast_allocexpr(LitState* state, uint64_t line, size_t size, LitExprType type)
{
    LitAstExpression* object;
    object = (LitAstExpression*)lit_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

LitAstLiteralExpr* lit_ast_make_literalexpr(LitState* state, size_t line, LitValue value)
{
    LitAstLiteralExpr* expression;
    expression = (LitAstLiteralExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstLiteralExpr), LITEXPR_LITERAL);
    expression->value = value;
    return expression;
}

LitAstBinaryExpr* lit_ast_make_binaryexpr(LitState* state, size_t line, LitAstExpression* left, LitAstExpression* right, LitTokType op)
{
    LitAstBinaryExpr* expression;
    expression = (LitAstBinaryExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstBinaryExpr), LITEXPR_BINARY);
    expression->left = left;
    expression->right = right;
    expression->op = op;
    expression->ignore_left = false;
    return expression;
}

LitAstUnaryExpr* lit_ast_make_unaryexpr(LitState* state, size_t line, LitAstExpression* right, LitTokType op)
{
    LitAstUnaryExpr* expression;
    expression = (LitAstUnaryExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstUnaryExpr), LITEXPR_UNARY);
    expression->right = right;
    expression->op = op;
    return expression;
}

LitAstVarExpr* lit_ast_make_varexpr(LitState* state, size_t line, const char* name, size_t length)
{
    LitAstVarExpr* expression;
    expression = (LitAstVarExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstVarExpr), LITEXPR_VAREXPR);
    expression->name = name;
    expression->length = length;
    return expression;
}

LitAstAssignExpr* lit_ast_make_assignexpr(LitState* state, size_t line, LitAstExpression* to, LitAstExpression* value)
{
    LitAstAssignExpr* expression;
    expression = (LitAstAssignExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstAssignExpr), LITEXPR_ASSIGN);
    expression->to = to;
    expression->value = value;
    return expression;
}

LitAstCallExpr* lit_ast_make_callexpr(LitState* state, size_t line, LitAstExpression* callee)
{
    LitAstCallExpr* expression;
    expression = (LitAstCallExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstCallExpr), LITEXPR_CALL);
    expression->callee = callee;
    expression->init = NULL;
    lit_exprlist_init(&expression->args);
    return expression;
}

LitAstGetExpr* lit_ast_make_getexpr(LitState* state, size_t line, LitAstExpression* where, const char* name, size_t length, bool questionable, bool ignore_result)
{
    LitAstGetExpr* expression;
    expression = (LitAstGetExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstGetExpr), LITEXPR_GET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->ignore_emit = false;
    expression->jump = questionable ? 0 : -1;
    expression->ignore_result = ignore_result;
    return expression;
}

LitAstSetExpr* lit_ast_make_setexpr(LitState* state, size_t line, LitAstExpression* where, const char* name, size_t length, LitAstExpression* value)
{
    LitAstSetExpr* expression;
    expression = (LitAstSetExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstSetExpr), LITEXPR_SET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->value = value;
    return expression;
}

LitAstLambdaExpr* lit_ast_make_lambdaexpr(LitState* state, size_t line)
{
    LitAstLambdaExpr* expression;
    expression = (LitAstLambdaExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstLambdaExpr), LITEXPR_LAMBDA);
    expression->body = NULL;
    lit_paramlist_init(&expression->parameters);
    return expression;
}

LitAstArrayExpr* lit_ast_make_arrayexpr(LitState* state, size_t line)
{
    LitAstArrayExpr* expression;
    expression = (LitAstArrayExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstArrayExpr), LITEXPR_ARRAY);
    lit_exprlist_init(&expression->values);
    return expression;
}

LitAstObjectExpr* lit_ast_make_objectexpr(LitState* state, size_t line)
{
    LitAstObjectExpr* expression;
    expression = (LitAstObjectExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstObjectExpr), LITEXPR_OBJECT);
    lit_vallist_init(&expression->keys);
    lit_exprlist_init(&expression->values);
    return expression;
}

LitAstIndexExpr* lit_ast_make_subscriptexpr(LitState* state, size_t line, LitAstExpression* array, LitAstExpression* index)
{
    LitAstIndexExpr* expression;
    expression = (LitAstIndexExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstIndexExpr), LITEXPR_SUBSCRIPT);
    expression->array = array;
    expression->index = index;
    return expression;
}

LitAstThisExpr* lit_ast_make_thisexpr(LitState* state, size_t line)
{
    return (LitAstThisExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstThisExpr), LITEXPR_THIS);
}

LitAstSuperExpr* lit_ast_make_superexpr(LitState* state, size_t line, LitString* method, bool ignore_result)
{
    LitAstSuperExpr* expression;
    expression = (LitAstSuperExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstSuperExpr), LITEXPR_SUPER);
    expression->method = method;
    expression->ignore_emit = false;
    expression->ignore_result = ignore_result;
    return expression;
}

LitAstRangeExpr* lit_ast_make_rangeexpr(LitState* state, size_t line, LitAstExpression* from, LitAstExpression* to)
{
    LitAstRangeExpr* expression;
    expression = (LitAstRangeExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstRangeExpr), LITEXPR_RANGE);
    expression->from = from;
    expression->to = to;
    return expression;
}

LitAstTernaryExpr* lit_ast_make_ternaryexpr(LitState* state, size_t line, LitAstExpression* condition, LitAstExpression* if_branch, LitAstExpression* else_branch)
{
    LitAstTernaryExpr* expression;
    expression = (LitAstTernaryExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstTernaryExpr), LITEXPR_TERNARY);
    expression->condition = condition;
    expression->if_branch = if_branch;
    expression->else_branch = else_branch;

    return expression;
}

LitAstStrInterExpr* lit_ast_make_strinterpolexpr(LitState* state, size_t line)
{
    LitAstStrInterExpr* expression;
    expression = (LitAstStrInterExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstStrInterExpr), LITEXPR_INTERPOLATION);
    lit_exprlist_init(&expression->expressions);
    return expression;
}

LitAstRefExpr* lit_ast_make_referenceexpr(LitState* state, size_t line, LitAstExpression* to)
{
    LitAstRefExpr* expression;
    expression = (LitAstRefExpr*)lit_ast_allocexpr(state, line, sizeof(LitAstRefExpr), LITEXPR_REFERENCE);
    expression->to = to;
    return expression;
}



static LitAstExpression* lit_ast_allocstmt(LitState* state, uint64_t line, size_t size, LitExprType type)
{
    LitAstExpression* object;
    object = (LitAstExpression*)lit_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

LitAstExprExpr* lit_ast_make_exprstmt(LitState* state, size_t line, LitAstExpression* expression)
{
    LitAstExprExpr* statement;
    statement = (LitAstExprExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstExprExpr), LITEXPR_EXPRESSION);
    statement->expression = expression;
    statement->pop = true;
    return statement;
}

LitAstBlockExpr* lit_ast_make_blockexpr(LitState* state, size_t line)
{
    LitAstBlockExpr* statement;
    statement = (LitAstBlockExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstBlockExpr), LITEXPR_BLOCK);
    lit_exprlist_init(&statement->statements);
    return statement;
}

LitAstAssignVarExpr* lit_ast_make_assignvarexpr(LitState* state, size_t line, const char* name, size_t length, LitAstExpression* init, bool constant)
{
    LitAstAssignVarExpr* statement;
    statement = (LitAstAssignVarExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstAssignVarExpr), LITEXPR_VARSTMT);
    statement->name = name;
    statement->length = length;
    statement->init = init;
    statement->constant = constant;
    return statement;
}

LitAstIfExpr* lit_ast_make_ifexpr(LitState* state,
                                        size_t line,
                                        LitAstExpression* condition,
                                        LitAstExpression* if_branch,
                                        LitAstExpression* else_branch,
                                        LitAstExprList* elseif_conditions,
                                        LitAstExprList* elseif_branches)
{
    LitAstIfExpr* statement;
    statement = (LitAstIfExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstIfExpr), LITEXPR_IFSTMT);
    statement->condition = condition;
    statement->if_branch = if_branch;
    statement->else_branch = else_branch;
    statement->elseif_conditions = elseif_conditions;
    statement->elseif_branches = elseif_branches;
    return statement;
}

LitAstWhileExpr* lit_ast_make_whileexpr(LitState* state, size_t line, LitAstExpression* condition, LitAstExpression* body)
{
    LitAstWhileExpr* statement;
    statement = (LitAstWhileExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstWhileExpr), LITEXPR_WHILE);
    statement->condition = condition;
    statement->body = body;
    return statement;
}

LitAstForExpr* lit_ast_make_forexpr(LitState* state,
                                          size_t line,
                                          LitAstExpression* init,
                                          LitAstExpression* var,
                                          LitAstExpression* condition,
                                          LitAstExpression* increment,
                                          LitAstExpression* body,
                                          bool c_style)
{
    LitAstForExpr* statement;
    statement = (LitAstForExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstForExpr), LITEXPR_FOR);
    statement->init = init;
    statement->var = var;
    statement->condition = condition;
    statement->increment = increment;
    statement->body = body;
    statement->c_style = c_style;
    return statement;
}

LitAstContinueExpr* lit_ast_make_continueexpr(LitState* state, size_t line)
{
    return (LitAstContinueExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstContinueExpr), LITEXPR_CONTINUE);
}

LitAstBreakExpr* lit_ast_make_breakexpr(LitState* state, size_t line)
{
    return (LitAstBreakExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstBreakExpr), LITEXPR_BREAK);
}

LitAstFunctionExpr* lit_ast_make_funcexpr(LitState* state, size_t line, const char* name, size_t length)
{
    LitAstFunctionExpr* function;
    function = (LitAstFunctionExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstFunctionExpr), LITEXPR_FUNCTION);
    function->name = name;
    function->length = length;
    function->body = NULL;
    lit_paramlist_init(&function->parameters);
    return function;
}

LitAstReturnExpr* lit_ast_make_returnexpr(LitState* state, size_t line, LitAstExpression* expression)
{
    LitAstReturnExpr* statement;
    statement = (LitAstReturnExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstReturnExpr), LITEXPR_RETURN);
    statement->expression = expression;
    return statement;
}

LitAstMethodExpr* lit_ast_make_methodexpr(LitState* state, size_t line, LitString* name, bool is_static)
{
    LitAstMethodExpr* statement;
    statement = (LitAstMethodExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstMethodExpr), LITEXPR_METHOD);
    statement->name = name;
    statement->body = NULL;
    statement->is_static = is_static;
    lit_paramlist_init(&statement->parameters);
    return statement;
}

LitAstClassExpr* lit_ast_make_classexpr(LitState* state, size_t line, LitString* name, LitString* parent)
{
    LitAstClassExpr* statement;
    statement = (LitAstClassExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstClassExpr), LITEXPR_CLASS);
    statement->name = name;
    statement->parent = parent;
    lit_exprlist_init(&statement->fields);
    return statement;
}

LitAstFieldExpr* lit_ast_make_fieldexpr(LitState* state, size_t line, LitString* name, LitAstExpression* getter, LitAstExpression* setter, bool is_static)
{
    LitAstFieldExpr* statement;
    statement = (LitAstFieldExpr*)lit_ast_allocstmt(state, line, sizeof(LitAstFieldExpr), LITEXPR_FIELD);
    statement->name = name;
    statement->getter = getter;
    statement->setter = setter;
    statement->is_static = is_static;
    return statement;
}

LitAstExprList* lit_ast_allocexprlist(LitState* state)
{
    LitAstExprList* expressions;
    expressions = (LitAstExprList*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(LitAstExprList));
    lit_exprlist_init(expressions);
    return expressions;
}

void lit_ast_destroy_allocdexprlist(LitState* state, LitAstExprList* expressions)
{
    size_t i;
    if(expressions == NULL)
    {
        return;
    }
    for(i = 0; i < expressions->count; i++)
    {
        lit_ast_destroyexpression(state, expressions->values[i]);
    }
    lit_exprlist_destroy(state, expressions);
    lit_gcmem_memrealloc(state, expressions, sizeof(LitAstExprList), 0);
}

LitAstExprList* lit_ast_allocate_stmtlist(LitState* state)
{
    LitAstExprList* statements;
    statements = (LitAstExprList*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(LitAstExprList));
    lit_exprlist_init(statements);
    return statements;
}

void lit_ast_destry_allocdstmtlist(LitState* state, LitAstExprList* statements)
{
    size_t i;
    if(statements == NULL)
    {
        return;
    }
    for(i = 0; i < statements->count; i++)
    {
        lit_ast_destroyexpression(state, statements->values[i]);
    }
    lit_exprlist_destroy(state, statements);
    lit_gcmem_memrealloc(state, statements, sizeof(LitAstExprList), 0);
}
