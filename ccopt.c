
#include "lit.h"
#include "priv.h"

#define LIT_DEBUG_OPTIMIZER

#define optc_do_binary_op(op) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        optdbg("translating constant binary expression of '" # op "' to constant value"); \
        return lit_value_numbertovalue(optimizer->state, lit_value_asnumber(a) op lit_value_asnumber(b)); \
    } \
    return NULL_VALUE;

#define optc_do_bitwise_op(op) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        optdbg("translating constant bitwise expression of '" #op "' to constant value"); \
        return lit_value_numbertovalue(optimizer->state, (int)lit_value_asnumber(a) op(int) lit_value_asnumber(b)); \
    } \
    return NULL_VALUE;

#define optc_do_fn_op(fn, tokstr) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        optdbg("translating constant expression of '" tokstr "' to constant value via call to '" #fn "'"); \
        return lit_value_numbertovalue(optimizer->state, fn(lit_value_asnumber(a), lit_value_asnumber(b))); \
    } \
    return NULL_VALUE;



static void optimize_expression(LitOptimizer* optimizer, LitExpression** slot);
static void optimize_expressions(LitOptimizer* optimizer, LitExprList* expressions);
static void optimize_statements(LitOptimizer* optimizer, LitExprList* statements);
static void optimize_statement(LitOptimizer* optimizer, LitExpression** slot);

static const char* optimization_level_descriptions[LITOPTLEVEL_TOTAL]
= { "No optimizations (same as -Ono-all)", "Super light optimizations, sepcific to interactive shell.",
    "(default) Recommended optimization level for the development.", "Medium optimization, recommended for the release.",
    "(default for bytecode) Extreme optimization, throws out most of the variable/function names, used for bytecode compilation." };

static const char* optimization_names[LITOPTSTATE_TOTAL]
= { "constant-folding", "literal-folding", "unused-var",    "unreachable-code",
    "empty-body",       "line-info",       "private-names", "c-for" };

static const char* optimization_descriptions[LITOPTSTATE_TOTAL]
= { "Replaces constants in code with their values.",
    "Precalculates literal expressions (3 + 4 is replaced with 7).",
    "Removes user-declared all variables, that were not used.",
    "Removes code that will never be reached.",
    "Removes loops with empty bodies.",
    "Removes line information from chunks to save on space.",
    "Removes names of the private locals from modules (they are indexed by id at runtime).",
    "Replaces for-in loops with c-style for loops where it can." };

static bool optimization_states[LITOPTSTATE_TOTAL];

static bool optimization_states_setup = false;
static bool any_optimization_enabled = false;

static void setup_optimization_states();

#if defined(LIT_DEBUG_OPTIMIZER)
void optdbg(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "optimizer: ");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
}
#else
    #define optdbg(msg, ...)
#endif

void lit_varlist_init(LitVarList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_varlist_destroy(LitState* state, LitVarList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitVariable), array->values, array->capacity);
    lit_varlist_init(array);
}

