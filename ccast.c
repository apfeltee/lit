
#include "lit.h"

void lit_init_expressions(LitExprList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}
void lit_free_expressions(LitState* state, LitExprList* array)
{
    LIT_FREE_ARRAY(state, LitExpression*, array->values, array->capacity);
    lit_init_expressions(array);
}
void lit_expressions_write(LitState* state, LitExprList* array, LitExpression* value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, LitExpression*, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}
void lit_init_statements(LitStmtList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}
void lit_free_statements(LitState* state, LitStmtList* array)
{
    LIT_FREE_ARRAY(state, LitStatement*, array->values, array->capacity);
    lit_init_statements(array);
}
void lit_statements_write(LitState* state, LitStmtList* array, LitStatement* value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, LitStatement*, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}
void lit_init_parameters(LitParameters* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}
void lit_free_parameters(LitState* state, LitParameters* array)
{
    LIT_FREE_ARRAY(state, LitParameter, array->values, array->capacity);
    lit_init_parameters(array);
}
void lit_parameters_write(LitState* state, LitParameters* array, LitParameter value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, LitParameter, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
};



static void free_parameters(LitState* state, LitParameters* parameters)
{
    for(size_t i = 0; i < parameters->count; i++)
    {
        lit_free_expression(state, parameters->values[i].default_value);
    }

    lit_free_parameters(state, parameters);
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

    lit_free_expressions(state, expressions);
}

void internal_free_statements(LitState* state, LitStmtList* statements)
{
    if(statements == NULL)
    {
        return;
    }

    for(size_t i = 0; i < statements->count; i++)
    {
        lit_free_statement(state, statements->values[i]);
    }

    lit_free_statements(state, statements);
}

