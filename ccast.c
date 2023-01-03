
#include "lit.h"
#include "priv.h"

void lit_exprlist_init(LitExprList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_exprlist_destroy(LitState* state, LitExprList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitExpression*), array->values, array->capacity);
    lit_exprlist_init(array);
}

void lit_exprlist_push(LitState* state, LitExprList* array, LitExpression* value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitExpression*), old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void lit_paramlist_init(LitParamList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_paramlist_destroy(LitState* state, LitParamList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitParameter), array->values, array->capacity);
    lit_paramlist_init(array);
}

void lit_paramlist_push(LitState* state, LitParamList* array, LitParameter value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitParameter), old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void lit_paramlist_destroyvalues(LitState* state, LitParamList* parameters)
{
    size_t i;
    for(i = 0; i < parameters->count; i++)
    {
        lit_ast_destroyexpression(state, parameters->values[i].default_value);
    }

    lit_paramlist_destroy(state, parameters);
}

void lit_ast_destroyexprlist(LitState* state, LitExprList* expressions)
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

void lit_ast_destroystmtlist(LitState* state, LitExprList* statements)
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

void lit_ast_destroyexpression(LitState* state, LitExpression* expression)
{
    if(expression == NULL)
    {
        return;
    }

    switch(expression->type)
    {
        case LITEXPR_LITERAL:
        {
            lit_gcmem_memrealloc(state, expression, sizeof(LitLiteralExpr), 0);
            break;
        }

        case LITEXPR_BINARY:
        {
            LitBinaryExpr* expr = (LitBinaryExpr*)expression;

            if(!expr->ignore_left)
            {
                lit_ast_destroyexpression(state, expr->left);
            }

            lit_ast_destroyexpression(state, expr->right);

            lit_gcmem_memrealloc(state, expression, sizeof(LitBinaryExpr), 0);
            break;
        }

        case LITEXPR_UNARY:
        {
            lit_ast_destroyexpression(state, ((LitUnaryExpr*)expression)->right);
            lit_gcmem_memrealloc(state, expression, sizeof(LitUnaryExpr), 0);

            break;
        }

        case LITEXPR_VAREXPR:
        {
            lit_gcmem_memrealloc(state, expression, sizeof(LitVarExpr), 0);
            break;
        }

        case LITEXPR_ASSIGN:
        {
            LitAssignExpr* expr = (LitAssignExpr*)expression;

            lit_ast_destroyexpression(state, expr->to);
            lit_ast_destroyexpression(state, expr->value);

            lit_gcmem_memrealloc(state, expression, sizeof(LitAssignExpr), 0);
            break;
        }

        case LITEXPR_CALL:
        {
            LitCallExpr* expr = (LitCallExpr*)expression;

            lit_ast_destroyexpression(state, expr->callee);
            lit_ast_destroyexpression(state, expr->init);

            lit_ast_destroyexprlist(state, &expr->args);

            lit_gcmem_memrealloc(state, expression, sizeof(LitCallExpr), 0);
            break;
        }

        case LITEXPR_GET:
        {
            lit_ast_destroyexpression(state, ((LitGetExpr*)expression)->where);
            lit_gcmem_memrealloc(state, expression, sizeof(LitGetExpr), 0);
            break;
        }

        case LITEXPR_SET:
        {
            LitSetExpr* expr = (LitSetExpr*)expression;

            lit_ast_destroyexpression(state, expr->where);
            lit_ast_destroyexpression(state, expr->value);

            lit_gcmem_memrealloc(state, expression, sizeof(LitSetExpr), 0);
            break;
        }

        case LITEXPR_LAMBDA:
            {
                LitLambdaExpr* expr = (LitLambdaExpr*)expression;
                lit_paramlist_destroyvalues(state, &expr->parameters);
                lit_ast_destroyexpression(state, expr->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitLambdaExpr), 0);
            }
            break;
        case LITEXPR_ARRAY:
            {
                lit_ast_destroyexprlist(state, &((LitArrayExpr*)expression)->values);
                lit_gcmem_memrealloc(state, expression, sizeof(LitArrayExpr), 0);
            }
            break;
        case LITEXPR_OBJECT:
            {
                LitObjectExpr* map = (LitObjectExpr*)expression;
                lit_vallist_destroy(state, &map->keys);
                lit_ast_destroyexprlist(state, &map->values);
                lit_gcmem_memrealloc(state, expression, sizeof(LitObjectExpr), 0);
            }
            break;
        case LITEXPR_SUBSCRIPT:
            {
                LitSubscriptExpr* expr = (LitSubscriptExpr*)expression;
                lit_ast_destroyexpression(state, expr->array);
                lit_ast_destroyexpression(state, expr->index);
                lit_gcmem_memrealloc(state, expression, sizeof(LitSubscriptExpr), 0);
            }
            break;
        case LITEXPR_THIS:
            {
                lit_gcmem_memrealloc(state, expression, sizeof(LitThisExpr), 0);
            }
            break;
        case LITEXPR_SUPER:
            {
                lit_gcmem_memrealloc(state, expression, sizeof(LitSuperExpr), 0);
            }
            break;
        case LITEXPR_RANGE:
            {
                LitRangeExpr* expr = (LitRangeExpr*)expression;
                lit_ast_destroyexpression(state, expr->from);
                lit_ast_destroyexpression(state, expr->to);
                lit_gcmem_memrealloc(state, expression, sizeof(LitRangeExpr), 0);
            }
            break;
        case LITEXPR_TERNARY:
            {
                LitTernaryExpr* expr = (LitTernaryExpr*)expression;
                lit_ast_destroyexpression(state, expr->condition);
                lit_ast_destroyexpression(state, expr->if_branch);
                lit_ast_destroyexpression(state, expr->else_branch);
                lit_gcmem_memrealloc(state, expression, sizeof(LitTernaryExpr), 0);
            }
            break;
        case LITEXPR_INTERPOLATION:
            {
                lit_ast_destroyexprlist(state, &((LitInterpolationExpr*)expression)->expressions);
                lit_gcmem_memrealloc(state, expression, sizeof(LitInterpolationExpr), 0);
            }
            break;
        case LITEXPR_REFERENCE:
            {
                lit_ast_destroyexpression(state, ((LitReferenceExpr*)expression)->to);
                lit_gcmem_memrealloc(state, expression, sizeof(LitReferenceExpr), 0);
            }
            break;
        case LITEXPR_EXPRESSION:
            {
                lit_ast_destroyexpression(state, ((LitExpressionExpr*)expression)->expression);
                lit_gcmem_memrealloc(state, expression, sizeof(LitExpressionExpr), 0);
            }
            break;
        case LITEXPR_BLOCK:
            {
                lit_ast_destroystmtlist(state, &((LitBlockExpr*)expression)->statements);
                lit_gcmem_memrealloc(state, expression, sizeof(LitBlockExpr), 0);
            }
            break;
        case LITEXPR_VARSTMT:
            {
                lit_ast_destroyexpression(state, ((LitAssignVarExpr*)expression)->init);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAssignVarExpr), 0);
            }
            break;
        case LITEXPR_IFSTMT:
            {
                LitIfExpr* stmt = (LitIfExpr*)expression;
                lit_ast_destroyexpression(state, stmt->condition);
                lit_ast_destroyexpression(state, stmt->if_branch);
                lit_ast_destroy_allocdexprlist(state, stmt->elseif_conditions);
                lit_ast_destry_allocdstmtlist(state, stmt->elseif_branches);
                lit_ast_destroyexpression(state, stmt->else_branch);
                lit_gcmem_memrealloc(state, expression, sizeof(LitIfExpr), 0);
            }
            break;
        case LITEXPR_WHILE:
            {
                LitWhileExpr* stmt = (LitWhileExpr*)expression;
                lit_ast_destroyexpression(state, stmt->condition);
                lit_ast_destroyexpression(state, stmt->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitWhileExpr), 0);
            }
            break;
        case LITEXPR_FOR:
            {
                LitForExpr* stmt = (LitForExpr*)expression;
                lit_ast_destroyexpression(state, stmt->increment);
                lit_ast_destroyexpression(state, stmt->condition);
                lit_ast_destroyexpression(state, stmt->init);

                lit_ast_destroyexpression(state, stmt->var);
                lit_ast_destroyexpression(state, stmt->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitForExpr), 0);
            }
            break;
        case LITEXPR_CONTINUE:
            {
                lit_gcmem_memrealloc(state, expression, sizeof(LitContinueExpr), 0);
            }
            break;
        case LITEXPR_BREAK:
            {
                lit_gcmem_memrealloc(state, expression, sizeof(LitBreakExpr), 0);
            }
            break;
        case LITEXPR_FUNCTION:
            {
                LitFunctionExpr* stmt = (LitFunctionExpr*)expression;
                lit_ast_destroyexpression(state, stmt->body);
                lit_paramlist_destroyvalues(state, &stmt->parameters);
                lit_gcmem_memrealloc(state, expression, sizeof(LitFunctionExpr), 0);
            }
            break;
        case LITEXPR_RETURN:
            {
                lit_ast_destroyexpression(state, ((LitReturnExpr*)expression)->expression);
                lit_gcmem_memrealloc(state, expression, sizeof(LitReturnExpr), 0);
            }
            break;
        case LITEXPR_METHOD:
            {
                LitMethodExpr* stmt = (LitMethodExpr*)expression;
                lit_paramlist_destroyvalues(state, &stmt->parameters);
                lit_ast_destroyexpression(state, stmt->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitMethodExpr), 0);
            }
            break;
        case LITEXPR_CLASS:
            {
                lit_ast_destroystmtlist(state, &((LitClassExpr*)expression)->fields);
                lit_gcmem_memrealloc(state, expression, sizeof(LitClassExpr), 0);
            }
            break;
        case LITEXPR_FIELD:
            {
                LitFieldExpr* stmt = (LitFieldExpr*)expression;
                lit_ast_destroyexpression(state, stmt->getter);
                lit_ast_destroyexpression(state, stmt->setter);
                lit_gcmem_memrealloc(state, expression, sizeof(LitFieldExpr), 0);
            }
            break;
        default:
            {
                lit_state_raiseerror(state, COMPILE_ERROR, "Unknown expression type %d", (int)expression->type);
            }
            break;
    }
}