void lit_varlist_push(LitState* state, LitVarList* array, LitVariable value)
{
    size_t old_capacity;
    if(array->capacity < array->count + 1)
    {
        old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitVariable), old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void lit_init_optimizer(LitState* state, LitOptimizer* optimizer)
{
    optimizer->state = state;
    optimizer->depth = -1;
    optimizer->mark_used = false;
    lit_varlist_init(&optimizer->variables);
}

static void opt_begin_scope(LitOptimizer* optimizer)
{
    optimizer->depth++;
}

static void opt_end_scope(LitOptimizer* optimizer)
{
    bool remove_unused;
    LitVariable* variable;
    LitVarList* variables;
    optimizer->depth--;
    variables = &optimizer->variables;
    remove_unused = lit_is_optimization_enabled(LITOPTSTATE_UNUSED_VAR);
    while(variables->count > 0 && variables->values[variables->count - 1].depth > optimizer->depth)
    {
        if(remove_unused && !variables->values[variables->count - 1].used)
        {
            variable = &variables->values[variables->count - 1];
            lit_free_expression(optimizer->state, *variable->declaration);
            *variable->declaration = NULL;
        }
        variables->count--;
    }
}

static LitVariable* add_variable(LitOptimizer* optimizer, const char* name, size_t length, bool constant, LitExpression** declaration)
{
    lit_varlist_push(optimizer->state, &optimizer->variables,
                        (LitVariable){ name, length, optimizer->depth, constant, optimizer->mark_used, NULL_VALUE, declaration });

    return &optimizer->variables.values[optimizer->variables.count - 1];
}

static LitVariable* resolve_variable(LitOptimizer* optimizer, const char* name, size_t length)
{
    int i;
    LitVarList* variables;
    LitVariable* variable;
    variables = &optimizer->variables;
    for(i = variables->count - 1; i >= 0; i--)
    {
        variable = &variables->values[i];
        if(length == variable->length && memcmp(variable->name, name, length) == 0)
        {
            return variable;
        }
    }
    return NULL;
}

static bool is_empty(LitExpression* statement)
{
    return statement == NULL || (statement->type == LITSTMT_BLOCK && ((LitBlockStatement*)statement)->statements.count == 0);
}

static LitValue evaluate_unary_op(LitOptimizer* optimizer, LitValue value, LitTokenType op)
{
    switch(op)
    {
        case LITTOK_MINUS:
            {
                if(lit_value_isnumber(value))
                {
                    optdbg("translating constant unary minus on number to literal value");
                    return lit_value_numbertovalue(optimizer->state, -lit_value_asnumber(value));
                }
            }
            break;
        case LITTOK_BANG:
            {
                optdbg("translating constant expression of '=' to literal value");
                return lit_bool_to_value(optimizer->state, lit_value_isfalsey(value));
            }
            break;
        case LITTOK_TILDE:
            {
                if(lit_value_isnumber(value))
                {
                    optdbg("translating unary tile (~) on number to literal value");
                    return lit_value_numbertovalue(optimizer->state, ~((int)lit_value_asnumber(value)));
                }
            }
            break;
        default:
            {
            }
            break;
    }
    return NULL_VALUE;
}

static LitValue evaluate_binary_op(LitOptimizer* optimizer, LitValue a, LitValue b, LitTokenType op)
{
    switch(op)
    {
        case LITTOK_PLUS:
            {
                optc_do_binary_op(+);
            }
            break;
        case LITTOK_MINUS:
            {
                optc_do_binary_op(-);
            }
            break;
        case LITTOK_STAR:
            {
                optc_do_binary_op(*);
            }
            break;
        case LITTOK_SLASH:
            {
                optc_do_binary_op(/);
            }
            break;
        case LITTOK_STAR_STAR:
            {
                optc_do_fn_op(pow, "**");
            }
            break;
        case LITTOK_PERCENT:
            {
                optc_do_fn_op(fmod, "%");
            }
            break;
        case LITTOK_GREATER:
            {
                optc_do_binary_op(>);
            }
            break;
        case LITTOK_GREATER_EQUAL:
            {
                optc_do_binary_op(>=);
            }
            break;
        case LITTOK_LESS:
            {
                optc_do_binary_op(<);
            }
            break;
        case LITTOK_LESS_EQUAL:
            {
                optc_do_binary_op(<=);
            }
            break;
        case LITTOK_LESS_LESS:
            {
                optc_do_bitwise_op(<<);
            }
            break;
        case LITTOK_GREATER_GREATER:
            {
                optc_do_bitwise_op(>>);
            }
            break;
        case LITTOK_BAR:
            {
                optc_do_bitwise_op(|);
            }
            break;
        case LITTOK_AMPERSAND:
            {
                optc_do_bitwise_op(&);
            }
            break;
        case LITTOK_CARET:
            {
                optc_do_bitwise_op(^);
            }
            break;
        case LITTOK_SHARP:
            {
                if(lit_value_isnumber(a) && lit_value_isnumber(b))
                {
                    return lit_value_numbertovalue(optimizer->state, floor(lit_value_asnumber(a) / lit_value_asnumber(b)));
                }
                return NULL_VALUE;
            }
            break;
        case LITTOK_EQUAL_EQUAL:
            {
                return lit_bool_to_value(optimizer->state, a == b);
            }
            break;
        case LITTOK_BANG_EQUAL:
            {
                return lit_bool_to_value(optimizer->state, a != b);
            }
            break;
        case LITTOK_IS:
        default:
            {
            }
            break;
    }
    return NULL_VALUE;
}

static LitValue attempt_to_optimize_binary(LitOptimizer* optimizer, LitBinaryExpr* expression, LitValue value, bool left)
{
    double number;
    LitTokenType op;
    op = expression->op;
    LitExpression* branch;
    branch = left ? expression->left : expression->right;
    if(lit_value_isnumber(value))
    {
        number = lit_value_asnumber(value);
        if(op == LITTOK_STAR)
        {
            if(number == 0)
            {
                optdbg("reducing expression to literal '0'");
                return lit_value_numbertovalue(optimizer->state, 0);
            }
            else if(number == 1)
            {
                optdbg("reducing expression to literal '1'");
                lit_free_expression(optimizer->state, left ? expression->right : expression->left);
                expression->left = branch;
                expression->right = NULL;
            }
        }
        else if((op == LITTOK_PLUS || op == LITTOK_MINUS) && number == 0)
        {
            optdbg("reducing expression that would result in '0' to literal '0'");
            lit_free_expression(optimizer->state, left ? expression->right : expression->left);
            expression->left = branch;
            expression->right = NULL;
        }
        else if(((left && op == LITTOK_SLASH) || op == LITTOK_STAR_STAR) && number == 1)
        {
            optdbg("reducing expression that would result in '1' to literal '1'");
            lit_free_expression(optimizer->state, left ? expression->right : expression->left);
            expression->left = branch;
            expression->right = NULL;
        }
    }
    return NULL_VALUE;
}

static LitValue evaluate_expression(LitOptimizer* optimizer, LitExpression* expression)
{
    LitUnaryExpr* uexpr;
    LitBinaryExpr* bexpr;
    LitValue a;
    LitValue b;
    LitValue branch;
    if(expression == NULL)
    {
        return NULL_VALUE;
    }
    switch(expression->type)
    {
        case LITEXPR_LITERAL:
            {
                return ((LitLiteralExpr*)expression)->value;
            }
            break;
        case LITEXPR_UNARY:
            {
                uexpr = (LitUnaryExpr*)expression;
                branch = evaluate_expression(optimizer, uexpr->right);
                if(branch != NULL_VALUE)
                {
                    return evaluate_unary_op(optimizer, branch, uexpr->op);
                }
            }
            break;
        case LITEXPR_BINARY:
            {
                bexpr = (LitBinaryExpr*)expression;
                a = evaluate_expression(optimizer, bexpr->left);
                b = evaluate_expression(optimizer, bexpr->right);
                if(a != NULL_VALUE && b != NULL_VALUE)
                {
                    return evaluate_binary_op(optimizer, a, b, bexpr->op);
                }
                else if(a != NULL_VALUE)
                {
                    return attempt_to_optimize_binary(optimizer, bexpr, a, false);
                }
                else if(b != NULL_VALUE)
                {
                    return attempt_to_optimize_binary(optimizer, bexpr, b, true);
                }
            }
            break;
        default:
            {
                return NULL_VALUE;
            }
            break;
    }
    return NULL_VALUE;
}

static void optimize_expression(LitOptimizer* optimizer, LitExpression** slot)
{

    LitExpression* expression = *slot;

    if(expression == NULL)
    {
        return;
    }

    LitState* state = optimizer->state;

    switch(expression->type)
    {
        case LITEXPR_UNARY:
        case LITEXPR_BINARY:
            {
                if(lit_is_optimization_enabled(LITOPTSTATE_LITERAL_FOLDING))
                {
                    LitValue optimized = evaluate_expression(optimizer, expression);
                    if(optimized != NULL_VALUE)
                    {
                        *slot = (LitExpression*)lit_create_literal_expression(state, expression->line, optimized);
                        lit_free_expression(state, expression);
                        break;
                    }
                }
                switch(expression->type)
                {
                    case LITEXPR_UNARY:
                        {
                            optimize_expression(optimizer, &((LitUnaryExpr*)expression)->right);
                        }
                        break;
                    case LITEXPR_BINARY:
                        {
                            LitBinaryExpr* expr = (LitBinaryExpr*)expression;
                            optimize_expression(optimizer, &expr->left);
                            optimize_expression(optimizer, &expr->right);
                        }
                        break;
                    default:
                        {
                            UNREACHABLE
                        }
                        break;
                }
            }
            break;
        case LITEXPR_ASSIGN:
            {
                LitAssignExpression* expr = (LitAssignExpression*)expression;
                optimize_expression(optimizer, &expr->to);
                optimize_expression(optimizer, &expr->value);
            }
            break;
        case LITEXPR_CALL:
            {
                LitCallExpression* expr = (LitCallExpression*)expression;
                optimize_expression(optimizer, &expr->callee);
                optimize_expressions(optimizer, &expr->args);
            }
            break;
        case LITEXPR_SET:
            {
                LitSetExpression* expr = (LitSetExpression*)expression;
                optimize_expression(optimizer, &expr->where);
                optimize_expression(optimizer, &expr->value);
            }
            break;
        case LITEXPR_GET:
            {
                optimize_expression(optimizer, &((LitGetExpression*)expression)->where);
            }
            break;
        case LITEXPR_LAMBDA:
            {
                opt_begin_scope(optimizer);
                optimize_statement(optimizer, &((LitLambdaExpression*)expression)->body);
                opt_end_scope(optimizer);
            }
            break;

        case LITEXPR_ARRAY:
        {
            optimize_expressions(optimizer, &((LitArrayExpression*)expression)->values);
            break;
        }

        case LITEXPR_OBJECT:
        {
            optimize_expressions(optimizer, &((LitObjectExpression*)expression)->values);
            break;
        }

        case LITEXPR_SUBSCRIPT:
        {
            LitSubscriptExpression* expr = (LitSubscriptExpression*)expression;

            optimize_expression(optimizer, &expr->array);
            optimize_expression(optimizer, &expr->index);

            break;
        }

        case LITEXPR_RANGE:
        {
            LitRangeExpression* expr = (LitRangeExpression*)expression;

            optimize_expression(optimizer, &expr->from);
            optimize_expression(optimizer, &expr->to);

            break;
        }

        case LITEXPR_IF:
        {
            LitIfExpression* expr = (LitIfExpression*)expression;
            LitValue optimized = evaluate_expression(optimizer, expr->condition);

            if(optimized != NULL_VALUE)
            {
                if(lit_value_isfalsey(optimized))
                {
                    *slot = expr->else_branch;
                    expr->else_branch = NULL;// So that it doesn't get freed
                }
                else
                {
                    *slot = expr->if_branch;
                    expr->if_branch = NULL;// So that it doesn't get freed
                }

                optimize_expression(optimizer, slot);
                lit_free_expression(state, expression);
            }
            else
            {
                optimize_expression(optimizer, &expr->if_branch);
                optimize_expression(optimizer, &expr->else_branch);
            }

            break;
        }

        case LITEXPR_INTERPOLATION:
        {
            optimize_expressions(optimizer, &((LitInterpolationExpression*)expression)->expressions);
            break;
        }

        case LITEXPR_VAR:
        {
            LitVarExpr* expr = (LitVarExpr*)expression;
            LitVariable* variable = resolve_variable(optimizer, expr->name, expr->length);

            if(variable != NULL)
            {
                variable->used = true;

                // Not checking here for the enable-ness of constant-folding, since if its off
                // the constant_value would be NULL_VALUE anyway (:thinkaboutit:)
                if(variable->constant && variable->constant_value != NULL_VALUE)
                {
                    *slot = (LitExpression*)lit_create_literal_expression(state, expression->line, variable->constant_value);
                    lit_free_expression(state, expression);
                }
            }

            break;
        }

        case LITEXPR_REFERENCE:
        {
            optimize_expression(optimizer, &((LitReferenceExpression*)expression)->to);
            break;
        }

        case LITEXPR_LITERAL:
        case LITEXPR_THIS:
        case LITEXPR_SUPER:
        {
            // Nothing, that we can do here
            break;
        }
    }
}

static void optimize_expressions(LitOptimizer* optimizer, LitExprList* expressions)
{
    for(size_t i = 0; i < expressions->count; i++)
    {
        optimize_expression(optimizer, &expressions->values[i]);
    }
}

static void optimize_statement(LitOptimizer* optimizer, LitExpression** slot)
{
    LitState* state;
    LitExpression* statement;
    statement = *slot;
    if(statement == NULL)
    {
        return;
    }
    state = optimizer->state;
    switch(statement->type)
    {
        case LITSTMT_EXPRESSION:
            {
                optimize_expression(optimizer, &((LitExpressionStatement*)statement)->expression);
            }
            break;
        case LITSTMT_BLOCK:
            {
                LitBlockStatement* stmt;
                stmt = (LitBlockStatement*)statement;
                if(stmt->statements.count == 0)
                {
                    lit_free_expression(state, statement);
                    *slot = NULL;
                    break;
                }
                opt_begin_scope(optimizer);
                optimize_statements(optimizer, &stmt->statements);
                opt_end_scope(optimizer);
                bool found = false;
                for(size_t i = 0; i < stmt->statements.count; i++)
                {
                    LitExpression* step = stmt->statements.values[i];
                    if(!is_empty(step))
                    {
                        found = true;
                        if(step->type == LITSTMT_RETURN)
                        {
                            // Remove all the statements post return
                            for(size_t j = i + 1; j < stmt->statements.count; j++)
                            {
                                step = stmt->statements.values[j];
                                if(step != NULL)
                                {
                                    lit_free_expression(state, step);
                                    stmt->statements.values[j] = NULL;
                                }
                            }
                            stmt->statements.count = i + 1;
                            break;
                        }
                    }
                }
                if(!found && lit_is_optimization_enabled(LITOPTSTATE_EMPTY_BODY))
                {
                    lit_free_expression(optimizer->state, statement);
                    *slot = NULL;
                }
            }
            break;

        case LITSTMT_IF:
        {
            LitIfStatement* stmt = (LitIfStatement*)statement;

            optimize_expression(optimizer, &stmt->condition);
            optimize_statement(optimizer, &stmt->if_branch);

            bool empty = lit_is_optimization_enabled(LITOPTSTATE_EMPTY_BODY);
            bool dead = lit_is_optimization_enabled(LITOPTSTATE_UNREACHABLE_CODE);

            LitValue optimized = empty ? evaluate_expression(optimizer, stmt->condition) : NULL_VALUE;

            if((optimized != NULL_VALUE && lit_value_isfalsey(optimized)) || (dead && is_empty(stmt->if_branch)))
            {
                lit_free_expression(state, stmt->condition);
                stmt->condition = NULL;

                lit_free_expression(state, stmt->if_branch);
                stmt->if_branch = NULL;
            }

            if(stmt->elseif_conditions != NULL)
            {
                optimize_expressions(optimizer, stmt->elseif_conditions);
                optimize_statements(optimizer, stmt->elseif_branches);

                if(dead || empty)
                {
                    for(size_t i = 0; i < stmt->elseif_conditions->count; i++)
                    {
                        if(empty && is_empty(stmt->elseif_branches->values[i]))
                        {
                            lit_free_expression(state, stmt->elseif_conditions->values[i]);
                            stmt->elseif_conditions->values[i] = NULL;

                            lit_free_expression(state, stmt->elseif_branches->values[i]);
                            stmt->elseif_branches->values[i] = NULL;

                            continue;
                        }

                        if(dead)
                        {
                            LitValue value = evaluate_expression(optimizer, stmt->elseif_conditions->values[i]);

                            if(value != NULL_VALUE && lit_value_isfalsey(value))
                            {
                                lit_free_expression(state, stmt->elseif_conditions->values[i]);
                                stmt->elseif_conditions->values[i] = NULL;

                                lit_free_expression(state, stmt->elseif_branches->values[i]);
                                stmt->elseif_branches->values[i] = NULL;
                            }
                        }
                    }
                }
            }

            optimize_statement(optimizer, &stmt->else_branch);
            break;
        }

        case LITSTMT_WHILE:
        {
            LitWhileStatement* stmt = (LitWhileStatement*)statement;
            optimize_expression(optimizer, &stmt->condition);

            if(lit_is_optimization_enabled(LITOPTSTATE_UNREACHABLE_CODE))
            {
                LitValue optimized = evaluate_expression(optimizer, stmt->condition);

                if(optimized != NULL_VALUE && lit_value_isfalsey(optimized))
                {
                    lit_free_expression(optimizer->state, statement);
                    *slot = NULL;
                    break;
                }
            }

            optimize_statement(optimizer, &stmt->body);

            if(lit_is_optimization_enabled(LITOPTSTATE_EMPTY_BODY) && is_empty(stmt->body))
            {
                lit_free_expression(optimizer->state, statement);
                *slot = NULL;
            }

            break;
        }

        case LITSTMT_FOR:
            {
                LitForStatement* stmt = (LitForStatement*)statement;
                opt_begin_scope(optimizer);
                // This is required, so that optimizer doesn't optimize out our i variable (and such)
                optimizer->mark_used = true;
                optimize_expression(optimizer, &stmt->init);
                optimize_expression(optimizer, &stmt->condition);
                optimize_expression(optimizer, &stmt->increment);
                optimize_statement(optimizer, &stmt->var);
                optimizer->mark_used = false;
                optimize_statement(optimizer, &stmt->body);
                opt_end_scope(optimizer);
                if(lit_is_optimization_enabled(LITOPTSTATE_EMPTY_BODY) && is_empty(stmt->body))
                {
                    lit_free_expression(optimizer->state, statement);
                    *slot = NULL;
                    break;
                }
                if(stmt->c_style || !lit_is_optimization_enabled(LITOPTSTATE_C_FOR) || stmt->condition->type != LITEXPR_RANGE)
                {
                    break;
                }
                LitRangeExpression* range = (LitRangeExpression*)stmt->condition;
                LitValue from = evaluate_expression(optimizer, range->from);
                LitValue to = evaluate_expression(optimizer, range->to);
                if(!lit_value_isnumber(from) || !lit_value_isnumber(to))
                {
                    break;
                }
                bool reverse = lit_value_asnumber(from) > lit_value_asnumber(to);
                LitVarStatement* var = (LitVarStatement*)stmt->var;
                size_t line = range->expression.line;
                // var i = from
                var->init = range->from;
                // i <= to
                stmt->condition = (LitExpression*)lit_create_binary_expression(
                state, line, (LitExpression*)lit_create_var_expression(state, line, var->name, var->length), range->to, LITTOK_LESS_EQUAL);
                // i++ (or i--)
                LitExpression* var_get = (LitExpression*)lit_create_var_expression(state, line, var->name, var->length);
                LitBinaryExpr* assign_value = lit_create_binary_expression(
                state, line, var_get, (LitExpression*)lit_create_literal_expression(state, line, lit_value_numbertovalue(optimizer->state, 1)),
                reverse ? LITTOK_MINUS_MINUS : LITTOK_PLUS);
                assign_value->ignore_left = true;
                LitExpression* increment
                = (LitExpression*)lit_create_assign_expression(state, line, var_get, (LitExpression*)assign_value);
                stmt->increment = (LitExpression*)increment;
                range->from = NULL;
                range->to = NULL;
                stmt->c_style = true;
                lit_free_expression(state, (LitExpression*)range);
            }
            break;

        case LITSTMT_VAR:
            {
                LitVarStatement* stmt = (LitVarStatement*)statement;
                LitVariable* variable = add_variable(optimizer, stmt->name, stmt->length, stmt->constant, slot);
                optimize_expression(optimizer, &stmt->init);
                if(stmt->constant && lit_is_optimization_enabled(LITOPTSTATE_CONSTANT_FOLDING))
                {
                    LitValue value = evaluate_expression(optimizer, stmt->init);

                    if(value != NULL_VALUE)
                    {
                        variable->constant_value = value;
                    }
                }
            }
            break;
        case LITSTMT_FUNCTION:
            {
                LitFunctionStatement* stmt = (LitFunctionStatement*)statement;
                LitVariable* variable = add_variable(optimizer, stmt->name, stmt->length, false, slot);
                if(stmt->exported)
                {
                    // Otherwise it will get optimized-out with a big chance
                    variable->used = true;
                }
                opt_begin_scope(optimizer);
                optimize_statement(optimizer, &stmt->body);
                opt_end_scope(optimizer);
            }
            break;
        case LITSTMT_RETURN:
            {
                optimize_expression(optimizer, &((LitReturnStatement*)statement)->expression);
            }
            break;
        case LITSTMT_METHOD:
            {
                opt_begin_scope(optimizer);
                optimize_statement(optimizer, &((LitMethodStatement*)statement)->body);
                opt_end_scope(optimizer);
            }
            break;
        case LITSTMT_CLASS:
            {
                optimize_statements(optimizer, &((LitClassStatement*)statement)->fields);
            }
            break;
        case LITSTMT_FIELD:
            {
                LitFieldStatement* stmt = (LitFieldStatement*)statement;
                if(stmt->getter != NULL)
                {
                    opt_begin_scope(optimizer);
                    optimize_statement(optimizer, &stmt->getter);
                    opt_end_scope(optimizer);
                }
                if(stmt->setter != NULL)
                {
                    opt_begin_scope(optimizer);
                    optimize_statement(optimizer, &stmt->setter);
                    opt_end_scope(optimizer);
                }
            }
            break;
        // Nothing to optimize there
        case LITSTMT_CONTINUE:
        case LITSTMT_BREAK:
            {
            }
            break;

    }
}

static void optimize_statements(LitOptimizer* optimizer, LitExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        optimize_statement(optimizer, &statements->values[i]);
    }
}

