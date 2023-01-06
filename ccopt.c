
#include "lit.h"

#define LIT_DEBUG_OPTIMIZER

#define optc_do_binary_op(op) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        lit_astopt_optdbg("translating constant binary expression of '" # op "' to constant value"); \
        return lit_value_numbertovalue(optimizer->state, lit_value_asnumber(a) op lit_value_asnumber(b)); \
    } \
    return NULL_VALUE;

#define optc_do_bitwise_op(op) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        lit_astopt_optdbg("translating constant bitwise expression of '" #op "' to constant value"); \
        return lit_value_numbertovalue(optimizer->state, (int)lit_value_asnumber(a) op(int) lit_value_asnumber(b)); \
    } \
    return NULL_VALUE;

#define optc_do_fn_op(fn, tokstr) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        lit_astopt_optdbg("translating constant expression of '" tokstr "' to constant value via lit_vm_callcallable to '" #fn "'"); \
        return lit_value_numbertovalue(optimizer->state, fn(lit_value_asnumber(a), lit_value_asnumber(b))); \
    } \
    return NULL_VALUE;



static void lit_astopt_optexpression(LitOptimizer* optimizer, LitAstExpression** slot);
static void lit_astopt_optexprlist(LitOptimizer* optimizer, LitAstExprList* expressions);
static void lit_astopt_optstmtlist(LitOptimizer* optimizer, LitAstExprList* statements);
static void lit_asdtopt_optstatement(LitOptimizer* optimizer, LitAstExpression** slot);

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

static void lit_astopt_setupstates();

#if defined(LIT_DEBUG_OPTIMIZER)
void lit_astopt_optdbg(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "optimizer: ");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
}
#else
    #define lit_astopt_optdbg(msg, ...)
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

void lit_astopt_init(LitState* state, LitOptimizer* optimizer)
{
    optimizer->state = state;
    optimizer->depth = -1;
    optimizer->mark_used = false;
    lit_varlist_init(&optimizer->variables);
}

static void lit_astopt_beginscope(LitOptimizer* optimizer)
{
    optimizer->depth++;
}

static void lit_astopt_endscope(LitOptimizer* optimizer)
{
    bool remove_unused;
    LitVariable* variable;
    LitVarList* variables;
    optimizer->depth--;
    variables = &optimizer->variables;
    remove_unused = lit_astopt_isoptenabled(LITOPTSTATE_UNUSED_VAR);
    while(variables->count > 0 && variables->values[variables->count - 1].depth > optimizer->depth)
    {
        if(remove_unused && !variables->values[variables->count - 1].used)
        {
            variable = &variables->values[variables->count - 1];
            lit_ast_destroyexpression(optimizer->state, *variable->declaration);
            *variable->declaration = NULL;
        }
        variables->count--;
    }
}

static LitVariable* lit_astopt_addvar(LitOptimizer* optimizer, const char* name, size_t length, bool constant, LitAstExpression** declaration)
{
    lit_varlist_push(optimizer->state, &optimizer->variables,
                        (LitVariable){ name, length, optimizer->depth, constant, optimizer->mark_used, NULL_VALUE, declaration });

    return &optimizer->variables.values[optimizer->variables.count - 1];
}

static LitVariable* lit_astopt_resolvevar(LitOptimizer* optimizer, const char* name, size_t length)
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

static bool lit_astopt_isemptyexpr(LitAstExpression* statement)
{
    return statement == NULL || (statement->type == LITEXPR_BLOCK && ((LitAstBlockExpr*)statement)->statements.count == 0);
}

