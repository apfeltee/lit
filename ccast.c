
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
        lit_free_statement(state, statements->values[i]);
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
            lit_reallocate(state, expression, sizeof(LitLiteralExpr), 0);
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

            lit_reallocate(state, expression, sizeof(LitBinaryExpr), 0);
            break;
        }

        case LITEXPR_UNARY:
        {
            lit_free_expression(state, ((LitUnaryExpr*)expression)->right);
            lit_reallocate(state, expression, sizeof(LitUnaryExpr), 0);

            break;
        }

        case LITEXPR_VAR:
        {
            lit_reallocate(state, expression, sizeof(LitVarExpr), 0);
            break;
        }

        case LITEXPR_ASSIGN:
        {
            LitAssignExpression* expr = (LitAssignExpression*)expression;

            lit_free_expression(state, expr->to);
            lit_free_expression(state, expr->value);

            lit_reallocate(state, expression, sizeof(LitAssignExpression), 0);
            break;
        }

        case LITEXPR_CALL:
        {
            LitCallExpression* expr = (LitCallExpression*)expression;

            lit_free_expression(state, expr->callee);
            lit_free_expression(state, expr->init);

            free_expressions(state, &expr->args);

            lit_reallocate(state, expression, sizeof(LitCallExpression), 0);
            break;
        }

        case LITEXPR_GET:
        {
            lit_free_expression(state, ((LitGetExpression*)expression)->where);
            lit_reallocate(state, expression, sizeof(LitGetExpression), 0);
            break;
        }

        case LITEXPR_SET:
        {
            LitSetExpression* expr = (LitSetExpression*)expression;

            lit_free_expression(state, expr->where);
            lit_free_expression(state, expr->value);

            lit_reallocate(state, expression, sizeof(LitSetExpression), 0);
            break;
        }

        case LITEXPR_LAMBDA:
            {
                LitLambdaExpression* expr = (LitLambdaExpression*)expression;
                lit_paramlist_freevalues(state, &expr->parameters);
                lit_free_statement(state, expr->body);
                lit_reallocate(state, expression, sizeof(LitLambdaExpression), 0);
            }
            break;
        case LITEXPR_ARRAY:
            {
                free_expressions(state, &((LitArrayExpression*)expression)->values);
                lit_reallocate(state, expression, sizeof(LitArrayExpression), 0);
            }
            break;
        case LITEXPR_OBJECT:
            {
                LitObjectExpression* map = (LitObjectExpression*)expression;
                lit_vallist_destroy(state, &map->keys);
                free_expressions(state, &map->values);
                lit_reallocate(state, expression, sizeof(LitObjectExpression), 0);
            }
            break;
        case LITEXPR_SUBSCRIPT:
            {
                LitSubscriptExpression* expr = (LitSubscriptExpression*)expression;
                lit_free_expression(state, expr->array);
                lit_free_expression(state, expr->index);
                lit_reallocate(state, expression, sizeof(LitSubscriptExpression), 0);
            }
            break;
        case LITEXPR_THIS:
            {
                lit_reallocate(state, expression, sizeof(LitThisExpression), 0);
            }
            break;
        case LITEXPR_SUPER:
            {
                lit_reallocate(state, expression, sizeof(LitSuperExpression), 0);
            }
            break;
        case LITEXPR_RANGE:
            {
                LitRangeExpression* expr = (LitRangeExpression*)expression;
                lit_free_expression(state, expr->from);
                lit_free_expression(state, expr->to);
                lit_reallocate(state, expression, sizeof(LitRangeExpression), 0);
            }
            break;
        case LITEXPR_IF:
            {
                LitIfExpression* expr = (LitIfExpression*)expression;
                lit_free_expression(state, expr->condition);
                lit_free_expression(state, expr->if_branch);
                lit_free_expression(state, expr->else_branch);
                lit_reallocate(state, expression, sizeof(LitIfExpression), 0);
            }
            break;
        case LITEXPR_INTERPOLATION:
            {
                free_expressions(state, &((LitInterpolationExpression*)expression)->expressions);
                lit_reallocate(state, expression, sizeof(LitInterpolationExpression), 0);
            }
            break;
        case LITEXPR_REFERENCE:
            {
                lit_free_expression(state, ((LitReferenceExpression*)expression)->to);
                lit_reallocate(state, expression, sizeof(LitReferenceExpression), 0);
            }
            break;
        default:
            {
                lit_error(state, COMPILE_ERROR, "Unknown expression type %d", (int)expression->type);
            }
            break;
    }
}