static LitExpression* lit_ast_allocexpr(LitState* state, uint64_t line, size_t size, LitExprType type)
{
    LitExpression* object;
    object = (LitExpression*)lit_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

LitLiteralExpr* lit_ast_make_literalexpr(LitState* state, size_t line, LitValue value)
{
    LitLiteralExpr* expression;
    expression = (LitLiteralExpr*)lit_ast_allocexpr(state, line, sizeof(LitLiteralExpr), LITEXPR_LITERAL);
    expression->value = value;
    return expression;
}

LitBinaryExpr* lit_ast_make_binaryexpr(LitState* state, size_t line, LitExpression* left, LitExpression* right, LitTokType op)
{
    LitBinaryExpr* expression;
    expression = (LitBinaryExpr*)lit_ast_allocexpr(state, line, sizeof(LitBinaryExpr), LITEXPR_BINARY);
    expression->left = left;
    expression->right = right;
    expression->op = op;
    expression->ignore_left = false;
    return expression;
}

LitUnaryExpr* lit_ast_make_unaryexpr(LitState* state, size_t line, LitExpression* right, LitTokType op)
{
    LitUnaryExpr* expression;
    expression = (LitUnaryExpr*)lit_ast_allocexpr(state, line, sizeof(LitUnaryExpr), LITEXPR_UNARY);
    expression->right = right;
    expression->op = op;
    return expression;
}

LitVarExpr* lit_ast_make_varexpr(LitState* state, size_t line, const char* name, size_t length)
{
    LitVarExpr* expression;
    expression = (LitVarExpr*)lit_ast_allocexpr(state, line, sizeof(LitVarExpr), LITEXPR_VAREXPR);
    expression->name = name;
    expression->length = length;
    return expression;
}

LitAssignExpr* lit_ast_make_assignexpr(LitState* state, size_t line, LitExpression* to, LitExpression* value)
{
    LitAssignExpr* expression;
    expression = (LitAssignExpr*)lit_ast_allocexpr(state, line, sizeof(LitAssignExpr), LITEXPR_ASSIGN);
    expression->to = to;
    expression->value = value;
    return expression;
}

LitCallExpr* lit_ast_make_callexpr(LitState* state, size_t line, LitExpression* callee)
{
    LitCallExpr* expression;
    expression = (LitCallExpr*)lit_ast_allocexpr(state, line, sizeof(LitCallExpr), LITEXPR_CALL);
    expression->callee = callee;
    expression->init = NULL;
    lit_exprlist_init(&expression->args);
    return expression;
}

LitGetExpr* lit_ast_make_getexpr(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, bool questionable, bool ignore_result)
{
    LitGetExpr* expression;
    expression = (LitGetExpr*)lit_ast_allocexpr(state, line, sizeof(LitGetExpr), LITEXPR_GET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->ignore_emit = false;
    expression->jump = questionable ? 0 : -1;
    expression->ignore_result = ignore_result;
    return expression;
}

LitSetExpr* lit_ast_make_setexpr(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, LitExpression* value)
{
    LitSetExpr* expression;
    expression = (LitSetExpr*)lit_ast_allocexpr(state, line, sizeof(LitSetExpr), LITEXPR_SET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->value = value;
    return expression;
}

LitLambdaExpr* lit_ast_make_lambdaexpr(LitState* state, size_t line)
{
    LitLambdaExpr* expression;
    expression = (LitLambdaExpr*)lit_ast_allocexpr(state, line, sizeof(LitLambdaExpr), LITEXPR_LAMBDA);
    expression->body = NULL;
    lit_paramlist_init(&expression->parameters);
    return expression;
}

LitArrayExpr* lit_ast_make_arrayexpr(LitState* state, size_t line)
{
    LitArrayExpr* expression;
    expression = (LitArrayExpr*)lit_ast_allocexpr(state, line, sizeof(LitArrayExpr), LITEXPR_ARRAY);
    lit_exprlist_init(&expression->values);
    return expression;
}

LitObjectExpr* lit_ast_make_objectexpr(LitState* state, size_t line)
{
    LitObjectExpr* expression;
    expression = (LitObjectExpr*)lit_ast_allocexpr(state, line, sizeof(LitObjectExpr), LITEXPR_OBJECT);
    lit_vallist_init(&expression->keys);
    lit_exprlist_init(&expression->values);
    return expression;
}

LitSubscriptExpr* lit_ast_make_subscriptexpr(LitState* state, size_t line, LitExpression* array, LitExpression* index)
{
    LitSubscriptExpr* expression;
    expression = (LitSubscriptExpr*)lit_ast_allocexpr(state, line, sizeof(LitSubscriptExpr), LITEXPR_SUBSCRIPT);
    expression->array = array;
    expression->index = index;
    return expression;
}

LitThisExpr* lit_ast_make_thisexpr(LitState* state, size_t line)
{
    return (LitThisExpr*)lit_ast_allocexpr(state, line, sizeof(LitThisExpr), LITEXPR_THIS);
}

LitSuperExpr* lit_ast_make_superexpr(LitState* state, size_t line, LitString* method, bool ignore_result)
{
    LitSuperExpr* expression;
    expression = (LitSuperExpr*)lit_ast_allocexpr(state, line, sizeof(LitSuperExpr), LITEXPR_SUPER);
    expression->method = method;
    expression->ignore_emit = false;
    expression->ignore_result = ignore_result;
    return expression;
}

LitRangeExpr* lit_ast_make_rangeexpr(LitState* state, size_t line, LitExpression* from, LitExpression* to)
{
    LitRangeExpr* expression;
    expression = (LitRangeExpr*)lit_ast_allocexpr(state, line, sizeof(LitRangeExpr), LITEXPR_RANGE);
    expression->from = from;
    expression->to = to;
    return expression;
}

LitTernaryExpr* lit_ast_make_ternaryexpr(LitState* state, size_t line, LitExpression* condition, LitExpression* if_branch, LitExpression* else_branch)
{
    LitTernaryExpr* expression;
    expression = (LitTernaryExpr*)lit_ast_allocexpr(state, line, sizeof(LitTernaryExpr), LITEXPR_TERNARY);
    expression->condition = condition;
    expression->if_branch = if_branch;
    expression->else_branch = else_branch;

    return expression;
}

LitInterpolationExpr* lit_ast_make_strinterpolexpr(LitState* state, size_t line)
{
    LitInterpolationExpr* expression;
    expression = (LitInterpolationExpr*)lit_ast_allocexpr(state, line, sizeof(LitInterpolationExpr), LITEXPR_INTERPOLATION);
    lit_exprlist_init(&expression->expressions);
    return expression;
}

LitReferenceExpr* lit_ast_make_referenceexpr(LitState* state, size_t line, LitExpression* to)
{
    LitReferenceExpr* expression;
    expression = (LitReferenceExpr*)lit_ast_allocexpr(state, line, sizeof(LitReferenceExpr), LITEXPR_REFERENCE);
    expression->to = to;
    return expression;
}



static LitExpression* lit_ast_allocstmt(LitState* state, uint64_t line, size_t size, LitExprType type)
{
    LitExpression* object;
    object = (LitExpression*)lit_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

LitExpressionExpr* lit_ast_make_exprstmt(LitState* state, size_t line, LitExpression* expression)
{
    LitExpressionExpr* statement;
    statement = (LitExpressionExpr*)lit_ast_allocstmt(state, line, sizeof(LitExpressionExpr), LITEXPR_EXPRESSION);
    statement->expression = expression;
    statement->pop = true;
    return statement;
}

LitBlockExpr* lit_ast_make_blockexpr(LitState* state, size_t line)
{
    LitBlockExpr* statement;
    statement = (LitBlockExpr*)lit_ast_allocstmt(state, line, sizeof(LitBlockExpr), LITEXPR_BLOCK);
    lit_exprlist_init(&statement->statements);
    return statement;
}

LitAssignVarExpr* lit_ast_make_assignvarexpr(LitState* state, size_t line, const char* name, size_t length, LitExpression* init, bool constant)
{
    LitAssignVarExpr* statement;
    statement = (LitAssignVarExpr*)lit_ast_allocstmt(state, line, sizeof(LitAssignVarExpr), LITEXPR_VARSTMT);
    statement->name = name;
    statement->length = length;
    statement->init = init;
    statement->constant = constant;
    return statement;
}

LitIfExpr* lit_ast_make_ifexpr(LitState* state,
                                        size_t line,
                                        LitExpression* condition,
                                        LitExpression* if_branch,
                                        LitExpression* else_branch,
                                        LitExprList* elseif_conditions,
                                        LitExprList* elseif_branches)
{
    LitIfExpr* statement;
    statement = (LitIfExpr*)lit_ast_allocstmt(state, line, sizeof(LitIfExpr), LITEXPR_IFSTMT);
    statement->condition = condition;
    statement->if_branch = if_branch;
    statement->else_branch = else_branch;
    statement->elseif_conditions = elseif_conditions;
    statement->elseif_branches = elseif_branches;
    return statement;
}

LitWhileExpr* lit_ast_make_whileexpr(LitState* state, size_t line, LitExpression* condition, LitExpression* body)
{
    LitWhileExpr* statement;
    statement = (LitWhileExpr*)lit_ast_allocstmt(state, line, sizeof(LitWhileExpr), LITEXPR_WHILE);
    statement->condition = condition;
    statement->body = body;
    return statement;
}

LitForExpr* lit_ast_make_forexpr(LitState* state,
                                          size_t line,
                                          LitExpression* init,
                                          LitExpression* var,
                                          LitExpression* condition,
                                          LitExpression* increment,
                                          LitExpression* body,
                                          bool c_style)
{
    LitForExpr* statement;
    statement = (LitForExpr*)lit_ast_allocstmt(state, line, sizeof(LitForExpr), LITEXPR_FOR);
    statement->init = init;
    statement->var = var;
    statement->condition = condition;
    statement->increment = increment;
    statement->body = body;
    statement->c_style = c_style;
    return statement;
}

LitContinueExpr* lit_ast_make_continueexpr(LitState* state, size_t line)
{
    return (LitContinueExpr*)lit_ast_allocstmt(state, line, sizeof(LitContinueExpr), LITEXPR_CONTINUE);
}

LitBreakExpr* lit_ast_make_breakexpr(LitState* state, size_t line)
{
    return (LitBreakExpr*)lit_ast_allocstmt(state, line, sizeof(LitBreakExpr), LITEXPR_BREAK);
}

LitFunctionExpr* lit_ast_make_funcexpr(LitState* state, size_t line, const char* name, size_t length)
{
    LitFunctionExpr* function;
    function = (LitFunctionExpr*)lit_ast_allocstmt(state, line, sizeof(LitFunctionExpr), LITEXPR_FUNCTION);
    function->name = name;
    function->length = length;
    function->body = NULL;
    lit_paramlist_init(&function->parameters);
    return function;
}

LitReturnExpr* lit_ast_make_returnexpr(LitState* state, size_t line, LitExpression* expression)
{
    LitReturnExpr* statement;
    statement = (LitReturnExpr*)lit_ast_allocstmt(state, line, sizeof(LitReturnExpr), LITEXPR_RETURN);
    statement->expression = expression;
    return statement;
}

LitMethodExpr* lit_ast_make_methodexpr(LitState* state, size_t line, LitString* name, bool is_static)
{
    LitMethodExpr* statement;
    statement = (LitMethodExpr*)lit_ast_allocstmt(state, line, sizeof(LitMethodExpr), LITEXPR_METHOD);
    statement->name = name;
    statement->body = NULL;
    statement->is_static = is_static;
    lit_paramlist_init(&statement->parameters);
    return statement;
}

LitClassExpr* lit_ast_make_classexpr(LitState* state, size_t line, LitString* name, LitString* parent)
{
    LitClassExpr* statement;
    statement = (LitClassExpr*)lit_ast_allocstmt(state, line, sizeof(LitClassExpr), LITEXPR_CLASS);
    statement->name = name;
    statement->parent = parent;
    lit_exprlist_init(&statement->fields);
    return statement;
}

LitFieldExpr* lit_ast_make_fieldexpr(LitState* state, size_t line, LitString* name, LitExpression* getter, LitExpression* setter, bool is_static)
{
    LitFieldExpr* statement;
    statement = (LitFieldExpr*)lit_ast_allocstmt(state, line, sizeof(LitFieldExpr), LITEXPR_FIELD);
    statement->name = name;
    statement->getter = getter;
    statement->setter = setter;
    statement->is_static = is_static;
    return statement;
}

LitExprList* lit_ast_allocexprlist(LitState* state)
{
    LitExprList* expressions;
    expressions = (LitExprList*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(LitExprList));
    lit_exprlist_init(expressions);
    return expressions;
}

void lit_ast_destroy_allocdexprlist(LitState* state, LitExprList* expressions)
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
    lit_gcmem_memrealloc(state, expressions, sizeof(LitExprList), 0);
}

LitExprList* lit_ast_allocate_stmtlist(LitState* state)
{
    LitExprList* statements;
    statements = (LitExprList*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(LitExprList));
    lit_exprlist_init(statements);
    return statements;
}

void lit_ast_destry_allocdstmtlist(LitState* state, LitExprList* statements)
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
    lit_gcmem_memrealloc(state, statements, sizeof(LitExprList), 0);
}