static LitValue lit_astopt_evalunaryop(LitOptimizer* optimizer, LitValue value, LitTokType op)
{
    switch(op)
    {
        case LITTOK_MINUS:
            {
                if(lit_value_isnumber(value))
                {
                    lit_astopt_optdbg("translating constant unary minus on number to literal value");
                    return lit_value_numbertovalue(optimizer->state, -lit_value_asnumber(value));
                }
            }
            break;
        case LITTOK_BANG:
            {
                lit_astopt_optdbg("translating constant expression of '=' to literal value");
                return lit_bool_to_value(optimizer->state, lit_value_isfalsey(value));
            }
            break;
        case LITTOK_TILDE:
            {
                if(lit_value_isnumber(value))
                {
                    lit_astopt_optdbg("translating unary tile (~) on number to literal value");
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

static LitValue lit_astopt_evalbinaryop(LitOptimizer* optimizer, LitValue a, LitValue b, LitTokType op)
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

static LitValue lit_astopt_attemptoptbinary(LitOptimizer* optimizer, LitAstBinaryExpr* expression, LitValue value, bool left)
{
    double number;
    LitTokType op;
    op = expression->op;
    LitAstExpression* branch;
    branch = left ? expression->left : expression->right;
    if(lit_value_isnumber(value))
    {
        number = lit_value_asnumber(value);
        if(op == LITTOK_STAR)
        {
            if(number == 0)
            {
                lit_astopt_optdbg("reducing expression to literal '0'");
                return lit_value_numbertovalue(optimizer->state, 0);
            }
            else if(number == 1)
            {
                lit_astopt_optdbg("reducing expression to literal '1'");
                lit_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
                expression->left = branch;
                expression->right = NULL;
            }
        }
        else if((op == LITTOK_PLUS || op == LITTOK_MINUS) && number == 0)
        {
            lit_astopt_optdbg("reducing expression that would result in '0' to literal '0'");
            lit_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
            expression->left = branch;
            expression->right = NULL;
        }
        else if(((left && op == LITTOK_SLASH) || op == LITTOK_STAR_STAR) && number == 1)
        {
            lit_astopt_optdbg("reducing expression that would result in '1' to literal '1'");
            lit_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
            expression->left = branch;
            expression->right = NULL;
        }
    }
    return NULL_VALUE;
}

static LitValue lit_astopt_evalexpr(LitOptimizer* optimizer, LitAstExpression* expression)
{
    LitAstUnaryExpr* uexpr;
    LitAstBinaryExpr* bexpr;
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
                return ((LitAstLiteralExpr*)expression)->value;
            }
            break;
        case LITEXPR_UNARY:
            {
                uexpr = (LitAstUnaryExpr*)expression;
                branch = lit_astopt_evalexpr(optimizer, uexpr->right);
                if(branch != NULL_VALUE)
                {
                    return lit_astopt_evalunaryop(optimizer, branch, uexpr->op);
                }
            }
            break;
        case LITEXPR_BINARY:
            {
                bexpr = (LitAstBinaryExpr*)expression;
                a = lit_astopt_evalexpr(optimizer, bexpr->left);
                b = lit_astopt_evalexpr(optimizer, bexpr->right);
                if(a != NULL_VALUE && b != NULL_VALUE)
                {
                    return lit_astopt_evalbinaryop(optimizer, a, b, bexpr->op);
                }
                else if(a != NULL_VALUE)
                {
                    return lit_astopt_attemptoptbinary(optimizer, bexpr, a, false);
                }
                else if(b != NULL_VALUE)
                {
                    return lit_astopt_attemptoptbinary(optimizer, bexpr, b, true);
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

static void lit_astopt_optexpression(LitOptimizer* optimizer, LitAstExpression** slot)
{

    LitAstExpression* expression = *slot;

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
                if(lit_astopt_isoptenabled(LITOPTSTATE_LITERAL_FOLDING))
                {
                    LitValue optimized = lit_astopt_evalexpr(optimizer, expression);
                    if(optimized != NULL_VALUE)
                    {
                        *slot = (LitAstExpression*)lit_ast_make_literalexpr(state, expression->line, optimized);
                        lit_ast_destroyexpression(state, expression);
                        break;
                    }
                }
                switch(expression->type)
                {
                    case LITEXPR_UNARY:
                        {
                            lit_astopt_optexpression(optimizer, &((LitAstUnaryExpr*)expression)->right);
                        }
                        break;
                    case LITEXPR_BINARY:
                        {
                            LitAstBinaryExpr* expr = (LitAstBinaryExpr*)expression;
                            lit_astopt_optexpression(optimizer, &expr->left);
                            lit_astopt_optexpression(optimizer, &expr->right);
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
                LitAstAssignExpr* expr = (LitAstAssignExpr*)expression;
                lit_astopt_optexpression(optimizer, &expr->to);
                lit_astopt_optexpression(optimizer, &expr->value);
            }
            break;
        case LITEXPR_CALL:
            {
                LitAstCallExpr* expr = (LitAstCallExpr*)expression;
                lit_astopt_optexpression(optimizer, &expr->callee);
                lit_astopt_optexprlist(optimizer, &expr->args);
            }
            break;
        case LITEXPR_SET:
            {
                LitAstSetExpr* expr = (LitAstSetExpr*)expression;
                lit_astopt_optexpression(optimizer, &expr->where);
                lit_astopt_optexpression(optimizer, &expr->value);
            }
            break;
        case LITEXPR_GET:
            {
                lit_astopt_optexpression(optimizer, &((LitAstGetExpr*)expression)->where);
            }
            break;
        case LITEXPR_LAMBDA:
            {
                lit_astopt_beginscope(optimizer);
                lit_asdtopt_optstatement(optimizer, &((LitAstLambdaExpr*)expression)->body);
                lit_astopt_endscope(optimizer);
            }
            break;

        case LITEXPR_ARRAY:
        {
            lit_astopt_optexprlist(optimizer, &((LitAstArrayExpr*)expression)->values);
            break;
        }

        case LITEXPR_OBJECT:
        {
            lit_astopt_optexprlist(optimizer, &((LitAstObjectExpr*)expression)->values);
            break;
        }

        case LITEXPR_SUBSCRIPT:
        {
            LitAstIndexExpr* expr = (LitAstIndexExpr*)expression;

            lit_astopt_optexpression(optimizer, &expr->array);
            lit_astopt_optexpression(optimizer, &expr->index);

            break;
        }

        case LITEXPR_RANGE:
        {
            LitAstRangeExpr* expr = (LitAstRangeExpr*)expression;

            lit_astopt_optexpression(optimizer, &expr->from);
            lit_astopt_optexpression(optimizer, &expr->to);

            break;
        }

        case LITEXPR_TERNARY:
        {
            LitAstTernaryExpr* expr = (LitAstTernaryExpr*)expression;
            LitValue optimized = lit_astopt_evalexpr(optimizer, expr->condition);

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

                lit_astopt_optexpression(optimizer, slot);
                lit_ast_destroyexpression(state, expression);
            }
            else
            {
                lit_astopt_optexpression(optimizer, &expr->if_branch);
                lit_astopt_optexpression(optimizer, &expr->else_branch);
            }

            break;
        }

        case LITEXPR_INTERPOLATION:
        {
            lit_astopt_optexprlist(optimizer, &((LitAstStrInterExpr*)expression)->expressions);
            break;
        }

        case LITEXPR_VAREXPR:
        {
            LitAstVarExpr* expr = (LitAstVarExpr*)expression;
            LitVariable* variable = lit_astopt_resolvevar(optimizer, expr->name, expr->length);

            if(variable != NULL)
            {
                variable->used = true;

                // Not checking here for the enable-ness of constant-folding, since if its off
                // the constant_value would be NULL_VALUE anyway (:thinkaboutit:)
                if(variable->constant && variable->constant_value != NULL_VALUE)
                {
                    *slot = (LitAstExpression*)lit_ast_make_literalexpr(state, expression->line, variable->constant_value);
                    lit_ast_destroyexpression(state, expression);
                }
            }

            break;
        }

        case LITEXPR_REFERENCE:
        {
            lit_astopt_optexpression(optimizer, &((LitAstRefExpr*)expression)->to);
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

static void lit_astopt_optexprlist(LitOptimizer* optimizer, LitAstExprList* expressions)
{
    for(size_t i = 0; i < expressions->count; i++)
    {
        lit_astopt_optexpression(optimizer, &expressions->values[i]);
    }
}

static void lit_asdtopt_optstatement(LitOptimizer* optimizer, LitAstExpression** slot)
{
    LitState* state;
    LitAstExpression* statement;
    statement = *slot;
    if(statement == NULL)
    {
        return;
    }
    state = optimizer->state;
    switch(statement->type)
    {
        case LITEXPR_EXPRESSION:
            {
                lit_astopt_optexpression(optimizer, &((LitAstExprExpr*)statement)->expression);
            }
            break;
        case LITEXPR_BLOCK:
            {
                LitAstBlockExpr* stmt;
                stmt = (LitAstBlockExpr*)statement;
                if(stmt->statements.count == 0)
                {
                    lit_ast_destroyexpression(state, statement);
                    *slot = NULL;
                    break;
                }
                lit_astopt_beginscope(optimizer);
                lit_astopt_optstmtlist(optimizer, &stmt->statements);
                lit_astopt_endscope(optimizer);
                bool found = false;
                for(size_t i = 0; i < stmt->statements.count; i++)
                {
                    LitAstExpression* step = stmt->statements.values[i];
                    if(!lit_astopt_isemptyexpr(step))
                    {
                        found = true;
                        if(step->type == LITEXPR_RETURN)
                        {
                            // Remove all the statements post return
                            for(size_t j = i + 1; j < stmt->statements.count; j++)
                            {
                                step = stmt->statements.values[j];
                                if(step != NULL)
                                {
                                    lit_ast_destroyexpression(state, step);
                                    stmt->statements.values[j] = NULL;
                                }
                            }
                            stmt->statements.count = i + 1;
                            break;
                        }
                    }
                }
                if(!found && lit_astopt_isoptenabled(LITOPTSTATE_EMPTY_BODY))
                {
                    lit_ast_destroyexpression(optimizer->state, statement);
                    *slot = NULL;
                }
            }
            break;

        case LITEXPR_IFSTMT:
        {
            LitAstIfExpr* stmt = (LitAstIfExpr*)statement;

            lit_astopt_optexpression(optimizer, &stmt->condition);
            lit_asdtopt_optstatement(optimizer, &stmt->if_branch);

            bool empty = lit_astopt_isoptenabled(LITOPTSTATE_EMPTY_BODY);
            bool dead = lit_astopt_isoptenabled(LITOPTSTATE_UNREACHABLE_CODE);

            LitValue optimized = empty ? lit_astopt_evalexpr(optimizer, stmt->condition) : NULL_VALUE;

            if((optimized != NULL_VALUE && lit_value_isfalsey(optimized)) || (dead && lit_astopt_isemptyexpr(stmt->if_branch)))
            {
                lit_ast_destroyexpression(state, stmt->condition);
                stmt->condition = NULL;

                lit_ast_destroyexpression(state, stmt->if_branch);
                stmt->if_branch = NULL;
            }

            if(stmt->elseif_conditions != NULL)
            {
                lit_astopt_optexprlist(optimizer, stmt->elseif_conditions);
                lit_astopt_optstmtlist(optimizer, stmt->elseif_branches);

                if(dead || empty)
                {
                    for(size_t i = 0; i < stmt->elseif_conditions->count; i++)
                    {
                        if(empty && lit_astopt_isemptyexpr(stmt->elseif_branches->values[i]))
                        {
                            lit_ast_destroyexpression(state, stmt->elseif_conditions->values[i]);
                            stmt->elseif_conditions->values[i] = NULL;

                            lit_ast_destroyexpression(state, stmt->elseif_branches->values[i]);
                            stmt->elseif_branches->values[i] = NULL;

                            continue;
                        }

                        if(dead)
                        {
                            LitValue value = lit_astopt_evalexpr(optimizer, stmt->elseif_conditions->values[i]);

                            if(value != NULL_VALUE && lit_value_isfalsey(value))
                            {
                                lit_ast_destroyexpression(state, stmt->elseif_conditions->values[i]);
                                stmt->elseif_conditions->values[i] = NULL;

                                lit_ast_destroyexpression(state, stmt->elseif_branches->values[i]);
                                stmt->elseif_branches->values[i] = NULL;
                            }
                        }
                    }
                }
            }

            lit_asdtopt_optstatement(optimizer, &stmt->else_branch);
            break;
        }

        case LITEXPR_WHILE:
        {
            LitAstWhileExpr* stmt = (LitAstWhileExpr*)statement;
            lit_astopt_optexpression(optimizer, &stmt->condition);

            if(lit_astopt_isoptenabled(LITOPTSTATE_UNREACHABLE_CODE))
            {
                LitValue optimized = lit_astopt_evalexpr(optimizer, stmt->condition);

                if(optimized != NULL_VALUE && lit_value_isfalsey(optimized))
                {
                    lit_ast_destroyexpression(optimizer->state, statement);
                    *slot = NULL;
                    break;
                }
            }

            lit_asdtopt_optstatement(optimizer, &stmt->body);

            if(lit_astopt_isoptenabled(LITOPTSTATE_EMPTY_BODY) && lit_astopt_isemptyexpr(stmt->body))
            {
                lit_ast_destroyexpression(optimizer->state, statement);
                *slot = NULL;
            }

            break;
        }

        case LITEXPR_FOR:
            {
                LitAstForExpr* stmt = (LitAstForExpr*)statement;
                lit_astopt_beginscope(optimizer);
                // This is required, so that optimizer doesn't optimize out our i variable (and such)
                optimizer->mark_used = true;
                lit_astopt_optexpression(optimizer, &stmt->init);
                lit_astopt_optexpression(optimizer, &stmt->condition);
                lit_astopt_optexpression(optimizer, &stmt->increment);
                lit_asdtopt_optstatement(optimizer, &stmt->var);
                optimizer->mark_used = false;
                lit_asdtopt_optstatement(optimizer, &stmt->body);
                lit_astopt_endscope(optimizer);
                if(lit_astopt_isoptenabled(LITOPTSTATE_EMPTY_BODY) && lit_astopt_isemptyexpr(stmt->body))
                {
                    lit_ast_destroyexpression(optimizer->state, statement);
                    *slot = NULL;
                    break;
                }
                if(stmt->c_style || !lit_astopt_isoptenabled(LITOPTSTATE_C_FOR) || stmt->condition->type != LITEXPR_RANGE)
                {
                    break;
                }
                LitAstRangeExpr* range = (LitAstRangeExpr*)stmt->condition;
                LitValue from = lit_astopt_evalexpr(optimizer, range->from);
                LitValue to = lit_astopt_evalexpr(optimizer, range->to);
                if(!lit_value_isnumber(from) || !lit_value_isnumber(to))
                {
                    break;
                }
                bool reverse = lit_value_asnumber(from) > lit_value_asnumber(to);
                LitAstAssignVarExpr* var = (LitAstAssignVarExpr*)stmt->var;
                size_t line = range->exobj.line;
                // var i = from
                var->init = range->from;
                // i <= to
                stmt->condition = (LitAstExpression*)lit_ast_make_binaryexpr(
                state, line, (LitAstExpression*)lit_ast_make_varexpr(state, line, var->name, var->length), range->to, LITTOK_LESS_EQUAL);
                // i++ (or i--)
                LitAstExpression* var_get = (LitAstExpression*)lit_ast_make_varexpr(state, line, var->name, var->length);
                LitAstBinaryExpr* assign_value = lit_ast_make_binaryexpr(
                state, line, var_get, (LitAstExpression*)lit_ast_make_literalexpr(state, line, lit_value_numbertovalue(optimizer->state, 1)),
                reverse ? LITTOK_MINUS_MINUS : LITTOK_PLUS);
                assign_value->ignore_left = true;
                LitAstExpression* increment
                = (LitAstExpression*)lit_ast_make_assignexpr(state, line, var_get, (LitAstExpression*)assign_value);
                stmt->increment = (LitAstExpression*)increment;
                range->from = NULL;
                range->to = NULL;
                stmt->c_style = true;
                lit_ast_destroyexpression(state, (LitAstExpression*)range);
            }
            break;

        case LITEXPR_VARSTMT:
            {
                LitAstAssignVarExpr* stmt = (LitAstAssignVarExpr*)statement;
                LitVariable* variable = lit_astopt_addvar(optimizer, stmt->name, stmt->length, stmt->constant, slot);
                lit_astopt_optexpression(optimizer, &stmt->init);
                if(stmt->constant && lit_astopt_isoptenabled(LITOPTSTATE_CONSTANT_FOLDING))
                {
                    LitValue value = lit_astopt_evalexpr(optimizer, stmt->init);

                    if(value != NULL_VALUE)
                    {
                        variable->constant_value = value;
                    }
                }
            }
            break;
        case LITEXPR_FUNCTION:
            {
                LitAstFunctionExpr* stmt = (LitAstFunctionExpr*)statement;
                LitVariable* variable = lit_astopt_addvar(optimizer, stmt->name, stmt->length, false, slot);
                if(stmt->exported)
                {
                    // Otherwise it will get optimized-out with a big chance
                    variable->used = true;
                }
                lit_astopt_beginscope(optimizer);
                lit_asdtopt_optstatement(optimizer, &stmt->body);
                lit_astopt_endscope(optimizer);
            }
            break;
        case LITEXPR_RETURN:
            {
                lit_astopt_optexpression(optimizer, &((LitAstReturnExpr*)statement)->expression);
            }
            break;
        case LITEXPR_METHOD:
            {
                lit_astopt_beginscope(optimizer);
                lit_asdtopt_optstatement(optimizer, &((LitAstMethodExpr*)statement)->body);
                lit_astopt_endscope(optimizer);
            }
            break;
        case LITEXPR_CLASS:
            {
                lit_astopt_optstmtlist(optimizer, &((LitAstClassExpr*)statement)->fields);
            }
            break;
        case LITEXPR_FIELD:
            {
                LitAstFieldExpr* stmt = (LitAstFieldExpr*)statement;
                if(stmt->getter != NULL)
                {
                    lit_astopt_beginscope(optimizer);
                    lit_asdtopt_optstatement(optimizer, &stmt->getter);
                    lit_astopt_endscope(optimizer);
                }
                if(stmt->setter != NULL)
                {
                    lit_astopt_beginscope(optimizer);
                    lit_asdtopt_optstatement(optimizer, &stmt->setter);
                    lit_astopt_endscope(optimizer);
                }
            }
            break;
        // Nothing to optimize there
        case LITEXPR_CONTINUE:
        case LITEXPR_BREAK:
            {
            }
            break;

    }
}

static void lit_astopt_optstmtlist(LitOptimizer* optimizer, LitAstExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        lit_asdtopt_optstatement(optimizer, &statements->values[i]);
    }
}

void lit_astopt_optast(LitOptimizer* optimizer, LitAstExprList* statements)
{
    return;
    if(!optimization_states_setup)
    {
        lit_astopt_setupstates();
    }
    if(!any_optimization_enabled)
    {
        return;
    }
    lit_astopt_beginscope(optimizer);
    lit_astopt_optstmtlist(optimizer, statements);
    lit_astopt_endscope(optimizer);
    lit_varlist_destroy(optimizer->state, &optimizer->variables);
}

static void lit_astopt_setupstates()
{
    lit_astopt_setoptlevel(LITOPTLEVEL_DEBUG);
}

bool lit_astopt_isoptenabled(LitOptimization optimization)
{
    if(!optimization_states_setup)
    {
        lit_astopt_setupstates();
    }
    return optimization_states[(int)optimization];
}

void lit_astopt_setoptenabled(LitOptimization optimization, bool enabled)
{
    size_t i;
    if(!optimization_states_setup)
    {
        lit_astopt_setupstates();
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

void lit_astopt_setalloptenabled(bool enabled)
{
    size_t i;
    optimization_states_setup = true;
    any_optimization_enabled = enabled;
    for(i = 0; i < LITOPTSTATE_TOTAL; i++)
    {
        optimization_states[i] = enabled;
    }
}

void lit_astopt_setoptlevel(LitOptLevel level)
{
    switch(level)
    {
        case LITOPTLEVEL_NONE:
            {
                lit_astopt_setalloptenabled(false);
            }
            break;
        case LITOPTLEVEL_REPL:
            {
                lit_astopt_setalloptenabled(true);
                lit_astopt_setoptenabled(LITOPTSTATE_UNUSED_VAR, false);
                lit_astopt_setoptenabled(LITOPTSTATE_UNREACHABLE_CODE, false);
                lit_astopt_setoptenabled(LITOPTSTATE_EMPTY_BODY, false);
                lit_astopt_setoptenabled(LITOPTSTATE_LINE_INFO, false);
                lit_astopt_setoptenabled(LITOPTSTATE_PRIVATE_NAMES, false);
            }
            break;
        case LITOPTLEVEL_DEBUG:
            {
                lit_astopt_setalloptenabled(true);
                lit_astopt_setoptenabled(LITOPTSTATE_UNUSED_VAR, false);
                lit_astopt_setoptenabled(LITOPTSTATE_LINE_INFO, false);
                lit_astopt_setoptenabled(LITOPTSTATE_PRIVATE_NAMES, false);
            }
            break;
        case LITOPTLEVEL_RELEASE:
            {
                lit_astopt_setalloptenabled(true);
                lit_astopt_setoptenabled(LITOPTSTATE_LINE_INFO, false);
            }
            break;
        case LITOPTLEVEL_EXTREME:
            {
                lit_astopt_setalloptenabled(true);
            }
            break;
        case LITOPTLEVEL_TOTAL:
            {
            }
            break;

    }
}

const char* lit_astopt_getoptname(LitOptimization optimization)
{
    return optimization_names[(int)optimization];
}

const char* lit_astopt_getoptdescr(LitOptimization optimization)
{
    return optimization_descriptions[(int)optimization];
}

const char* lit_astopt_getoptleveldescr(LitOptLevel level)
{
    return optimization_level_descriptions[(int)level];
}