static LitExpression* allocate_expression(LitState* state, uint64_t line, size_t size, LitExpressionType type)
{
    LitExpression* object;
    object = (LitExpression*)lit_reallocate(state, NULL, 0, size);
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

LitBinaryExpr* lit_create_binary_expression(LitState* state, size_t line, LitExpression* left, LitExpression* right, LitTokenType op)
{
    LitBinaryExpr* expression;
    expression = (LitBinaryExpr*)allocate_expression(state, line, sizeof(LitBinaryExpr), LITEXPR_BINARY);
    expression->left = left;
    expression->right = right;
    expression->op = op;
    expression->ignore_left = false;
    return expression;
}

LitUnaryExpr* lit_create_unary_expression(LitState* state, size_t line, LitExpression* right, LitTokenType op)
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
    expression = (LitVarExpr*)allocate_expression(state, line, sizeof(LitVarExpr), LITEXPR_VAR);
    expression->name = name;
    expression->length = length;
    return expression;
}

LitAssignExpression* lit_create_assign_expression(LitState* state, size_t line, LitExpression* to, LitExpression* value)
{
    LitAssignExpression* expression;
    expression = (LitAssignExpression*)allocate_expression(state, line, sizeof(LitAssignExpression), LITEXPR_ASSIGN);
    expression->to = to;
    expression->value = value;
    return expression;
}

LitCallExpression* lit_create_call_expression(LitState* state, size_t line, LitExpression* callee)
{
    LitCallExpression* expression;
    expression = (LitCallExpression*)allocate_expression(state, line, sizeof(LitCallExpression), LITEXPR_CALL);
    expression->callee = callee;
    expression->init = NULL;
    lit_exprlist_init(&expression->args);
    return expression;
}