void lit_optimize(LitOptimizer* optimizer, LitExprList* statements)
{
    return;
    if(!optimization_states_setup)
    {
        setup_optimization_states();
    }
    if(!any_optimization_enabled)
    {
        return;
    }
    opt_begin_scope(optimizer);
    optimize_statements(optimizer, statements);
    opt_end_scope(optimizer);
    lit_varlist_destroy(optimizer->state, &optimizer->variables);
}

static void setup_optimization_states()
{
    lit_set_optimization_level(LITOPTLEVEL_DEBUG);
}

bool lit_is_optimization_enabled(LitOptimization optimization)
{
    if(!optimization_states_setup)
    {
        setup_optimization_states();
    }
    return optimization_states[(int)optimization];
}

void lit_set_optimization_enabled(LitOptimization optimization, bool enabled)
{
    size_t i;
    if(!optimization_states_setup)
    {
        setup_optimization_states();
    }
    optimization_states[(int)optimization] = enabled;
    if(enabled)
    {
        any_optimization_enabled = true;
    }
    else
    {
        for(i = 0; i < LITOPTSTATE_TOTAL; i++)
        {
            if(optimization_states[i])
            {
                return;
            }
        }
        any_optimization_enabled = false;
    }
}

void lit_set_all_optimization_enabled(bool enabled)
{
    size_t i;
    optimization_states_setup = true;
    any_optimization_enabled = enabled;
    for(i = 0; i < LITOPTSTATE_TOTAL; i++)
    {
        optimization_states[i] = enabled;
    }
}