void lit_free_expression(LitState* state, LitExpression* expression)
{
    if(expression == NULL)
    {
        return;
    }

    switch(expression->type)
    {
        case LITERAL_EXPRESSION:
        {
            lit_reallocate(state, expression, sizeof(LitLiteralExpression), 0);
            break;
        }

        case BINARY_EXPRESSION:
        {
            LitBinaryExpression* expr = (LitBinaryExpression*)expression;

            if(!expr->ignore_left)
            {
                lit_free_expression(state, expr->left);
            }

            lit_free_expression(state, expr->right);

            lit_reallocate(state, expression, sizeof(LitBinaryExpression), 0);
            break;
        }

        case UNARY_EXPRESSION:
        {
            lit_free_expression(state, ((LitUnaryExpression*)expression)->right);
            lit_reallocate(state, expression, sizeof(LitUnaryExpression), 0);

            break;
        }

        case VAR_EXPRESSION:
        {
            lit_reallocate(state, expression, sizeof(LitVarExpression), 0);
            break;
        }

        case ASSIGN_EXPRESSION:
        {
            LitAssignExpression* expr = (LitAssignExpression*)expression;

            lit_free_expression(state, expr->to);
            lit_free_expression(state, expr->value);

            lit_reallocate(state, expression, sizeof(LitAssignExpression), 0);
            break;
        }

        case CALL_EXPRESSION:
        {
            LitCallExpression* expr = (LitCallExpression*)expression;

            lit_free_expression(state, expr->callee);
            lit_free_expression(state, expr->init);

            free_expressions(state, &expr->args);

            lit_reallocate(state, expression, sizeof(LitCallExpression), 0);
            break;
        }

        case GET_EXPRESSION:
        {
            lit_free_expression(state, ((LitGetExpression*)expression)->where);
            lit_reallocate(state, expression, sizeof(LitGetExpression), 0);
            break;
        }

        case SET_EXPRESSION:
        {
            LitSetExpression* expr = (LitSetExpression*)expression;

            lit_free_expression(state, expr->where);
            lit_free_expression(state, expr->value);

            lit_reallocate(state, expression, sizeof(LitSetExpression), 0);
            break;
        }

        case LAMBDA_EXPRESSION:
            {
                LitLambdaExpression* expr = (LitLambdaExpression*)expression;
                free_parameters(state, &expr->parameters);
                lit_free_statement(state, expr->body);
                lit_reallocate(state, expression, sizeof(LitLambdaExpression), 0);
            }
            break;
        case ARRAY_EXPRESSION:
            {
                free_expressions(state, &((LitArrayExpression*)expression)->values);
                lit_reallocate(state, expression, sizeof(LitArrayExpression), 0);
            }
            break;
        case OBJECT_EXPRESSION:
            {
                LitObjectExpression* map = (LitObjectExpression*)expression;
                lit_free_values(state, &map->keys);
                free_expressions(state, &map->values);
                lit_reallocate(state, expression, sizeof(LitObjectExpression), 0);
            }
            break;
        case SUBSCRIPT_EXPRESSION:
            {
                LitSubscriptExpression* expr = (LitSubscriptExpression*)expression;
                lit_free_expression(state, expr->array);
                lit_free_expression(state, expr->index);
                lit_reallocate(state, expression, sizeof(LitSubscriptExpression), 0);
            }
            break;
        case THIS_EXPRESSION:
            {
                lit_reallocate(state, expression, sizeof(LitThisExpression), 0);
            }
            break;
        case SUPER_EXPRESSION:
            {
                lit_reallocate(state, expression, sizeof(LitSuperExpression), 0);
            }
            break;
        case RANGE_EXPRESSION:
            {
                LitRangeExpression* expr = (LitRangeExpression*)expression;
                lit_free_expression(state, expr->from);
                lit_free_expression(state, expr->to);
                lit_reallocate(state, expression, sizeof(LitRangeExpression), 0);
            }
            break;
        case IF_EXPRESSION:
            {
                LitIfExpression* expr = (LitIfExpression*)expression;
                lit_free_expression(state, expr->condition);
                lit_free_expression(state, expr->if_branch);
                lit_free_expression(state, expr->else_branch);
                lit_reallocate(state, expression, sizeof(LitIfExpression), 0);
            }
            break;
        case INTERPOLATION_EXPRESSION:
            {
                free_expressions(state, &((LitInterpolationExpression*)expression)->expressions);
                lit_reallocate(state, expression, sizeof(LitInterpolationExpression), 0);
            }
            break;
        case REFERENCE_EXPRESSION:
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

LitLiteralExpression* lit_create_literal_expression(LitState* state, size_t line, LitValue value)
{
    LitLiteralExpression* expression;
    expression = (LitLiteralExpression*)allocate_expression(state, line, sizeof(LitLiteralExpression), LITERAL_EXPRESSION);
    expression->value = value;
    return expression;
}

LitBinaryExpression* lit_create_binary_expression(LitState* state, size_t line, LitExpression* left, LitExpression* right, LitTokenType op)
{
    LitBinaryExpression* expression;
    expression = (LitBinaryExpression*)allocate_expression(state, line, sizeof(LitBinaryExpression), BINARY_EXPRESSION);
    expression->left = left;
    expression->right = right;
    expression->op = op;
    expression->ignore_left = false;
    return expression;
}

LitUnaryExpression* lit_create_unary_expression(LitState* state, size_t line, LitExpression* right, LitTokenType op)
{
    LitUnaryExpression* expression;
    expression = (LitUnaryExpression*)allocate_expression(state, line, sizeof(LitUnaryExpression), UNARY_EXPRESSION);
    expression->right = right;
    expression->op = op;
    return expression;
}

LitVarExpression* lit_create_var_expression(LitState* state, size_t line, const char* name, size_t length)
{
    LitVarExpression* expression;
    expression = (LitVarExpression*)allocate_expression(state, line, sizeof(LitVarExpression), VAR_EXPRESSION);
    expression->name = name;
    expression->length = length;
    return expression;
}

LitAssignExpression* lit_create_assign_expression(LitState* state, size_t line, LitExpression* to, LitExpression* value)
{
    LitAssignExpression* expression;
    expression = (LitAssignExpression*)allocate_expression(state, line, sizeof(LitAssignExpression), ASSIGN_EXPRESSION);
    expression->to = to;
    expression->value = value;
    return expression;
}

LitCallExpression* lit_create_call_expression(LitState* state, size_t line, LitExpression* callee)
{
    LitCallExpression* expression;
    expression = (LitCallExpression*)allocate_expression(state, line, sizeof(LitCallExpression), CALL_EXPRESSION);
    expression->callee = callee;
    expression->init = NULL;
    lit_init_expressions(&expression->args);
    return expression;
}

LitGetExpression* lit_create_get_expression(LitState* state, size_t line, LitExpression* where, const char* name, size_t length, bool questionable, bool ignore_result)
{
    LitGetExpression* expression;
    expression = (LitGetExpression*)allocate_expression(state, line, sizeof(LitGetExpression), GET_EXPRESSION);
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
    expression = (LitSetExpression*)allocate_expression(state, line, sizeof(LitSetExpression), SET_EXPRESSION);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->value = value;
    return expression;
}

LitLambdaExpression* lit_create_lambda_expression(LitState* state, size_t line)
{
    LitLambdaExpression* expression;
    expression = (LitLambdaExpression*)allocate_expression(state, line, sizeof(LitLambdaExpression), LAMBDA_EXPRESSION);
    expression->body = NULL;
    lit_init_parameters(&expression->parameters);
    return expression;
}

LitArrayExpression* lit_create_array_expression(LitState* state, size_t line)
{
    LitArrayExpression* expression;
    expression = (LitArrayExpression*)allocate_expression(state, line, sizeof(LitArrayExpression), ARRAY_EXPRESSION);
    lit_init_expressions(&expression->values);
    return expression;
}

LitObjectExpression* lit_create_object_expression(LitState* state, size_t line)
{
    LitObjectExpression* expression;
    expression = (LitObjectExpression*)allocate_expression(state, line, sizeof(LitObjectExpression), OBJECT_EXPRESSION);
    lit_init_values(&expression->keys);
    lit_init_expressions(&expression->values);
    return expression;
}

LitSubscriptExpression* lit_create_subscript_expression(LitState* state, size_t line, LitExpression* array, LitExpression* index)
{
    LitSubscriptExpression* expression;
    expression = (LitSubscriptExpression*)allocate_expression(state, line, sizeof(LitSubscriptExpression), SUBSCRIPT_EXPRESSION);
    expression->array = array;
    expression->index = index;
    return expression;
}

LitThisExpression* lit_create_this_expression(LitState* state, size_t line)
{
    return (LitThisExpression*)allocate_expression(state, line, sizeof(LitThisExpression), THIS_EXPRESSION);
}

LitSuperExpression* lit_create_super_expression(LitState* state, size_t line, LitString* method, bool ignore_result)
{
    LitSuperExpression* expression;
    expression = (LitSuperExpression*)allocate_expression(state, line, sizeof(LitSuperExpression), SUPER_EXPRESSION);
    expression->method = method;
    expression->ignore_emit = false;
    expression->ignore_result = ignore_result;
    return expression;
}

LitRangeExpression* lit_create_range_expression(LitState* state, size_t line, LitExpression* from, LitExpression* to)
{
    LitRangeExpression* expression;
    expression = (LitRangeExpression*)allocate_expression(state, line, sizeof(LitRangeExpression), RANGE_EXPRESSION);
    expression->from = from;
    expression->to = to;
    return expression;
}

LitIfExpression* lit_create_if_experssion(LitState* state, size_t line, LitExpression* condition, LitExpression* if_branch, LitExpression* else_branch)
{
    LitIfExpression* expression;
    expression = (LitIfExpression*)allocate_expression(state, line, sizeof(LitIfExpression), IF_EXPRESSION);
    expression->condition = condition;
    expression->if_branch = if_branch;
    expression->else_branch = else_branch;

    return expression;
}

LitInterpolationExpression* lit_create_interpolation_expression(LitState* state, size_t line)
{
    LitInterpolationExpression* expression;
    expression = (LitInterpolationExpression*)allocate_expression(state, line, sizeof(LitInterpolationExpression), INTERPOLATION_EXPRESSION);
    lit_init_expressions(&expression->expressions);
    return expression;
}

LitReferenceExpression* lit_create_reference_expression(LitState* state, size_t line, LitExpression* to)
{
    LitReferenceExpression* expression;
    expression = (LitReferenceExpression*)allocate_expression(state, line, sizeof(LitReferenceExpression), REFERENCE_EXPRESSION);
    expression->to = to;
    return expression;
}

void lit_free_statement(LitState* state, LitStatement* statement)
{
    if(statement == NULL)
    {
        return;
    }
    switch(statement->type)
    {
        case EXPRESSION_STATEMENT:
            {
                lit_free_expression(state, ((LitExpressionStatement*)statement)->expression);
                lit_reallocate(state, statement, sizeof(LitExpressionStatement), 0);
            }
            break;
        case BLOCK_STATEMENT:
            {
                internal_free_statements(state, &((LitBlockStatement*)statement)->statements);
                lit_reallocate(state, statement, sizeof(LitBlockStatement), 0);
            }
            break;
        case VAR_STATEMENT:
            {
                lit_free_expression(state, ((LitVarStatement*)statement)->init);
                lit_reallocate(state, statement, sizeof(LitVarStatement), 0);
            }
            break;
        case IF_STATEMENT:
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
        case WHILE_STATEMENT:
            {
                LitWhileStatement* stmt = (LitWhileStatement*)statement;
                lit_free_expression(state, stmt->condition);
                lit_free_statement(state, stmt->body);
                lit_reallocate(state, statement, sizeof(LitWhileStatement), 0);
            }
            break;
        case FOR_STATEMENT:
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
        case CONTINUE_STATEMENT:
            {
                lit_reallocate(state, statement, sizeof(LitContinueStatement), 0);
            }
            break;
        case BREAK_STATEMENT:
            {
                lit_reallocate(state, statement, sizeof(LitBreakStatement), 0);
            }
            break;
        case FUNCTION_STATEMENT:
            {
                LitFunctionStatement* stmt = (LitFunctionStatement*)statement;
                lit_free_statement(state, stmt->body);
                free_parameters(state, &stmt->parameters);
                lit_reallocate(state, statement, sizeof(LitFunctionStatement), 0);
            }
            break;
        case RETURN_STATEMENT:
            {
                lit_free_expression(state, ((LitReturnStatement*)statement)->expression);
                lit_reallocate(state, statement, sizeof(LitReturnStatement), 0);
            }
            break;
        case METHOD_STATEMENT:
            {
                LitMethodStatement* stmt = (LitMethodStatement*)statement;
                free_parameters(state, &stmt->parameters);
                lit_free_statement(state, stmt->body);
                lit_reallocate(state, statement, sizeof(LitMethodStatement), 0);
            }
            break;
        case CLASS_STATEMENT:
            {
                internal_free_statements(state, &((LitClassStatement*)statement)->fields);
                lit_reallocate(state, statement, sizeof(LitClassStatement), 0);
            }
            break;
        case FIELD_STATEMENT:
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

static LitStatement* allocate_statement(LitState* state, uint64_t line, size_t size, LitStatementType type)
{
    LitStatement* object;
    object = (LitStatement*)lit_reallocate(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

LitExpressionStatement* lit_create_expression_statement(LitState* state, size_t line, LitExpression* expression)
{
    LitExpressionStatement* statement;
    statement = (LitExpressionStatement*)allocate_statement(state, line, sizeof(LitExpressionStatement), EXPRESSION_STATEMENT);
    statement->expression = expression;
    statement->pop = true;
    return statement;
}

LitBlockStatement* lit_create_block_statement(LitState* state, size_t line)
{
    LitBlockStatement* statement;
    statement = (LitBlockStatement*)allocate_statement(state, line, sizeof(LitBlockStatement), BLOCK_STATEMENT);
    lit_init_statements(&statement->statements);
    return statement;
}

LitVarStatement* lit_create_var_statement(LitState* state, size_t line, const char* name, size_t length, LitExpression* init, bool constant)
{
    LitVarStatement* statement;
    statement = (LitVarStatement*)allocate_statement(state, line, sizeof(LitVarStatement), VAR_STATEMENT);
    statement->name = name;
    statement->length = length;
    statement->init = init;
    statement->constant = constant;
    return statement;
}

LitIfStatement* lit_create_if_statement(LitState* state,
                                        size_t line,
                                        LitExpression* condition,
                                        LitStatement* if_branch,
                                        LitStatement* else_branch,
                                        LitExprList* elseif_conditions,
                                        LitStmtList* elseif_branches)
{
    LitIfStatement* statement;
    statement = (LitIfStatement*)allocate_statement(state, line, sizeof(LitIfStatement), IF_STATEMENT);
    statement->condition = condition;
    statement->if_branch = if_branch;
    statement->else_branch = else_branch;
    statement->elseif_conditions = elseif_conditions;
    statement->elseif_branches = elseif_branches;
    return statement;
}

LitWhileStatement* lit_create_while_statement(LitState* state, size_t line, LitExpression* condition, LitStatement* body)
{
    LitWhileStatement* statement;
    statement = (LitWhileStatement*)allocate_statement(state, line, sizeof(LitWhileStatement), WHILE_STATEMENT);
    statement->condition = condition;
    statement->body = body;
    return statement;
}

LitForStatement* lit_create_for_statement(LitState* state,
                                          size_t line,
                                          LitExpression* init,
                                          LitStatement* var,
                                          LitExpression* condition,
                                          LitExpression* increment,
                                          LitStatement* body,
                                          bool c_style)
{
    LitForStatement* statement;
    statement = (LitForStatement*)allocate_statement(state, line, sizeof(LitForStatement), FOR_STATEMENT);
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
    return (LitContinueStatement*)allocate_statement(state, line, sizeof(LitContinueStatement), CONTINUE_STATEMENT);
}

LitBreakStatement* lit_create_break_statement(LitState* state, size_t line)
{
    return (LitBreakStatement*)allocate_statement(state, line, sizeof(LitBreakStatement), BREAK_STATEMENT);
}

LitFunctionStatement* lit_create_function_statement(LitState* state, size_t line, const char* name, size_t length)
{
    LitFunctionStatement* function;
    function = (LitFunctionStatement*)allocate_statement(state, line, sizeof(LitFunctionStatement), FUNCTION_STATEMENT);
    function->name = name;
    function->length = length;
    function->body = NULL;
    lit_init_parameters(&function->parameters);
    return function;
}

LitReturnStatement* lit_create_return_statement(LitState* state, size_t line, LitExpression* expression)
{
    LitReturnStatement* statement;
    statement = (LitReturnStatement*)allocate_statement(state, line, sizeof(LitReturnStatement), RETURN_STATEMENT);
    statement->expression = expression;
    return statement;
}

LitMethodStatement* lit_create_method_statement(LitState* state, size_t line, LitString* name, bool is_static)
{
    LitMethodStatement* statement;
    statement = (LitMethodStatement*)allocate_statement(state, line, sizeof(LitMethodStatement), METHOD_STATEMENT);
    statement->name = name;
    statement->body = NULL;
    statement->is_static = is_static;
    lit_init_parameters(&statement->parameters);
    return statement;
}

LitClassStatement* lit_create_class_statement(LitState* state, size_t line, LitString* name, LitString* parent)
{
    LitClassStatement* statement;
    statement = (LitClassStatement*)allocate_statement(state, line, sizeof(LitClassStatement), CLASS_STATEMENT);
    statement->name = name;
    statement->parent = parent;
    lit_init_statements(&statement->fields);
    return statement;
}

LitFieldStatement* lit_create_field_statement(LitState* state, size_t line, LitString* name, LitStatement* getter, LitStatement* setter, bool is_static)
{
    LitFieldStatement* statement;
    statement = (LitFieldStatement*)allocate_statement(state, line, sizeof(LitFieldStatement), FIELD_STATEMENT);
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
    lit_init_expressions(expressions);
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
    lit_free_expressions(state, expressions);
    lit_reallocate(state, expressions, sizeof(LitExprList), 0);
}

LitStmtList* lit_allocate_statements(LitState* state)
{
    LitStmtList* statements;
    statements = (LitStmtList*)lit_reallocate(state, NULL, 0, sizeof(LitStmtList));
    lit_init_statements(statements);
    return statements;
}

void lit_free_allocated_statements(LitState* state, LitStmtList* statements)
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
    lit_free_statements(state, statements);
    lit_reallocate(state, statements, sizeof(LitStmtList), 0);
}