LitGetExpression* lit_create_get_expression(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, bool questionable, bool ignore_result)
{
    LitGetExpression* expression;
    expression = (LitGetExpression*)allocate_expression(state, line, sizeof(LitGetExpression), LITEXPR_GET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->ignore_emit = false;
    expression->jump = questionable ? 0 : -1;
    expression->ignore_result = ignore_result;
    return expression;
}

LitSetExpression* lit_create_set_expression(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, LitExpression* value)
{
    LitSetExpression* expression;
    expression = (LitSetExpression*)allocate_expression(state, line, sizeof(LitSetExpression), LITEXPR_SET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->value = value;
    return expression;
}

LitLambdaExpression* lit_create_lambda_expression(LitState* state, size_t line)
{
    LitLambdaExpression* expression;
    expression = (LitLambdaExpression*)allocate_expression(state, line, sizeof(LitLambdaExpression), LITEXPR_LAMBDA);
    expression->body = NULL;
    lit_paramlist_init(&expression->parameters);
    return expression;
}

LitArrayExpression* lit_create_array_expression(LitState* state, size_t line)
{
    LitArrayExpression* expression;
    expression = (LitArrayExpression*)allocate_expression(state, line, sizeof(LitArrayExpression), LITEXPR_ARRAY);
    lit_exprlist_init(&expression->values);
    return expression;
}

LitObjectExpression* lit_create_object_expression(LitState* state, size_t line)
{
    LitObjectExpression* expression;
    expression = (LitObjectExpression*)allocate_expression(state, line, sizeof(LitObjectExpression), LITEXPR_OBJECT);
    lit_vallist_init(&expression->keys);
    lit_exprlist_init(&expression->values);
    return expression;
}

LitSubscriptExpression* lit_create_subscript_expression(LitState* state, size_t line, LitExpression* array, LitExpression* index)
{
    LitSubscriptExpression* expression;
    expression = (LitSubscriptExpression*)allocate_expression(state, line, sizeof(LitSubscriptExpression), LITEXPR_SUBSCRIPT);
    expression->array = array;
    expression->index = index;
    return expression;
}

LitThisExpression* lit_create_this_expression(LitState* state, size_t line)
{
    return (LitThisExpression*)allocate_expression(state, line, sizeof(LitThisExpression), LITEXPR_THIS);
}

LitSuperExpression* lit_create_super_expression(LitState* state, size_t line, LitString* method, bool ignore_result)
{
    LitSuperExpression* expression;
    expression = (LitSuperExpression*)allocate_expression(state, line, sizeof(LitSuperExpression), LITEXPR_SUPER);
    expression->method = method;
    expression->ignore_emit = false;
    expression->ignore_result = ignore_result;
    return expression;
}

LitRangeExpression* lit_create_range_expression(LitState* state, size_t line, LitExpression* from, LitExpression* to)
{
    LitRangeExpression* expression;
    expression = (LitRangeExpression*)allocate_expression(state, line, sizeof(LitRangeExpression), LITEXPR_RANGE);
    expression->from = from;
    expression->to = to;
    return expression;
}

LitIfExpression* lit_create_if_experssion(LitState* state, size_t line, LitExpression* condition, LitExpression* if_branch, LitExpression* else_branch)
{
    LitIfExpression* expression;
    expression = (LitIfExpression*)allocate_expression(state, line, sizeof(LitIfExpression), LITEXPR_IF);
    expression->condition = condition;
    expression->if_branch = if_branch;
    expression->else_branch = else_branch;

    return expression;
}

LitInterpolationExpression* lit_create_interpolation_expression(LitState* state, size_t line)
{
    LitInterpolationExpression* expression;
    expression = (LitInterpolationExpression*)allocate_expression(state, line, sizeof(LitInterpolationExpression), LITEXPR_INTERPOLATION);
    lit_exprlist_init(&expression->expressions);
    return expression;
}

LitReferenceExpression* lit_create_reference_expression(LitState* state, size_t line, LitExpression* to)
{
    LitReferenceExpression* expression;
    expression = (LitReferenceExpression*)allocate_expression(state, line, sizeof(LitReferenceExpression), LITEXPR_REFERENCE);
    expression->to = to;
    return expression;
}

void lit_free_statement(LitState* state, LitExpression* statement)
{
    if(statement == NULL)
    {
        return;
    }
    switch(statement->type)
    {
        case LITSTMT_EXPRESSION:
            {
                lit_free_expression(state, ((LitExpressionStatement*)statement)->expression);
                lit_reallocate(state, statement, sizeof(LitExpressionStatement), 0);
            }
            break;
        case LITSTMT_BLOCK:
            {
                internal_free_statements(state, &((LitBlockStatement*)statement)->statements);
                lit_reallocate(state, statement, sizeof(LitBlockStatement), 0);
            }
            break;
        case LITSTMT_VAR:
            {
                lit_free_expression(state, ((LitVarStatement*)statement)->init);
                lit_reallocate(state, statement, sizeof(LitVarStatement), 0);
            }
            break;
        case LITSTMT_IF:
            {
                LitIfStatement* stmt = (LitIfStatement*)statement;
                lit_free_expression(state, stmt->condition);
                lit_free_statement(state, stmt->if_branch);
                lit_free_allocated_expressions(state, stmt->elseif_conditions);
                lit_free_allocated_statements(state, stmt->elseif_branches);
                lit_free_statement(state, stmt->else_branch);
                lit_reallocate(state, statement, sizeof(LitIfStatement), 0);
            }
            break;
        case LITSTMT_WHILE:
            {
                LitWhileStatement* stmt = (LitWhileStatement*)statement;
                lit_free_expression(state, stmt->condition);
                lit_free_statement(state, stmt->body);
                lit_reallocate(state, statement, sizeof(LitWhileStatement), 0);
            }
            break;
        case LITSTMT_FOR:
            {
                LitForStatement* stmt = (LitForStatement*)statement;
                lit_free_expression(state, stmt->increment);
                lit_free_expression(state, stmt->condition);
                lit_free_expression(state, stmt->init);

                lit_free_statement(state, stmt->var);
                lit_free_statement(state, stmt->body);
                lit_reallocate(state, statement, sizeof(LitForStatement), 0);
            }
            break;
        case LITSTMT_CONTINUE:
            {
                lit_reallocate(state, statement, sizeof(LitContinueStatement), 0);
            }
            break;
        case LITSTMT_BREAK:
            {
                lit_reallocate(state, statement, sizeof(LitBreakStatement), 0);
            }
            break;
        case LITSTMT_FUNCTION:
            {
                LitFunctionStatement* stmt = (LitFunctionStatement*)statement;
                lit_free_statement(state, stmt->body);
                lit_paramlist_freevalues(state, &stmt->parameters);
                lit_reallocate(state, statement, sizeof(LitFunctionStatement), 0);
            }
            break;
        case LITSTMT_RETURN:
            {
                lit_free_expression(state, ((LitReturnStatement*)statement)->expression);
                lit_reallocate(state, statement, sizeof(LitReturnStatement), 0);
            }
            break;
        case LITSTMT_METHOD:
            {
                LitMethodStatement* stmt = (LitMethodStatement*)statement;
                lit_paramlist_freevalues(state, &stmt->parameters);
                lit_free_statement(state, stmt->body);
                lit_reallocate(state, statement, sizeof(LitMethodStatement), 0);
            }
            break;
        case LITSTMT_CLASS:
            {
                internal_free_statements(state, &((LitClassStatement*)statement)->fields);
                lit_reallocate(state, statement, sizeof(LitClassStatement), 0);
            }
            break;
        case LITSTMT_FIELD:
            {
                LitFieldStatement* stmt = (LitFieldStatement*)statement;
                lit_free_statement(state, stmt->getter);
                lit_free_statement(state, stmt->setter);
                lit_reallocate(state, statement, sizeof(LitFieldStatement), 0);
            }
            break;
        default:
            {
                lit_error(state, COMPILE_ERROR, "Unknown statement type %d", (int)statement->type);
            }
            break;
    }
}

static LitExpression* allocate_statement(LitState* state, uint64_t line, size_t size, LitExpressionType type)
{
    LitExpression* object;
    object = (LitExpression*)lit_reallocate(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

LitExpressionStatement* lit_create_expression_statement(LitState* state, size_t line, LitExpression* expression)
{
    LitExpressionStatement* statement;
    statement = (LitExpressionStatement*)allocate_statement(state, line, sizeof(LitExpressionStatement), LITSTMT_EXPRESSION);
    statement->expression = expression;
    statement->pop = true;
    return statement;
}

LitBlockStatement* lit_create_block_statement(LitState* state, size_t line)
{
    LitBlockStatement* statement;
    statement = (LitBlockStatement*)allocate_statement(state, line, sizeof(LitBlockStatement), LITSTMT_BLOCK);
    lit_exprlist_init(&statement->statements);
    return statement;
}

LitVarStatement* lit_create_var_statement(LitState* state, size_t line, const char* name, size_t length, LitExpression* init, bool constant)
{
    LitVarStatement* statement;
    statement = (LitVarStatement*)allocate_statement(state, line, sizeof(LitVarStatement), LITSTMT_VAR);
    statement->name = name;
    statement->length = length;
    statement->init = init;
    statement->constant = constant;
    return statement;
}

LitIfStatement* lit_create_if_statement(LitState* state,
                                        size_t line,
                                        LitExpression* condition,
                                        LitExpression* if_branch,
                                        LitExpression* else_branch,
                                        LitExprList* elseif_conditions,
                                        LitExprList* elseif_branches)
{
    LitIfStatement* statement;
    statement = (LitIfStatement*)allocate_statement(state, line, sizeof(LitIfStatement), LITSTMT_IF);
    statement->condition = condition;
    statement->if_branch = if_branch;
    statement->else_branch = else_branch;
    statement->elseif_conditions = elseif_conditions;
    statement->elseif_branches = elseif_branches;
    return statement;
}

LitWhileStatement* lit_create_while_statement(LitState* state, size_t line, LitExpression* condition, LitExpression* body)
{
    LitWhileStatement* statement;
    statement = (LitWhileStatement*)allocate_statement(state, line, sizeof(LitWhileStatement), LITSTMT_WHILE);
    statement->condition = condition;
    statement->body = body;
    return statement;
}

LitForStatement* lit_create_for_statement(LitState* state,
                                          size_t line,
                                          LitExpression* init,
                                          LitExpression* var,
                                          LitExpression* condition,
                                          LitExpression* increment,
                                          LitExpression* body,
                                          bool c_style)
{
    LitForStatement* statement;
    statement = (LitForStatement*)allocate_statement(state, line, sizeof(LitForStatement), LITSTMT_FOR);
    statement->init = init;
    statement->var = var;
    statement->condition = condition;
    statement->increment = increment;
    statement->body = body;
    statement->c_style = c_style;
    return statement;
}

LitContinueStatement* lit_create_continue_statement(LitState* state, size_t line)
{
    return (LitContinueStatement*)allocate_statement(state, line, sizeof(LitContinueStatement), LITSTMT_CONTINUE);
}

LitBreakStatement* lit_create_break_statement(LitState* state, size_t line)
{
    return (LitBreakStatement*)allocate_statement(state, line, sizeof(LitBreakStatement), LITSTMT_BREAK);
}

LitFunctionStatement* lit_create_function_statement(LitState* state, size_t line, const char* name, size_t length)
{
    LitFunctionStatement* function;
    function = (LitFunctionStatement*)allocate_statement(state, line, sizeof(LitFunctionStatement), LITSTMT_FUNCTION);
    function->name = name;
    function->length = length;
    function->body = NULL;
    lit_paramlist_init(&function->parameters);
    return function;
}

LitReturnStatement* lit_create_return_statement(LitState* state, size_t line, LitExpression* expression)
{
    LitReturnStatement* statement;
    statement = (LitReturnStatement*)allocate_statement(state, line, sizeof(LitReturnStatement), LITSTMT_RETURN);
    statement->expression = expression;
    return statement;
}

LitMethodStatement* lit_create_method_statement(LitState* state, size_t line, LitString* name, bool is_static)
{
    LitMethodStatement* statement;
    statement = (LitMethodStatement*)allocate_statement(state, line, sizeof(LitMethodStatement), LITSTMT_METHOD);
    statement->name = name;
    statement->body = NULL;
    statement->is_static = is_static;
    lit_paramlist_init(&statement->parameters);
    return statement;
}

LitClassStatement* lit_create_class_statement(LitState* state, size_t line, LitString* name, LitString* parent)
{
    LitClassStatement* statement;
    statement = (LitClassStatement*)allocate_statement(state, line, sizeof(LitClassStatement), LITSTMT_CLASS);
    statement->name = name;
    statement->parent = parent;
    lit_exprlist_init(&statement->fields);
    return statement;
}

LitFieldStatement* lit_create_field_statement(LitState* state, size_t line, LitString* name, LitExpression* getter, LitExpression* setter, bool is_static)
{
    LitFieldStatement* statement;
    statement = (LitFieldStatement*)allocate_statement(state, line, sizeof(LitFieldStatement), LITSTMT_FIELD);
    statement->name = name;
    statement->getter = getter;
    statement->setter = setter;
    statement->is_static = is_static;
    return statement;
}

LitExprList* lit_allocate_expressions(LitState* state)
{
    LitExprList* expressions;
    expressions = (LitExprList*)lit_reallocate(state, NULL, 0, sizeof(LitExprList));
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
    lit_reallocate(state, expressions, sizeof(LitExprList), 0);
}

LitExprList* lit_allocate_statements(LitState* state)
{
    LitExprList* statements;
    statements = (LitExprList*)lit_reallocate(state, NULL, 0, sizeof(LitExprList));
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
        lit_free_statement(state, statements->values[i]);
    }
    lit_exprlist_destroy(state, statements);
    lit_reallocate(state, statements, sizeof(LitExprList), 0);
}