void lit_set_optimization_level(LitOptimizationLevel level)
{
    switch(level)
    {
        case LITOPTLEVEL_NONE:
            {
                lit_set_all_optimization_enabled(false);
            }
            break;
        case LITOPTLEVEL_REPL:
            {
                lit_set_all_optimization_enabled(true);
                lit_set_optimization_enabled(LITOPTSTATE_UNUSED_VAR, false);
                lit_set_optimization_enabled(LITOPTSTATE_UNREACHABLE_CODE, false);
                lit_set_optimization_enabled(LITOPTSTATE_EMPTY_BODY, false);
                lit_set_optimization_enabled(LITOPTSTATE_LINE_INFO, false);
                lit_set_optimization_enabled(LITOPTSTATE_PRIVATE_NAMES, false);
            }
            break;
        case LITOPTLEVEL_DEBUG:
            {
                lit_set_all_optimization_enabled(true);
                lit_set_optimization_enabled(LITOPTSTATE_UNUSED_VAR, false);
                lit_set_optimization_enabled(LITOPTSTATE_LINE_INFO, false);
                lit_set_optimization_enabled(LITOPTSTATE_PRIVATE_NAMES, false);
            }
            break;
        case LITOPTLEVEL_RELEASE:
            {
                lit_set_all_optimization_enabled(true);
                lit_set_optimization_enabled(LITOPTSTATE_LINE_INFO, false);
            }
            break;
        case LITOPTLEVEL_EXTREME:
            {
                lit_set_all_optimization_enabled(true);
            }
            break;
        case LITOPTLEVEL_TOTAL:
            {
            }
            break;

    }
}

const char* lit_get_optimization_name(LitOptimization optimization)
{
    return optimization_names[(int)optimization];
}

const char* lit_get_optimization_description(LitOptimization optimization)
{
    return optimization_descriptions[(int)optimization];
}

const char* lit_get_optimization_level_description(LitOptimizationLevel level)
{
    return optimization_level_descriptions[(int)level];
}

