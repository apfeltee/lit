
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

void lit_paramlist_freevalues(LitState* state, LitParamList* parameters)
{
    size_t i;
    for(i = 0; i < parameters->count; i++)
    {
        lit_free_expression(state, parameters->values[i].default_value);
    }

    lit_paramlist_destroy(state, parameters);
}

void free_expressions(LitState* state, LitExprList* expressions)
{
    if(expressions == NULL)
    {
        return;
    }

    for(size_t i = 0; i < expressions->count; i++)
    {
        lit_free_expression(state, expressions->values[i]);
    }

    lit_exprlist_destroy(state, expressions);
}

void internal_free_statements(LitState* state, LitExprList* statements)
{
    if(statements == NULL)
    {
        return;
    }

    for(size_t i = 0; i < statements->count; i++)
    {
        lit_free_expression(state, statements->values[i]);
    }

    lit_exprlist_destroy(state, statements);
}

void lit_free_expression(LitState* state, LitExpression* expression)
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
                lit_free_expression(state, expr->left);
            }

            lit_free_expression(state, expr->right);

            lit_gcmem_memrealloc(state, expression, sizeof(LitBinaryExpr), 0);
            break;
        }

        case LITEXPR_UNARY:
        {
            lit_free_expression(state, ((LitUnaryExpr*)expression)->right);
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

            lit_free_expression(state, expr->to);
            lit_free_expression(state, expr->value);

            lit_gcmem_memrealloc(state, expression, sizeof(LitAssignExpr), 0);
            break;
        }

        case LITEXPR_CALL:
        {
            LitCallExpr* expr = (LitCallExpr*)expression;

            lit_free_expression(state, expr->callee);
            lit_free_expression(state, expr->init);

            free_expressions(state, &expr->args);

            lit_gcmem_memrealloc(state, expression, sizeof(LitCallExpr), 0);
            break;
        }

        case LITEXPR_GET:
        {
            lit_free_expression(state, ((LitGetExpr*)expression)->where);
            lit_gcmem_memrealloc(state, expression, sizeof(LitGetExpr), 0);
            break;
        }

        case LITEXPR_SET:
        {
            LitSetExpr* expr = (LitSetExpr*)expression;

            lit_free_expression(state, expr->where);
            lit_free_expression(state, expr->value);

            lit_gcmem_memrealloc(state, expression, sizeof(LitSetExpr), 0);
            break;
        }

        case LITEXPR_LAMBDA:
            {
                LitLambdaExpr* expr = (LitLambdaExpr*)expression;
                lit_paramlist_freevalues(state, &expr->parameters);
                lit_free_expression(state, expr->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitLambdaExpr), 0);
            }
            break;
        case LITEXPR_ARRAY:
            {
                free_expressions(state, &((LitArrayExpr*)expression)->values);
                lit_gcmem_memrealloc(state, expression, sizeof(LitArrayExpr), 0);
            }
            break;
        case LITEXPR_OBJECT:
            {
                LitObjectExpr* map = (LitObjectExpr*)expression;
                lit_vallist_destroy(state, &map->keys);
                free_expressions(state, &map->values);
                lit_gcmem_memrealloc(state, expression, sizeof(LitObjectExpr), 0);
            }
            break;
        case LITEXPR_SUBSCRIPT:
            {
                LitSubscriptExpr* expr = (LitSubscriptExpr*)expression;
                lit_free_expression(state, expr->array);
                lit_free_expression(state, expr->index);
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
                lit_free_expression(state, expr->from);
                lit_free_expression(state, expr->to);
                lit_gcmem_memrealloc(state, expression, sizeof(LitRangeExpr), 0);
            }
            break;
        case LITEXPR_TERNARY:
            {
                LitTernaryExpr* expr = (LitTernaryExpr*)expression;
                lit_free_expression(state, expr->condition);
                lit_free_expression(state, expr->if_branch);
                lit_free_expression(state, expr->else_branch);
                lit_gcmem_memrealloc(state, expression, sizeof(LitTernaryExpr), 0);
            }
            break;
        case LITEXPR_INTERPOLATION:
            {
                free_expressions(state, &((LitInterpolationExpr*)expression)->expressions);
                lit_gcmem_memrealloc(state, expression, sizeof(LitInterpolationExpr), 0);
            }
            break;
        case LITEXPR_REFERENCE:
            {
                lit_free_expression(state, ((LitReferenceExpr*)expression)->to);
                lit_gcmem_memrealloc(state, expression, sizeof(LitReferenceExpr), 0);
            }
            break;
        case LITEXPR_EXPRESSION:
            {
                lit_free_expression(state, ((LitExpressionExpr*)expression)->expression);
                lit_gcmem_memrealloc(state, expression, sizeof(LitExpressionExpr), 0);
            }
            break;
        case LITEXPR_BLOCK:
            {
                internal_free_statements(state, &((LitBlockExpr*)expression)->statements);
                lit_gcmem_memrealloc(state, expression, sizeof(LitBlockExpr), 0);
            }
            break;
        case LITEXPR_VARSTMT:
            {
                lit_free_expression(state, ((LitAssignVarExpr*)expression)->init);
                lit_gcmem_memrealloc(state, expression, sizeof(LitAssignVarExpr), 0);
            }
            break;
        case LITEXPR_IFSTMT:
            {
                LitIfExpr* stmt = (LitIfExpr*)expression;
                lit_free_expression(state, stmt->condition);
                lit_free_expression(state, stmt->if_branch);
                lit_free_allocated_expressions(state, stmt->elseif_conditions);
                lit_free_allocated_statements(state, stmt->elseif_branches);
                lit_free_expression(state, stmt->else_branch);
                lit_gcmem_memrealloc(state, expression, sizeof(LitIfExpr), 0);
            }
            break;
        case LITEXPR_WHILE:
            {
                LitWhileExpr* stmt = (LitWhileExpr*)expression;
                lit_free_expression(state, stmt->condition);
                lit_free_expression(state, stmt->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitWhileExpr), 0);
            }
            break;
        case LITEXPR_FOR:
            {
                LitForExpr* stmt = (LitForExpr*)expression;
                lit_free_expression(state, stmt->increment);
                lit_free_expression(state, stmt->condition);
                lit_free_expression(state, stmt->init);

                lit_free_expression(state, stmt->var);
                lit_free_expression(state, stmt->body);
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
                lit_free_expression(state, stmt->body);
                lit_paramlist_freevalues(state, &stmt->parameters);
                lit_gcmem_memrealloc(state, expression, sizeof(LitFunctionExpr), 0);
            }
            break;
        case LITEXPR_RETURN:
            {
                lit_free_expression(state, ((LitReturnExpr*)expression)->expression);
                lit_gcmem_memrealloc(state, expression, sizeof(LitReturnExpr), 0);
            }
            break;
        case LITEXPR_METHOD:
            {
                LitMethodExpr* stmt = (LitMethodExpr*)expression;
                lit_paramlist_freevalues(state, &stmt->parameters);
                lit_free_expression(state, stmt->body);
                lit_gcmem_memrealloc(state, expression, sizeof(LitMethodExpr), 0);
            }
            break;
        case LITEXPR_CLASS:
            {
                internal_free_statements(state, &((LitClassExpr*)expression)->fields);
                lit_gcmem_memrealloc(state, expression, sizeof(LitClassExpr), 0);
            }
            break;
        case LITEXPR_FIELD:
            {
                LitFieldExpr* stmt = (LitFieldExpr*)expression;
                lit_free_expression(state, stmt->getter);
                lit_free_expression(state, stmt->setter);
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

static LitExpression* allocate_expression(LitState* state, uint64_t line, size_t size, LitExprType type)
{
    LitExpression* object;
    object = (LitExpression*)lit_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

LitLiteralExpr* lit_create_literal_expression(LitState* state, size_t line, LitValue value)
{
    LitLiteralExpr* expression;
    expression = (LitLiteralExpr*)allocate_expression(state, line, sizeof(LitLiteralExpr), LITEXPR_LITERAL);
    expression->value = value;
    return expression;
}

LitBinaryExpr* lit_create_binary_expression(LitState* state, size_t line, LitExpression* left, LitExpression* right, LitTokType op)
{
    LitBinaryExpr* expression;
    expression = (LitBinaryExpr*)allocate_expression(state, line, sizeof(LitBinaryExpr), LITEXPR_BINARY);
    expression->left = left;
    expression->right = right;
    expression->op = op;
    expression->ignore_left = false;
    return expression;
}

LitUnaryExpr* lit_create_unary_expression(LitState* state, size_t line, LitExpression* right, LitTokType op)
{
    LitUnaryExpr* expression;
    expression = (LitUnaryExpr*)allocate_expression(state, line, sizeof(LitUnaryExpr), LITEXPR_UNARY);
    expression->right = right;
    expression->op = op;
    return expression;
}

LitVarExpr* lit_create_var_expression(LitState* state, size_t line, const char* name, size_t length)
{
    LitVarExpr* expression;
    expression = (LitVarExpr*)allocate_expression(state, line, sizeof(LitVarExpr), LITEXPR_VAREXPR);
    expression->name = name;
    expression->length = length;
    return expression;
}

LitAssignExpr* lit_create_assign_expression(LitState* state, size_t line, LitExpression* to, LitExpression* value)
{
    LitAssignExpr* expression;
    expression = (LitAssignExpr*)allocate_expression(state, line, sizeof(LitAssignExpr), LITEXPR_ASSIGN);
    expression->to = to;
    expression->value = value;
    return expression;
}

LitCallExpr* lit_create_call_expression(LitState* state, size_t line, LitExpression* callee)
{
    LitCallExpr* expression;
    expression = (LitCallExpr*)allocate_expression(state, line, sizeof(LitCallExpr), LITEXPR_CALL);
    expression->callee = callee;
    expression->init = NULL;
    lit_exprlist_init(&expression->args);
    return expression;
}

LitGetExpr* lit_create_get_expression(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, bool questionable, bool ignore_result)
{
    LitGetExpr* expression;
    expression = (LitGetExpr*)allocate_expression(state, line, sizeof(LitGetExpr), LITEXPR_GET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->ignore_emit = false;
    expression->jump = questionable ? 0 : -1;
    expression->ignore_result = ignore_result;
    return expression;
}

LitSetExpr* lit_create_set_expression(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, LitExpression* value)
{
    LitSetExpr* expression;
    expression = (LitSetExpr*)allocate_expression(state, line, sizeof(LitSetExpr), LITEXPR_SET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->value = value;
    return expression;
}

LitLambdaExpr* lit_create_lambda_expression(LitState* state, size_t line)
{
    LitLambdaExpr* expression;
    expression = (LitLambdaExpr*)allocate_expression(state, line, sizeof(LitLambdaExpr), LITEXPR_LAMBDA);
    expression->body = NULL;
    lit_paramlist_init(&expression->parameters);
    return expression;
}

LitArrayExpr* lit_create_array_expression(LitState* state, size_t line)
{
    LitArrayExpr* expression;
    expression = (LitArrayExpr*)allocate_expression(state, line, sizeof(LitArrayExpr), LITEXPR_ARRAY);
    lit_exprlist_init(&expression->values);
    return expression;
}

LitObjectExpr* lit_create_object_expression(LitState* state, size_t line)
{
    LitObjectExpr* expression;
    expression = (LitObjectExpr*)allocate_expression(state, line, sizeof(LitObjectExpr), LITEXPR_OBJECT);
    lit_vallist_init(&expression->keys);
    lit_exprlist_init(&expression->values);
    return expression;
}

LitSubscriptExpr* lit_create_subscript_expression(LitState* state, size_t line, LitExpression* array, LitExpression* index)
{
    LitSubscriptExpr* expression;
    expression = (LitSubscriptExpr*)allocate_expression(state, line, sizeof(LitSubscriptExpr), LITEXPR_SUBSCRIPT);
    expression->array = array;
    expression->index = index;
    return expression;
}

LitThisExpr* lit_create_this_expression(LitState* state, size_t line)
{
    return (LitThisExpr*)allocate_expression(state, line, sizeof(LitThisExpr), LITEXPR_THIS);
}

LitSuperExpr* lit_create_super_expression(LitState* state, size_t line, LitString* method, bool ignore_result)
{
    LitSuperExpr* expression;
    expression = (LitSuperExpr*)allocate_expression(state, line, sizeof(LitSuperExpr), LITEXPR_SUPER);
    expression->method = method;
    expression->ignore_emit = false;
    expression->ignore_result = ignore_result;
    return expression;
}

LitRangeExpr* lit_create_range_expression(LitState* state, size_t line, LitExpression* from, LitExpression* to)
{
    LitRangeExpr* expression;
    expression = (LitRangeExpr*)allocate_expression(state, line, sizeof(LitRangeExpr), LITEXPR_RANGE);
    expression->from = from;
    expression->to = to;
    return expression;
}

LitTernaryExpr* lit_create_ternary_expression(LitState* state, size_t line, LitExpression* condition, LitExpression* if_branch, LitExpression* else_branch)
{
    LitTernaryExpr* expression;
    expression = (LitTernaryExpr*)allocate_expression(state, line, sizeof(LitTernaryExpr), LITEXPR_TERNARY);
    expression->condition = condition;
    expression->if_branch = if_branch;
    expression->else_branch = else_branch;

    return expression;
}

LitInterpolationExpr* lit_create_interpolation_expression(LitState* state, size_t line)
{
    LitInterpolationExpr* expression;
    expression = (LitInterpolationExpr*)allocate_expression(state, line, sizeof(LitInterpolationExpr), LITEXPR_INTERPOLATION);
    lit_exprlist_init(&expression->expressions);
    return expression;
}

LitReferenceExpr* lit_create_reference_expression(LitState* state, size_t line, LitExpression* to)
{
    LitReferenceExpr* expression;
    expression = (LitReferenceExpr*)allocate_expression(state, line, sizeof(LitReferenceExpr), LITEXPR_REFERENCE);
    expression->to = to;
    return expression;
}



static LitExpression* allocate_statement(LitState* state, uint64_t line, size_t size, LitExprType type)
{
    LitExpression* object;
    object = (LitExpression*)lit_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

LitExpressionExpr* lit_create_expression_statement(LitState* state, size_t line, LitExpression* expression)
{
    LitExpressionExpr* statement;
    statement = (LitExpressionExpr*)allocate_statement(state, line, sizeof(LitExpressionExpr), LITEXPR_EXPRESSION);
    statement->expression = expression;
    statement->pop = true;
    return statement;
}

LitBlockExpr* lit_create_block_statement(LitState* state, size_t line)
{
    LitBlockExpr* statement;
    statement = (LitBlockExpr*)allocate_statement(state, line, sizeof(LitBlockExpr), LITEXPR_BLOCK);
    lit_exprlist_init(&statement->statements);
    return statement;
}

LitAssignVarExpr* lit_create_assignvar_statement(LitState* state, size_t line, const char* name, size_t length, LitExpression* init, bool constant)
{
    LitAssignVarExpr* statement;
    statement = (LitAssignVarExpr*)allocate_statement(state, line, sizeof(LitAssignVarExpr), LITEXPR_VARSTMT);
    statement->name = name;
    statement->length = length;
    statement->init = init;
    statement->constant = constant;
    return statement;
}

LitIfExpr* lit_create_if_statement(LitState* state,
                                        size_t line,
                                        LitExpression* condition,
                                        LitExpression* if_branch,
                                        LitExpression* else_branch,
                                        LitExprList* elseif_conditions,
                                        LitExprList* elseif_branches)
{
    LitIfExpr* statement;
    statement = (LitIfExpr*)allocate_statement(state, line, sizeof(LitIfExpr), LITEXPR_IFSTMT);
    statement->condition = condition;
    statement->if_branch = if_branch;
    statement->else_branch = else_branch;
    statement->elseif_conditions = elseif_conditions;
    statement->elseif_branches = elseif_branches;
    return statement;
}

LitWhileExpr* lit_create_while_statement(LitState* state, size_t line, LitExpression* condition, LitExpression* body)
{
    LitWhileExpr* statement;
    statement = (LitWhileExpr*)allocate_statement(state, line, sizeof(LitWhileExpr), LITEXPR_WHILE);
    statement->condition = condition;
    statement->body = body;
    return statement;
}

LitForExpr* lit_create_for_statement(LitState* state,
                                          size_t line,
                                          LitExpression* init,
                                          LitExpression* var,
                                          LitExpression* condition,
                                          LitExpression* increment,
                                          LitExpression* body,
                                          bool c_style)
{
    LitForExpr* statement;
    statement = (LitForExpr*)allocate_statement(state, line, sizeof(LitForExpr), LITEXPR_FOR);
    statement->init = init;
    statement->var = var;
    statement->condition = condition;
    statement->increment = increment;
    statement->body = body;
    statement->c_style = c_style;
    return statement;
}

LitContinueExpr* lit_create_continue_statement(LitState* state, size_t line)
{
    return (LitContinueExpr*)allocate_statement(state, line, sizeof(LitContinueExpr), LITEXPR_CONTINUE);
}

LitBreakExpr* lit_create_break_statement(LitState* state, size_t line)
{
    return (LitBreakExpr*)allocate_statement(state, line, sizeof(LitBreakExpr), LITEXPR_BREAK);
}

LitFunctionExpr* lit_create_function_statement(LitState* state, size_t line, const char* name, size_t length)
{
    LitFunctionExpr* function;
    function = (LitFunctionExpr*)allocate_statement(state, line, sizeof(LitFunctionExpr), LITEXPR_FUNCTION);
    function->name = name;
    function->length = length;
    function->body = NULL;
    lit_paramlist_init(&function->parameters);
    return function;
}

LitReturnExpr* lit_create_return_statement(LitState* state, size_t line, LitExpression* expression)
{
    LitReturnExpr* statement;
    statement = (LitReturnExpr*)allocate_statement(state, line, sizeof(LitReturnExpr), LITEXPR_RETURN);
    statement->expression = expression;
    return statement;
}

LitMethodExpr* lit_create_method_statement(LitState* state, size_t line, LitString* name, bool is_static)
{
    LitMethodExpr* statement;
    statement = (LitMethodExpr*)allocate_statement(state, line, sizeof(LitMethodExpr), LITEXPR_METHOD);
    statement->name = name;
    statement->body = NULL;
    statement->is_static = is_static;
    lit_paramlist_init(&statement->parameters);
    return statement;
}

LitClassExpr* lit_create_class_statement(LitState* state, size_t line, LitString* name, LitString* parent)
{
    LitClassExpr* statement;
    statement = (LitClassExpr*)allocate_statement(state, line, sizeof(LitClassExpr), LITEXPR_CLASS);
    statement->name = name;
    statement->parent = parent;
    lit_exprlist_init(&statement->fields);
    return statement;
}

LitFieldExpr* lit_create_field_statement(LitState* state, size_t line, LitString* name, LitExpression* getter, LitExpression* setter, bool is_static)
{
    LitFieldExpr* statement;
    statement = (LitFieldExpr*)allocate_statement(state, line, sizeof(LitFieldExpr), LITEXPR_FIELD);
    statement->name = name;
    statement->getter = getter;
    statement->setter = setter;
    statement->is_static = is_static;
    return statement;
}

LitExprList* lit_allocate_expressions(LitState* state)
{
    LitExprList* expressions;
    expressions = (LitExprList*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(LitExprList));
    lit_exprlist_init(expressions);
    return expressions;
}

void lit_free_allocated_expressions(LitState* state, LitExprList* expressions)
{
    size_t i;
    if(expressions == NULL)
    {
        return;
    }
    for(i = 0; i < expressions->count; i++)
    {
        lit_free_expression(state, expressions->values[i]);
    }
    lit_exprlist_destroy(state, expressions);
    lit_gcmem_memrealloc(state, expressions, sizeof(LitExprList), 0);
}

LitExprList* lit_allocate_statements(LitState* state)
{
    LitExprList* statements;
    statements = (LitExprList*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(LitExprList));
    lit_exprlist_init(statements);
    return statements;
}

void lit_free_allocated_statements(LitState* state, LitExprList* statements)
{
    size_t i;
    if(statements == NULL)
    {
        return;
    }
    for(i = 0; i < statements->count; i++)
    {
        lit_free_expression(state, statements->values[i]);
    }
    lit_exprlist_destroy(state, statements);
    lit_gcmem_memrealloc(state, statements, sizeof(LitExprList), 0);
}
