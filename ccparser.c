
#include <stdlib.h>
#include <setjmp.h>
#include "lit.h"
#include "priv.h"

static jmp_buf prs_jmpbuffer;
static LitParseRule rules[LITTOK_EOF + 1];


static LitTokType operators[]=
{
    LITTOK_PLUS, LITTOK_MINUS, LITTOK_STAR, LITTOK_PERCENT, LITTOK_SLASH,
    LITTOK_SHARP, LITTOK_BANG, LITTOK_LESS, LITTOK_LESS_EQUAL, LITTOK_GREATER,
    LITTOK_GREATER_EQUAL, LITTOK_EQUAL_EQUAL, LITTOK_LEFT_BRACKET, LITTOK_EOF
};


static bool did_setup_rules;
static void setup_rules();
static void sync(LitParser* parser);

static LitExpression *parse_block(LitParser *parser);
static LitExpression *parse_precedence(LitParser *parser, LitPrecedence precedence, bool err, bool ignsemi);
static LitExpression *parse_lambda(LitParser *parser, LitLambdaExpr *lambda);
static void parse_parameters(LitParser *parser, LitParamList *parameters);
static LitExpression *parse_expression(LitParser *parser, bool ignsemi);
static LitExpression *parse_var_declaration(LitParser *parser, bool ignsemi);
static LitExpression *parse_if(LitParser *parser);
static LitExpression *parse_for(LitParser *parser);
static LitExpression *parse_while(LitParser *parser);
static LitExpression *parse_return(LitParser *parser);
static LitExpression *parse_field(LitParser *parser, LitString *name, bool is_static);
static LitExpression *parse_method(LitParser *parser, bool is_static);
static LitExpression *parse_class(LitParser *parser);
static LitExpression *parse_statement(LitParser *parser);
static LitExpression *parse_declaration(LitParser *parser);

static LitExpression *prule_number(LitParser *parser, bool can_assign);
static LitExpression *prule_grouping_or_lambda(LitParser *parser, bool can_assign);
static LitExpression *prule_call(LitParser *parser, LitExpression *prev, bool can_assign);
static LitExpression *prule_unary(LitParser *parser, bool can_assign);
static LitExpression *prule_binary(LitParser *parser, LitExpression *prev, bool can_assign);
static LitExpression *prule_and(LitParser *parser, LitExpression *prev, bool can_assign);
static LitExpression *prule_or(LitParser *parser, LitExpression *prev, bool can_assign);
static LitExpression *prule_null_filter(LitParser *parser, LitExpression *prev, bool can_assign);
static LitExpression *prule_compound(LitParser *parser, LitExpression *prev, bool can_assign);
static LitExpression *prule_literal(LitParser *parser, bool can_assign);
static LitExpression *prule_string(LitParser *parser, bool can_assign);
static LitExpression *prule_interpolation(LitParser *parser, bool can_assign);
static LitExpression *prule_object(LitParser *parser, bool can_assign);
static LitExpression *prule_variable_expression_base(LitParser *parser, bool can_assign, bool isnew);
static LitExpression *prule_variable_expression(LitParser *parser, bool can_assign);
static LitExpression *prule_new_expression(LitParser *parser, bool can_assign);
static LitExpression *prule_dot(LitParser *parser, LitExpression *previous, bool can_assign);
static LitExpression *prule_range(LitParser *parser, LitExpression *previous, bool can_assign);
static LitExpression *prule_ternary_or_question(LitParser *parser, LitExpression *previous, bool can_assign);
static LitExpression *prule_array(LitParser *parser, bool can_assign);
static LitExpression *prule_subscript(LitParser *parser, LitExpression *previous, bool can_assign);
static LitExpression *prule_this(LitParser *parser, bool can_assign);
static LitExpression *prule_super(LitParser *parser, bool can_assign);
static LitExpression *prule_reference(LitParser *parser, bool can_assign);
static LitExpression *prule_nothing(LitParser *parser, bool canassign);
static LitExpression *prule_function(LitParser *parser, bool canassign);


static void setup_rules()
{
    rules[LITTOK_LEFT_PAREN] = (LitParseRule){ prule_grouping_or_lambda, prule_call, LITPREC_CALL };
    rules[LITTOK_PLUS] = (LitParseRule){ NULL, prule_binary, LITPREC_TERM };
    rules[LITTOK_MINUS] = (LitParseRule){ prule_unary, prule_binary, LITPREC_TERM };
    rules[LITTOK_BANG] = (LitParseRule){ prule_unary, prule_binary, LITPREC_TERM };
    rules[LITTOK_STAR] = (LitParseRule){ NULL, prule_binary, LITPREC_FACTOR };
    rules[LITTOK_STAR_STAR] = (LitParseRule){ NULL, prule_binary, LITPREC_FACTOR };
    rules[LITTOK_SLASH] = (LitParseRule){ NULL, prule_binary, LITPREC_FACTOR };
    rules[LITTOK_SHARP] = (LitParseRule){ NULL, prule_binary, LITPREC_FACTOR };
    rules[LITTOK_STAR] = (LitParseRule){ NULL, prule_binary, LITPREC_FACTOR };
    rules[LITTOK_STAR] = (LitParseRule){ NULL, prule_binary, LITPREC_FACTOR };
    rules[LITTOK_BAR] = (LitParseRule){ NULL, prule_binary, LITPREC_BOR };
    rules[LITTOK_AMPERSAND] = (LitParseRule){ NULL, prule_binary, LITPREC_BAND };
    rules[LITTOK_TILDE] = (LitParseRule){ prule_unary, NULL, LITPREC_UNARY };
    rules[LITTOK_CARET] = (LitParseRule){ NULL, prule_binary, LITPREC_BOR };
    rules[LITTOK_LESS_LESS] = (LitParseRule){ NULL, prule_binary, LITPREC_SHIFT };
    rules[LITTOK_GREATER_GREATER] = (LitParseRule){ NULL, prule_binary, LITPREC_SHIFT };
    rules[LITTOK_PERCENT] = (LitParseRule){ NULL, prule_binary, LITPREC_FACTOR };
    rules[LITTOK_IS] = (LitParseRule){ NULL, prule_binary, LITPREC_IS };
    rules[LITTOK_NUMBER] = (LitParseRule){ prule_number, NULL, LITPREC_NONE };
    rules[LITTOK_TRUE] = (LitParseRule){ prule_literal, NULL, LITPREC_NONE };
    rules[LITTOK_FALSE] = (LitParseRule){ prule_literal, NULL, LITPREC_NONE };
    rules[LITTOK_NULL] = (LitParseRule){ prule_literal, NULL, LITPREC_NONE };
    rules[LITTOK_BANG_EQUAL] = (LitParseRule){ NULL, prule_binary, LITPREC_EQUALITY };
    rules[LITTOK_EQUAL_EQUAL] = (LitParseRule){ NULL, prule_binary, LITPREC_EQUALITY };
    rules[LITTOK_GREATER] = (LitParseRule){ NULL, prule_binary, LITPREC_COMPARISON };
    rules[LITTOK_GREATER_EQUAL] = (LitParseRule){ NULL, prule_binary, LITPREC_COMPARISON };
    rules[LITTOK_LESS] = (LitParseRule){ NULL, prule_binary, LITPREC_COMPARISON };
    rules[LITTOK_LESS_EQUAL] = (LitParseRule){ NULL, prule_binary, LITPREC_COMPARISON };
    rules[LITTOK_STRING] = (LitParseRule){ prule_string, NULL, LITPREC_NONE };
    rules[LITTOK_INTERPOLATION] = (LitParseRule){ prule_interpolation, NULL, LITPREC_NONE };
    rules[LITTOK_IDENTIFIER] = (LitParseRule){ prule_variable_expression, NULL, LITPREC_NONE };
    rules[LITTOK_NEW] = (LitParseRule){ prule_new_expression, NULL, LITPREC_NONE };
    rules[LITTOK_PLUS_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_MINUS_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_STAR_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_SLASH_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_SHARP_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_PERCENT_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_CARET_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_BAR_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_AMPERSAND_EQUAL] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_PLUS_PLUS] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_MINUS_MINUS] = (LitParseRule){ NULL, prule_compound, LITPREC_COMPOUND };
    rules[LITTOK_AMPERSAND_AMPERSAND] = (LitParseRule){ NULL, prule_and, LITPREC_AND };
    rules[LITTOK_BAR_BAR] = (LitParseRule){ NULL, prule_or, LITPREC_AND };
    rules[LITTOK_QUESTION_QUESTION] = (LitParseRule){ NULL, prule_null_filter, LITPREC_NULL };
    rules[LITTOK_DOT] = (LitParseRule){ NULL, prule_dot, LITPREC_CALL };
    rules[LITTOK_SMALL_ARROW] = (LitParseRule){ NULL, prule_dot, LITPREC_CALL };
    rules[LITTOK_DOT_DOT] = (LitParseRule){ NULL, prule_range, LITPREC_RANGE };
    rules[LITTOK_DOT_DOT_DOT] = (LitParseRule){ prule_variable_expression, NULL, LITPREC_ASSIGNMENT };
    rules[LITTOK_LEFT_BRACKET] = (LitParseRule){ prule_array, prule_subscript, LITPREC_NONE };
    rules[LITTOK_LEFT_BRACE] = (LitParseRule){ prule_object, NULL, LITPREC_NONE };
    rules[LITTOK_THIS] = (LitParseRule){ prule_this, NULL, LITPREC_NONE };
    rules[LITTOK_SUPER] = (LitParseRule){ prule_super, NULL, LITPREC_NONE };
    rules[LITTOK_QUESTION] = (LitParseRule){ NULL, prule_ternary_or_question, LITPREC_EQUALITY };
    rules[LITTOK_REF] = (LitParseRule){ prule_reference, NULL, LITPREC_NONE };
    rules[LITTOK_FUNCTION] = (LitParseRule){prule_function, NULL, LITPREC_NONE};
    rules[LITTOK_SEMICOLON] = (LitParseRule){prule_nothing, NULL, LITPREC_NONE};
}


const char* token_name(int t)
{
    switch(t)
    {
        case LITTOK_NEW_LINE: return "LITTOK_NEW_LINE";
        case LITTOK_LEFT_PAREN: return "LITTOK_LEFT_PAREN";
        case LITTOK_RIGHT_PAREN: return "LITTOK_RIGHT_PAREN";
        case LITTOK_LEFT_BRACE: return "LITTOK_LEFT_BRACE";
        case LITTOK_RIGHT_BRACE: return "LITTOK_RIGHT_BRACE";
        case LITTOK_LEFT_BRACKET: return "LITTOK_LEFT_BRACKET";
        case LITTOK_RIGHT_BRACKET: return "LITTOK_RIGHT_BRACKET";
        case LITTOK_COMMA: return "LITTOK_COMMA";
        case LITTOK_SEMICOLON: return "LITTOK_SEMICOLON";
        case LITTOK_COLON: return "LITTOK_COLON";
        case LITTOK_BAR_EQUAL: return "LITTOK_BAR_EQUAL";
        case LITTOK_BAR: return "LITTOK_BAR";
        case LITTOK_BAR_BAR: return "LITTOK_BAR_BAR";
        case LITTOK_AMPERSAND_EQUAL: return "LITTOK_AMPERSAND_EQUAL";
        case LITTOK_AMPERSAND: return "LITTOK_AMPERSAND";
        case LITTOK_AMPERSAND_AMPERSAND: return "LITTOK_AMPERSAND_AMPERSAND";
        case LITTOK_BANG: return "LITTOK_BANG";
        case LITTOK_BANG_EQUAL: return "LITTOK_BANG_EQUAL";
        case LITTOK_EQUAL: return "LITTOK_EQUAL";
        case LITTOK_EQUAL_EQUAL: return "LITTOK_EQUAL_EQUAL";
        case LITTOK_GREATER: return "LITTOK_GREATER";
        case LITTOK_GREATER_EQUAL: return "LITTOK_GREATER_EQUAL";
        case LITTOK_GREATER_GREATER: return "LITTOK_GREATER_GREATER";
        case LITTOK_LESS: return "LITTOK_LESS";
        case LITTOK_LESS_EQUAL: return "LITTOK_LESS_EQUAL";
        case LITTOK_LESS_LESS: return "LITTOK_LESS_LESS";
        case LITTOK_PLUS: return "LITTOK_PLUS";
        case LITTOK_PLUS_EQUAL: return "LITTOK_PLUS_EQUAL";
        case LITTOK_PLUS_PLUS: return "LITTOK_PLUS_PLUS";
        case LITTOK_MINUS: return "LITTOK_MINUS";
        case LITTOK_MINUS_EQUAL: return "LITTOK_MINUS_EQUAL";
        case LITTOK_MINUS_MINUS: return "LITTOK_MINUS_MINUS";
        case LITTOK_STAR: return "LITTOK_STAR";
        case LITTOK_STAR_EQUAL: return "LITTOK_STAR_EQUAL";
        case LITTOK_STAR_STAR: return "LITTOK_STAR_STAR";
        case LITTOK_SLASH: return "LITTOK_SLASH";
        case LITTOK_SLASH_EQUAL: return "LITTOK_SLASH_EQUAL";
        case LITTOK_QUESTION: return "LITTOK_QUESTION";
        case LITTOK_QUESTION_QUESTION: return "LITTOK_QUESTION_QUESTION";
        case LITTOK_PERCENT: return "LITTOK_PERCENT";
        case LITTOK_PERCENT_EQUAL: return "LITTOK_PERCENT_EQUAL";
        case LITTOK_ARROW: return "LITTOK_ARROW";
        case LITTOK_SMALL_ARROW: return "LITTOK_SMALL_ARROW";
        case LITTOK_TILDE: return "LITTOK_TILDE";
        case LITTOK_CARET: return "LITTOK_CARET";
        case LITTOK_CARET_EQUAL: return "LITTOK_CARET_EQUAL";
        case LITTOK_DOT: return "LITTOK_DOT";
        case LITTOK_DOT_DOT: return "LITTOK_DOT_DOT";
        case LITTOK_DOT_DOT_DOT: return "LITTOK_DOT_DOT_DOT";
        case LITTOK_SHARP: return "LITTOK_SHARP";
        case LITTOK_SHARP_EQUAL: return "LITTOK_SHARP_EQUAL";
        case LITTOK_IDENTIFIER: return "LITTOK_IDENTIFIER";
        case LITTOK_STRING: return "LITTOK_STRING";
        case LITTOK_INTERPOLATION: return "LITTOK_INTERPOLATION";
        case LITTOK_NUMBER: return "LITTOK_NUMBER";
        case LITTOK_CLASS: return "LITTOK_CLASS";
        case LITTOK_ELSE: return "LITTOK_ELSE";
        case LITTOK_FALSE: return "LITTOK_FALSE";
        case LITTOK_FOR: return "LITTOK_FOR";
        case LITTOK_FUNCTION: return "LITTOK_FUNCTION";
        case LITTOK_IF: return "LITTOK_IF";
        case LITTOK_NULL: return "LITTOK_NULL";
        case LITTOK_RETURN: return "LITTOK_RETURN";
        case LITTOK_SUPER: return "LITTOK_SUPER";
        case LITTOK_THIS: return "LITTOK_THIS";
        case LITTOK_TRUE: return "LITTOK_TRUE";
        case LITTOK_VAR: return "LITTOK_VAR";
        case LITTOK_WHILE: return "LITTOK_WHILE";
        case LITTOK_CONTINUE: return "LITTOK_CONTINUE";
        case LITTOK_BREAK: return "LITTOK_BREAK";
        case LITTOK_NEW: return "LITTOK_NEW";
        case LITTOK_EXPORT: return "LITTOK_EXPORT";
        case LITTOK_IS: return "LITTOK_IS";
        case LITTOK_STATIC: return "LITTOK_STATIC";
        case LITTOK_OPERATOR: return "LITTOK_OPERATOR";
        case LITTOK_GET: return "LITTOK_GET";
        case LITTOK_SET: return "LITTOK_SET";
        case LITTOK_IN: return "LITTOK_IN";
        case LITTOK_CONST: return "LITTOK_CONST";
        case LITTOK_REF: return "LITTOK_REF";
        case LITTOK_ERROR: return "LITTOK_ERROR";
        case LITTOK_EOF: return "LITTOK_EOF";
        default:
            break;
    }
    return "?unknown?";
}


static void prs_init_compiler(LitParser* parser, LitCompiler* compiler)
{
    compiler->scope_depth = 0;
    compiler->function = NULL;
    compiler->enclosing = (struct LitCompiler*)parser->compiler;

    parser->compiler = compiler;
}

static void prs_end_compiler(LitParser* parser, LitCompiler* compiler)
{
    parser->compiler = (LitCompiler*)compiler->enclosing;
}

static void prs_begin_scope(LitParser* parser)
{
    parser->compiler->scope_depth++;
}

static void prs_end_scope(LitParser* parser)
{
    parser->compiler->scope_depth--;
}

static LitParseRule* get_rule(LitTokType type)
{
    return &rules[type];
}

static inline bool prs_is_at_end(LitParser* parser)
{
    return parser->current.type == LITTOK_EOF;
}

void lit_init_parser(LitState* state, LitParser* parser)
{
    if(!did_setup_rules)
    {
        did_setup_rules = true;
        setup_rules();
    }
    parser->state = state;
    parser->had_error = false;
    parser->panic_mode = false;
}

void lit_free_parser(LitParser* parser)
{
    (void)parser;
}

static void string_error(LitParser* parser, LitToken* token, const char* message)
{
    (void)token;
    if(parser->panic_mode)
    {
        return;
    }
    lit_state_raiseerror(parser->state, COMPILE_ERROR, message);
    parser->had_error = true;
    sync(parser);
}

static void error_at(LitParser* parser, LitToken* token, LitError lit_emitter_raiseerror, va_list args)
{
    string_error(parser, token, lit_vformat_error(parser->state, token->line, lit_emitter_raiseerror, args)->chars);
}

static void error_at_current(LitParser* parser, LitError lit_emitter_raiseerror, ...)
{
    va_list args;
    va_start(args, lit_emitter_raiseerror);
    error_at(parser, &parser->current, lit_emitter_raiseerror, args);
    va_end(args);
}

static void prs_error(LitParser* parser, LitError lit_emitter_raiseerror, ...)
{
    va_list args;
    va_start(args, lit_emitter_raiseerror);
    error_at(parser, &parser->previous, lit_emitter_raiseerror, args);
    va_end(args);
}

static void prs_advance(LitParser* parser)
{
    parser->previous = parser->current;

    while(true)
    {
        parser->current = lit_scan_token(parser->state->scanner);
        if(parser->current.type != LITTOK_ERROR)
        {
            break;
        }
        string_error(parser, &parser->current, parser->current.start);
    }
}


static void prs_rollback(LitParser* parser)
{
    parser->current = parser->previous;
    lit_scan_rollback(parser->state->scanner);
}


static bool check(LitParser* parser, LitTokType type)
{
    return parser->current.type == type;
}

static bool prs_match(LitParser* parser, LitTokType type)
{
    if(parser->current.type == type)
    {
        prs_advance(parser);
        return true;
    }
    return false;
}

static bool match_new_line(LitParser* parser)
{
    while(true)
    {
        if(!prs_match(parser, LITTOK_NEW_LINE))
        {
            return false;
        }
    }
    return true;
}

/*
struct LitParser
{
    LitState* state;
    bool had_error;
    bool panic_mode;
    LitToken previous;
    LitToken current;
    LitCompiler* compiler;
    uint8_t expression_root_count;
    uint8_t statement_root_count;
};

*/
static void ignore_new_lines(LitParser* parser, bool checksemi)
{
    /*
    * verbosely do the wrong thing.
    *
    * this would have checked if $next is a semicolon *followed by* a new line;
    * if so, shift the tokens forward.
    * if not, and since we need to read the next token(s) to figure out whether or not
    * the next may be ignored...
    * actually, just ignore it. i'm keeping it as a bit of memorabilia.
    */
    #if 0
        LitToken nowtok;
        LitToken currtok;
        LitToken prevtok;
        LitToken nextcurrtok;
        LitToken nextprevtok;
        nowtok = parser->current;
        if(checksemi)
        {
            currtok = parser->current;
            prevtok = parser->previous;
            if(currtok.type == LITTOK_SEMICOLON)
            {
                //fprintf(stderr, "before mangling: parser->previous.type=%s, parser->current.type=%s\n", token_name(parser->previous.type), token_name(parser->current.type));
                nextcurrtok = parser->current;
                nextprevtok = parser->previous;
                prs_advance(parser);

                if((parser->current.type == LITTOK_NEW_LINE) || (parser->current.type == LITTOK_EOF))
                {
                    return ignore_new_lines(parser, checksemi);
                }
                else
                {
                    prs_rollback(parser);
                    #if 0
                        parser->current = prevtok;
                        parser->previous = currtok;
                    #else
                        //prs_advance(parser);
                        #if 1
                            //parser->current = nowtok;
                            parser->current = nextcurrtok;
                            parser->previous = nextprevtok;
                        #else
                            parser->current = currtok;
                        #endif
                    #endif
                }

                //fprintf(stderr, "after mangling: parser->previous.type=%s, parser->current.type=%s\n", token_name(parser->previous.type), token_name(parser->current.type));
            }
        }
    #endif
    match_new_line(parser);
}

static void consume(LitParser* parser, LitTokType type, const char* lit_emitter_raiseerror)
{
    bool line;
    size_t olen;
    const char* fmt;
    const char* otext;
    if(parser->current.type == type)
    {
        prs_advance(parser);
        return;
    }
    //fprintf(stderr, "in consume: failed?\n");
    line = parser->previous.type == LITTOK_NEW_LINE;
    olen = (line ? 8 : parser->previous.length);
    otext = (line ? "new line" : parser->previous.start);
    fmt = lit_format_error(parser->state, parser->current.line, LITERROR_EXPECTATION_UNMET, lit_emitter_raiseerror, olen, otext)->chars;
    string_error(parser, &parser->current,fmt);
}

static LitExpression* parse_block(LitParser* parser)
{
    LitBlockExpr* statement;
    prs_begin_scope(parser);
    statement = lit_ast_make_blockexpr(parser->state, parser->previous.line);
    while(true)
    {
        ignore_new_lines(parser, true);
        if(check(parser, LITTOK_RIGHT_BRACE) || check(parser, LITTOK_EOF))
        {
            break;
        }
        ignore_new_lines(parser, true);
        lit_exprlist_push(parser->state, &statement->statements, parse_statement(parser));
        ignore_new_lines(parser, true);
    }
    ignore_new_lines(parser, true);
    consume(parser, LITTOK_RIGHT_BRACE, "'}'");
    ignore_new_lines(parser, true);
    prs_end_scope(parser);
    return (LitExpression*)statement;
}

static LitExpression* parse_precedence(LitParser* parser, LitPrecedence precedence, bool err, bool ignsemi)
{
    bool new_line;
    bool prev_newline;
    bool parser_prev_newline;
    bool can_assign;
    LitExpression* expr;
    LitPrefixParseFn prefix_rule;
    LitInfixParseFn infix_rule;
    LitToken previous;
    (void)new_line;
    prev_newline = false;
    previous = parser->previous;
    prs_advance(parser);
    prefix_rule = get_rule(parser->previous.type)->prefix;
    if(prefix_rule == NULL)
    {
        //if(parser->previous.type != parser->current.type)
        {
            // todo: file start
            new_line = previous.start != NULL && *previous.start == '\n';
            parser_prev_newline = parser->previous.start != NULL && *parser->previous.start == '\n';
            prs_error(parser, LITERROR_EXPECTED_EXPRESSION,
                (prev_newline ? 8 : previous.length),
                (prev_newline ? "new line" : previous.start),
                (parser_prev_newline ? 8 : parser->previous.length),
                (parser_prev_newline ? "new line" : parser->previous.start)
            );
            return NULL;
        }
    }
    can_assign = precedence <= LITPREC_ASSIGNMENT;
    expr = prefix_rule(parser, can_assign);
    ignore_new_lines(parser, ignsemi);
    while(precedence <= get_rule(parser->current.type)->precedence)
    {
        prs_advance(parser);
        infix_rule = get_rule(parser->previous.type)->infix;
        expr = infix_rule(parser, expr, can_assign);
    }
    if(err && can_assign && prs_match(parser, LITTOK_EQUAL))
    {
        prs_error(parser, LITERROR_INVALID_ASSIGMENT_TARGET);
    }
    return expr;
}

static LitExpression* prule_number(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    return (LitExpression*)lit_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value);
}

static LitExpression* parse_lambda(LitParser* parser, LitLambdaExpr* lambda)
{
    lambda->body = parse_statement(parser);
    return (LitExpression*)lambda;
}

static void parse_parameters(LitParser* parser, LitParamList* parameters)
{
    bool had_default;
    size_t arg_length;
    const char* arg_name;
    LitExpression* default_value;
    had_default = false;
    while(!check(parser, LITTOK_RIGHT_PAREN))
    {
        // Vararg ...
        if(prs_match(parser, LITTOK_DOT_DOT_DOT))
        {
            lit_paramlist_push(parser->state, parameters, (LitParameter){ "...", 3, NULL });
            return;
        }
        consume(parser, LITTOK_IDENTIFIER, "argument name");
        arg_name = parser->previous.start;
        arg_length = parser->previous.length;
        default_value = NULL;
        if(prs_match(parser, LITTOK_EQUAL))
        {
            had_default = true;
            default_value = parse_expression(parser, true);
        }
        else if(had_default)
        {
            prs_error(parser, LITERROR_DEFAULT_ARG_CENTRED);
        }
        lit_paramlist_push(parser->state, parameters, (LitParameter){ arg_name, arg_length, default_value });
        if(!prs_match(parser, LITTOK_COMMA))
        {
            break;
        }
    }
}

/*
* this is extremely not working at all.
*/
static LitExpression* prule_grouping_or_lambda(LitParser* parser, bool can_assign)
{
    bool stop;
    bool had_default;
    bool had_vararg;
    bool had_array;
    bool had_arrow;
    size_t line;
    size_t first_arg_length;
    size_t arg_length;
    const char* start;
    const char* arg_name;
    const char* first_arg_start;
    LitExpression* expression;
    LitExpression* default_value;
    LitScanner* scanner;
    (void)can_assign;
    (void)had_arrow;
    (void)had_array;
    had_arrow = false;
    if(prs_match(parser, LITTOK_RIGHT_PAREN))
    {
        consume(parser, LITTOK_ARROW, "=> after lambda arguments");
        return parse_lambda(parser, lit_ast_make_lambdaexpr(parser->state, parser->previous.line));
    }
    start = parser->previous.start;
    line = parser->previous.line;
    if(prs_match(parser, LITTOK_IDENTIFIER) || prs_match(parser, LITTOK_DOT_DOT_DOT))
    {
        LitState* state = parser->state;
        first_arg_start = parser->previous.start;
        first_arg_length = parser->previous.length;
        if(prs_match(parser, LITTOK_COMMA) || (prs_match(parser, LITTOK_RIGHT_PAREN) && prs_match(parser, LITTOK_ARROW)))
        {
            had_array = parser->previous.type == LITTOK_ARROW;
            had_vararg= parser->previous.type == LITTOK_DOT_DOT_DOT;
            // This is a lambda
            LitLambdaExpr* lambda = lit_ast_make_lambdaexpr(state, line);
            LitExpression* def_value = NULL;
            had_default = prs_match(parser, LITTOK_EQUAL);
            if(had_default)
            {
                def_value = parse_expression(parser, true);
            }
            lit_paramlist_push(state, &lambda->parameters, (LitParameter){ first_arg_start, first_arg_length, def_value });
            if(!had_vararg && parser->previous.type == LITTOK_COMMA)
            {
                do
                {
                    stop = false;
                    if(prs_match(parser, LITTOK_DOT_DOT_DOT))
                    {
                        stop = true;
                    }
                    else
                    {
                        consume(parser, LITTOK_IDENTIFIER, "argument name");
                    }

                    arg_name = parser->previous.start;
                    arg_length = parser->previous.length;
                    default_value = NULL;
                    if(prs_match(parser, LITTOK_EQUAL))
                    {
                        default_value = parse_expression(parser, true);
                        had_default = true;
                    }
                    else if(had_default)
                    {
                        prs_error(parser, LITERROR_DEFAULT_ARG_CENTRED);
                    }
                    lit_paramlist_push(state, &lambda->parameters, (LitParameter){ arg_name, arg_length, default_value });
                    if(stop)
                    {
                        break;
                    }
                } while(prs_match(parser, LITTOK_COMMA));
            }
            #if 0
            if(!had_arrow)
            {
                consume(parser, LITTOK_RIGHT_PAREN, "')' after lambda parameters");
                consume(parser, LITTOK_ARROW, "=> after lambda parameters");
            }
            #endif
            return parse_lambda(parser, lambda);
        }
        else
        {
            // Ouch, this was a grouping with a single identifier
            scanner = state->scanner;
            scanner->current = start;
            scanner->line = line;
            parser->current = lit_scan_token(scanner);
            prs_advance(parser);
        }
    }
    expression = parse_expression(parser, true);
    consume(parser, LITTOK_RIGHT_PAREN, "')' after grouping expression");
    return expression;
}

static LitExpression* prule_call(LitParser* parser, LitExpression* prev, bool can_assign)
{
    (void)can_assign;
    LitExpression* e;
    LitVarExpr* ee;
    LitCallExpr* expression;
    expression = lit_ast_make_callexpr(parser->state, parser->previous.line, prev);
    while(!check(parser, LITTOK_RIGHT_PAREN))
    {
        e = parse_expression(parser, true);
        lit_exprlist_push(parser->state, &expression->args, e);
        if(!prs_match(parser, LITTOK_COMMA))
        {
            break;
        }
        if(e->type == LITEXPR_VAREXPR)
        {
            ee = (LitVarExpr*)e;
            // Vararg ...
            if(ee->length == 3 && memcmp(ee->name, "...", 3) == 0)
            {
                break;
            }
        }
    }
    if(expression->args.count > 255)
    {
        prs_error(parser, LITERROR_TOO_MANY_FUNCTION_ARGS, (int)expression->args.count);
    }
    consume(parser, LITTOK_RIGHT_PAREN, "')' after arguments");
    return (LitExpression*)expression;
}

static LitExpression* prule_unary(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    size_t line;
    LitExpression* expression;
    LitTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    expression = parse_precedence(parser, LITPREC_UNARY, true, true);
    return (LitExpression*)lit_ast_make_unaryexpr(parser->state, line, expression, op);
}

static LitExpression* prule_binary(LitParser* parser, LitExpression* prev, bool can_assign)
{
    (void)can_assign;
    bool invert;
    size_t line;
    LitParseRule* rule;
    LitExpression* expression;
    LitTokType op;
    invert = parser->previous.type == LITTOK_BANG;
    if(invert)
    {
        consume(parser, LITTOK_IS, "'is' after '!'");
    }
    op = parser->previous.type;
    line = parser->previous.line;
    rule = get_rule(op);
    expression = parse_precedence(parser, (LitPrecedence)(rule->precedence + 1), true, true);
    expression = (LitExpression*)lit_ast_make_binaryexpr(parser->state, line, prev, expression, op);
    if(invert)
    {
        expression = (LitExpression*)lit_ast_make_unaryexpr(parser->state, line, expression, LITTOK_BANG);
    }
    return expression;
}

static LitExpression* prule_and(LitParser* parser, LitExpression* prev, bool can_assign)
{
    (void)can_assign;
    size_t line;
    LitTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (LitExpression*)lit_ast_make_binaryexpr(parser->state, line, prev, parse_precedence(parser, LITPREC_AND, true, true), op);
}

static LitExpression* prule_or(LitParser* parser, LitExpression* prev, bool can_assign)
{
    (void)can_assign;
    size_t line;
    LitTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (LitExpression*)lit_ast_make_binaryexpr(parser->state, line, prev, parse_precedence(parser, LITPREC_OR, true, true), op);
}

static LitExpression* prule_null_filter(LitParser* parser, LitExpression* prev, bool can_assign)
{
    (void)can_assign;
    size_t line;
    LitTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (LitExpression*)lit_ast_make_binaryexpr(parser->state, line, prev, parse_precedence(parser, LITPREC_NULL, true, true), op);
}

static LitTokType convert_compound_operator(LitTokType op)
{
    switch(op)
    {
        case LITTOK_PLUS_EQUAL:
            {
                return LITTOK_PLUS;
            }
            break;
        case LITTOK_MINUS_EQUAL:
            {
                return LITTOK_MINUS;
            }
            break;
        case LITTOK_STAR_EQUAL:
            {
                return LITTOK_STAR;
            }
            break;
        case LITTOK_SLASH_EQUAL:
            {
                return LITTOK_SLASH;
            }
            break;
        case LITTOK_SHARP_EQUAL:
            {
                return LITTOK_SHARP;
            }
            break;
        case LITTOK_PERCENT_EQUAL:
            {
                return LITTOK_PERCENT;
            }
            break;
        case LITTOK_CARET_EQUAL:
            {
                return LITTOK_CARET;
            }
            break;
        case LITTOK_BAR_EQUAL:
            {
                return LITTOK_BAR;
            }
            break;
        case LITTOK_AMPERSAND_EQUAL:
            {
                return LITTOK_AMPERSAND;
            }
            break;
        case LITTOK_PLUS_PLUS:
            {
                return LITTOK_PLUS;
            }
            break;
        case LITTOK_MINUS_MINUS:
            {
                return LITTOK_MINUS;
            }
            break;
        default:
            {
                UNREACHABLE
            }
            break;
    }
    return (LitTokType)-1;
}

static LitExpression* prule_compound(LitParser* parser, LitExpression* prev, bool can_assign)
{
    (void)can_assign;
    size_t line;
    LitBinaryExpr* binary;
    LitExpression* expression;
    LitParseRule* rule;
    LitTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    rule = get_rule(op);
    if(op == LITTOK_PLUS_PLUS || op == LITTOK_MINUS_MINUS)
    {
        expression = (LitExpression*)lit_ast_make_literalexpr(parser->state, line, lit_value_numbertovalue(parser->state, 1));
    }
    else
    {
        expression = parse_precedence(parser, (LitPrecedence)(rule->precedence + 1), true, true);
    }
    binary = lit_ast_make_binaryexpr(parser->state, line, prev, expression, convert_compound_operator(op));
    binary->ignore_left = true;// To make sure we don't free it twice
    return (LitExpression*)lit_ast_make_assignexpr(parser->state, line, prev, (LitExpression*)binary);
}

static LitExpression* prule_literal(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    size_t line;
    line = parser->previous.line;
    switch(parser->previous.type)
    {
        case LITTOK_TRUE:
            {
                return (LitExpression*)lit_ast_make_literalexpr(parser->state, line, TRUE_VALUE);
            }
            break;
        case LITTOK_FALSE:
            {
                return (LitExpression*)lit_ast_make_literalexpr(parser->state, line, FALSE_VALUE);
            }
            break;
        case LITTOK_NULL:
            {
                return (LitExpression*)lit_ast_make_literalexpr(parser->state, line, NULL_VALUE);
            }
            break;
        default:
            {
                UNREACHABLE
            }
            break;
    }
    return NULL;
}

static LitExpression* prule_string(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    LitExpression* expression;
    expression = (LitExpression*)lit_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value);
    if(prs_match(parser, LITTOK_LEFT_BRACKET))
    {
        return prule_subscript(parser, expression, can_assign);
    }
    return expression;
}

static LitExpression* prule_interpolation(LitParser* parser, bool can_assign)
{
    LitInterpolationExpr* expression;
    (void)can_assign;
    expression = lit_ast_make_strinterpolexpr(parser->state, parser->previous.line);
    do
    {
        if(lit_string_getlength(lit_value_asstring(parser->previous.value)) > 0)
        {
            lit_exprlist_push(
            parser->state, &expression->expressions,
            (LitExpression*)lit_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value));
        }
        lit_exprlist_push(parser->state, &expression->expressions, parse_expression(parser, true));
    } while(prs_match(parser, LITTOK_INTERPOLATION));
    consume(parser, LITTOK_STRING, "end of interpolation");
    if(lit_string_getlength(lit_value_asstring(parser->previous.value)) > 0)
    {
        lit_exprlist_push(
        parser->state, &expression->expressions,
        (LitExpression*)lit_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value));
    }
    if(prs_match(parser, LITTOK_LEFT_BRACKET))
    {
        return prule_subscript(parser, (LitExpression*)expression, can_assign);
    }
    return (LitExpression*)expression;
}

static LitExpression* prule_object(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    LitObjectExpr* object;
    object = lit_ast_make_objectexpr(parser->state, parser->previous.line);
    ignore_new_lines(parser, true);
    while(!check(parser, LITTOK_RIGHT_BRACE))
    {
        ignore_new_lines(parser, true);
        consume(parser, LITTOK_IDENTIFIER, "key string after '{'");
        lit_vallist_push(parser->state, &object->keys, lit_value_objectvalue(lit_string_copy(parser->state, parser->previous.start, parser->previous.length)));
        ignore_new_lines(parser, true);
        consume(parser, LITTOK_EQUAL, "'=' after key string");
        ignore_new_lines(parser, true);
        lit_exprlist_push(parser->state, &object->values, parse_expression(parser, true));
        if(!prs_match(parser, LITTOK_COMMA))
        {
            break;
        }
    }
    ignore_new_lines(parser, true);
    consume(parser, LITTOK_RIGHT_BRACE, "'}' after object");
    return (LitExpression*)object;
}

static LitExpression* prule_variable_expression_base(LitParser* parser, bool can_assign, bool isnew)
{
    (void)can_assign;
    bool had_args;
    LitCallExpr* lit_vm_callcallable;
    LitExpression* expression;
    expression = (LitExpression*)lit_ast_make_varexpr(parser->state, parser->previous.line, parser->previous.start, parser->previous.length);
    if(isnew)
    {
        had_args = check(parser, LITTOK_LEFT_PAREN);
        lit_vm_callcallable = NULL;
        if(had_args)
        {
            prs_advance(parser);
            lit_vm_callcallable = (LitCallExpr*)prule_call(parser, expression, false);
        }
        if(prs_match(parser, LITTOK_LEFT_BRACE))
        {
            if(lit_vm_callcallable == NULL)
            {
                lit_vm_callcallable = lit_ast_make_callexpr(parser->state, expression->line, expression);
            }
            lit_vm_callcallable->init = prule_object(parser, false);
        }
        else if(!had_args)
        {
            error_at_current(parser, LITERROR_EXPECTATION_UNMET, "argument list for instance creation",
                             parser->previous.length, parser->previous.start);
        }
        return (LitExpression*)lit_vm_callcallable;
    }
    if(prs_match(parser, LITTOK_LEFT_BRACKET))
    {
        return prule_subscript(parser, expression, can_assign);
    }
    if(can_assign && prs_match(parser, LITTOK_EQUAL))
    {
        return (LitExpression*)lit_ast_make_assignexpr(parser->state, parser->previous.line, expression,
                                                            parse_expression(parser, true));
    }
    return expression;
}

static LitExpression* prule_variable_expression(LitParser* parser, bool can_assign)
{
    return prule_variable_expression_base(parser, can_assign, false);
}

static LitExpression* prule_new_expression(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    consume(parser, LITTOK_IDENTIFIER, "class name after 'new'");
    return prule_variable_expression_base(parser, false, true);
}

static LitExpression* prule_dot(LitParser* parser, LitExpression* previous, bool can_assign)
{
    (void)can_assign;
    bool ignored;
    size_t line;
    size_t length;
    const char* name;
    LitExpression* expression;
    line = parser->previous.line;
    ignored = parser->previous.type == LITTOK_SMALL_ARROW;
    if(!(prs_match(parser, LITTOK_CLASS) || prs_match(parser, LITTOK_SUPER)))
    {// class and super are allowed field names
        consume(parser, LITTOK_IDENTIFIER, ignored ? "propety name after '->'" : "property name after '.'");
    }
    name = parser->previous.start;
    length = parser->previous.length;
    if(!ignored && can_assign && prs_match(parser, LITTOK_EQUAL))
    {
        return (LitExpression*)lit_ast_make_setexpr(parser->state, line, previous, name, length, parse_expression(parser, true));
    }
    expression = (LitExpression*)lit_ast_make_getexpr(parser->state, line, previous, name, length, false, ignored);
    if(!ignored && prs_match(parser, LITTOK_LEFT_BRACKET))
    {
        return prule_subscript(parser, expression, can_assign);
    }
    return expression;
}

static LitExpression* prule_range(LitParser* parser, LitExpression* previous, bool can_assign)
{
    (void)can_assign;
    size_t line;
    line = parser->previous.line;
    return (LitExpression*)lit_ast_make_rangeexpr(parser->state, line, previous, parse_expression(parser, true));
}

static LitExpression* prule_ternary_or_question(LitParser* parser, LitExpression* previous, bool can_assign)
{
    (void)can_assign;
    bool ignored;
    size_t line;
    LitExpression* if_branch;
    LitExpression* else_branch;
    line = parser->previous.line;
    if(prs_match(parser, LITTOK_DOT) || prs_match(parser, LITTOK_SMALL_ARROW))
    {
        ignored = parser->previous.type == LITTOK_SMALL_ARROW;
        consume(parser, LITTOK_IDENTIFIER, ignored ? "property name after '->'" : "property name after '.'");
        return (LitExpression*)lit_ast_make_getexpr(parser->state, line, previous, parser->previous.start,
                                                         parser->previous.length, true, ignored);
    }
    if_branch = parse_expression(parser, true);
    consume(parser, LITTOK_COLON, "':' after expression");
    else_branch = parse_expression(parser, true);
    return (LitExpression*)lit_ast_make_ternaryexpr(parser->state, line, previous, if_branch, else_branch);
}

static LitExpression* prule_array(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    LitExpression* expr;
    LitArrayExpr* array;
    array = lit_ast_make_arrayexpr(parser->state, parser->previous.line);
    ignore_new_lines(parser, true);
    while(!check(parser, LITTOK_RIGHT_BRACKET))
    {
        expr = NULL;
        ignore_new_lines(parser, true);
        #if 1
            expr = parse_expression(parser, true);
        #else
            if(check(parser, LITTOK_COMMA))
            {
                //prule_null_filter(LitParser *parser, LitExpression *prev, _Bool can_assign)
                expr = prule_null_filter(parser, NULL, false);
            }
            else
            {
                expr = parse_expression(parser, true);
            }
        #endif
        lit_exprlist_push(parser->state, &array->values, expr);
        if(!prs_match(parser, LITTOK_COMMA))
        {
            break;
        }
        ignore_new_lines(parser, true);
    }
    ignore_new_lines(parser, true);

    

    consume(parser, LITTOK_RIGHT_BRACKET, "']' after array");
    if(prs_match(parser, LITTOK_LEFT_BRACKET))
    {
        return prule_subscript(parser, (LitExpression*)array, can_assign);
    }
    return (LitExpression*)array;
}

static LitExpression* prule_subscript(LitParser* parser, LitExpression* previous, bool can_assign)
{
    size_t line;
    LitExpression* index;
    LitExpression* expression;
    line = parser->previous.line;
    index = parse_expression(parser, true);
    consume(parser, LITTOK_RIGHT_BRACKET, "']' after subscript");
    expression = (LitExpression*)lit_ast_make_subscriptexpr(parser->state, line, previous, index);
    if(prs_match(parser, LITTOK_LEFT_BRACKET))
    {
        return prule_subscript(parser, expression, can_assign);
    }
    else if(can_assign && prs_match(parser, LITTOK_EQUAL))
    {
        return (LitExpression*)lit_ast_make_assignexpr(parser->state, parser->previous.line, expression, parse_expression(parser, true));
    }
    return expression;
}

static LitExpression* prule_this(LitParser* parser, bool can_assign)
{
    LitExpression* expression;
    expression = (LitExpression*)lit_ast_make_thisexpr(parser->state, parser->previous.line);
    if(prs_match(parser, LITTOK_LEFT_BRACKET))
    {
        return prule_subscript(parser, expression, can_assign);
    }
    return expression;
}

static LitExpression* prule_super(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    bool ignoring;
    size_t line;
    LitExpression* expression;
    line = parser->previous.line;

    if(!(prs_match(parser, LITTOK_DOT) || prs_match(parser, LITTOK_SMALL_ARROW)))
    {
        expression = (LitExpression*)lit_ast_make_superexpr(
        parser->state, line, lit_string_copy(parser->state, "constructor", 11), false);
        consume(parser, LITTOK_LEFT_PAREN, "'(' after 'super'");
        return prule_call(parser, expression, false);
    }
    ignoring = parser->previous.type == LITTOK_SMALL_ARROW;
    consume(parser, LITTOK_IDENTIFIER, ignoring ? "super method name after '->'" : "super method name after '.'");
    expression = (LitExpression*)lit_ast_make_superexpr(
    parser->state, line, lit_string_copy(parser->state, parser->previous.start, parser->previous.length), ignoring);
    if(prs_match(parser, LITTOK_LEFT_PAREN))
    {
        return prule_call(parser, expression, false);
    }
    return expression;
}

static LitExpression *prule_nothing(LitParser *parser, bool canassign)
{
    (void)canassign;
    return NULL;
}

static LitExpression* prule_reference(LitParser* parser, bool can_assign)
{
    (void)can_assign;
    size_t line;
    LitReferenceExpr* expression;
    line = parser->previous.line;
    ignore_new_lines(parser, true);
    expression = lit_ast_make_referenceexpr(parser->state, line, parse_precedence(parser, LITPREC_CALL, false, true));
    if(prs_match(parser, LITTOK_EQUAL))
    {
        return (LitExpression*)lit_ast_make_assignexpr(parser->state, line, (LitExpression*)expression, parse_expression(parser, true));
    }
    return (LitExpression*)expression;
}



static LitExpression* parse_statement(LitParser* parser)
{
    LitExpression* expression;
    ignore_new_lines(parser, true);
    if(setjmp(prs_jmpbuffer))
    {
        return NULL;
    }
    if(prs_match(parser, LITTOK_VAR) || prs_match(parser, LITTOK_CONST))
    {
        return parse_var_declaration(parser, true);
    }
    else if(prs_match(parser, LITTOK_IF))
    {
        return parse_if(parser);
    }
    else if(prs_match(parser, LITTOK_FOR))
    {
        return parse_for(parser);
    }
    else if(prs_match(parser, LITTOK_WHILE))
    {
        return parse_while(parser);
    }
    else if(prs_match(parser, LITTOK_CONTINUE))
    {
        return (LitExpression*)lit_ast_make_continueexpr(parser->state, parser->previous.line);
    }
    else if(prs_match(parser, LITTOK_BREAK))
    {
        return (LitExpression*)lit_ast_make_breakexpr(parser->state, parser->previous.line);
    }
    else if(prs_match(parser, LITTOK_FUNCTION) || prs_match(parser, LITTOK_EXPORT))
    {
        return prule_function(parser, false);
    }
    else if(prs_match(parser, LITTOK_RETURN))
    {
        return parse_return(parser);
    }
    else if(prs_match(parser, LITTOK_LEFT_BRACE))
    {
        return parse_block(parser);
    }
    expression = parse_expression(parser, true);
    return expression == NULL ? NULL : (LitExpression*)lit_ast_make_exprstmt(parser->state, parser->previous.line, expression);
}

static LitExpression* parse_expression(LitParser* parser, bool ignsemi)
{
    ignore_new_lines(parser, ignsemi);
    return parse_precedence(parser, LITPREC_ASSIGNMENT, true, ignsemi);
}

static LitExpression* parse_var_declaration(LitParser* parser, bool ignsemi)
{
    bool constant;
    size_t line;
    size_t length;
    const char* name;
    LitExpression* init;
    constant = parser->previous.type == LITTOK_CONST;
    line = parser->previous.line;
    consume(parser, LITTOK_IDENTIFIER, "variable name");
    name = parser->previous.start;
    length = parser->previous.length;
    init = NULL;
    if(prs_match(parser, LITTOK_EQUAL))
    {
        init = parse_expression(parser, ignsemi);
    }
    return (LitExpression*)lit_ast_make_assignvarexpr(parser->state, line, name, length, init, constant);
}

static LitExpression* parse_if(LitParser* parser)
{
    size_t line;
    bool invert;
    bool had_paren;
    LitExpression* condition;
    LitExpression* if_branch;
    LitExprList* elseif_conditions;
    LitExprList* elseif_branches;
    LitExpression* else_branch;
    LitExpression* e;
    line = parser->previous.line;
    invert = prs_match(parser, LITTOK_BANG);
    had_paren = prs_match(parser, LITTOK_LEFT_PAREN);
    condition = parse_expression(parser, true);
    if(had_paren)
    {
        consume(parser, LITTOK_RIGHT_PAREN, "')'");
    }
    if(invert)
    {
        condition = (LitExpression*)lit_ast_make_unaryexpr(parser->state, condition->line, condition, LITTOK_BANG);
    }
    ignore_new_lines(parser, true);
    if_branch = parse_statement(parser);
    elseif_conditions = NULL;
    elseif_branches = NULL;
    else_branch = NULL;
    ignore_new_lines(parser, true);
    while(prs_match(parser, LITTOK_ELSE))
    {
        // else if
        ignore_new_lines(parser, true);
        if(prs_match(parser, LITTOK_IF))
        {
            if(elseif_conditions == NULL)
            {
                elseif_conditions = lit_ast_allocexprlist(parser->state);
                elseif_branches = lit_ast_allocate_stmtlist(parser->state);
            }
            invert = prs_match(parser, LITTOK_BANG);
            had_paren = prs_match(parser, LITTOK_LEFT_PAREN);
            ignore_new_lines(parser, true);
            e = parse_expression(parser, true);
            if(had_paren)
            {
                consume(parser, LITTOK_RIGHT_PAREN, "')'");
            }
            ignore_new_lines(parser, true);
            if(invert)
            {
                e = (LitExpression*)lit_ast_make_unaryexpr(parser->state, condition->line, e, LITTOK_BANG);
            }
            lit_exprlist_push(parser->state, elseif_conditions, e);
            lit_exprlist_push(parser->state, elseif_branches, parse_statement(parser));
            continue;
        }
        // else
        if(else_branch != NULL)
        {
            prs_error(parser, LITERROR_MULTIPLE_ELSE_BRANCHES);
        }
        ignore_new_lines(parser, true);
        else_branch = parse_statement(parser);
    }
    return (LitExpression*)lit_ast_make_ifexpr(parser->state, line, condition, if_branch, else_branch, elseif_conditions, elseif_branches);
}

static LitExpression* parse_for(LitParser* parser)
{

    bool c_style;
    bool had_paren;
    size_t line;
    LitExpression* condition;
    LitExpression* increment;
    LitExpression* var;
    LitExpression* init;
    line= parser->previous.line;
    had_paren = prs_match(parser, LITTOK_LEFT_PAREN);
    var = NULL;
    init = NULL;
    if(!check(parser, LITTOK_SEMICOLON))
    {
        if(prs_match(parser, LITTOK_VAR))
        {
            var = parse_var_declaration(parser, false);
        }
        else
        {
            init = parse_expression(parser, false);
        }
    }
    c_style = !prs_match(parser, LITTOK_IN);
    condition= NULL;
    increment = NULL;
    if(c_style)
    {
        consume(parser, LITTOK_SEMICOLON, "';'");
        condition = check(parser, LITTOK_SEMICOLON) ? NULL : parse_expression(parser, false);
        consume(parser, LITTOK_SEMICOLON, "';'");
        increment = check(parser, LITTOK_RIGHT_PAREN) ? NULL : parse_expression(parser, false);
    }
    else
    {
        condition = parse_expression(parser, true);
        if(var == NULL)
        {
            prs_error(parser, LITERROR_VAR_MISSING_IN_FORIN);
        }
    }
    if(had_paren)
    {
        consume(parser, LITTOK_RIGHT_PAREN, "')'");
    }
    ignore_new_lines(parser, true);
    return (LitExpression*)lit_ast_make_forexpr(parser->state, line, init, var, condition, increment,
                                                   parse_statement(parser), c_style);
}

static LitExpression* parse_while(LitParser* parser)
{
    bool had_paren;
    size_t line;
    LitExpression* body;
    line = parser->previous.line;
    had_paren = prs_match(parser, LITTOK_LEFT_PAREN);
    LitExpression* condition = parse_expression(parser, true);
    if(had_paren)
    {
        consume(parser, LITTOK_RIGHT_PAREN, "')'");
    }
    ignore_new_lines(parser, true);
    body = parse_statement(parser);
    return (LitExpression*)lit_ast_make_whileexpr(parser->state, line, condition, body);
}

static LitExpression* prule_function(LitParser* parser, bool canassign)
{
    bool isexport;
    bool islambda;
    size_t line;
    size_t fnamelen;
    const char* fnamestr;
    LitCompiler compiler;
    LitFunctionExpr* function;
    LitLambdaExpr* lambda;
    LitSetExpr* to;
    islambda = canassign;
    isexport = parser->previous.type == LITTOK_EXPORT;
    fnamestr = "<anonymous>";
    fnamelen = strlen(fnamestr);
    if(isexport)
    {
        consume(parser, LITTOK_FUNCTION, "'function' after 'export'");
    }
    line = parser->previous.line;
    if(check(parser, LITTOK_IDENTIFIER))
    {
        consume(parser, LITTOK_IDENTIFIER, "function name");
        fnamestr = parser->previous.start;
        fnamelen = parser->previous.length;
    }
    if(prs_match(parser, LITTOK_DOT) || islambda)
    //if(prs_match(parser, LITTOK_DOT))
    {
        to = NULL;
        if(check(parser, LITTOK_IDENTIFIER))
        {
            consume(parser, LITTOK_IDENTIFIER, "function name");
        }
        lambda = lit_ast_make_lambdaexpr(parser->state, line);
        //if(islambda)
        {
            to = lit_ast_make_setexpr(
                parser->state,
                line,
                (LitExpression*)lit_ast_make_varexpr(parser->state, line, fnamestr, fnamelen),
                parser->previous.start,
                parser->previous.length,
                (LitExpression*)lambda
            );
        }
        consume(parser, LITTOK_LEFT_PAREN, "'(' after function name");
        prs_init_compiler(parser, &compiler);
        prs_begin_scope(parser);
        parse_parameters(parser, &lambda->parameters);
        if(lambda->parameters.count > 255)
        {
            prs_error(parser, LITERROR_TOO_MANY_FUNCTION_ARGS, (int)lambda->parameters.count);
        }
        consume(parser, LITTOK_RIGHT_PAREN, "')' after function arguments");
        ignore_new_lines(parser, true);
        lambda->body = parse_statement(parser);
        prs_end_scope(parser);
        prs_end_compiler(parser, &compiler);
        if(islambda)
        {
            return lambda;
        }
        return (LitExpression*)lit_ast_make_exprstmt(parser->state, line, (LitExpression*)to);
    }
    function = lit_ast_make_funcexpr(parser->state, line, fnamestr, fnamelen);
    function->exported = isexport;
    consume(parser, LITTOK_LEFT_PAREN, "'(' after function name");
    prs_init_compiler(parser, &compiler);
    prs_begin_scope(parser);
    parse_parameters(parser, &function->parameters);
    if(function->parameters.count > 255)
    {
        prs_error(parser, LITERROR_TOO_MANY_FUNCTION_ARGS, (int)function->parameters.count);
    }
    consume(parser, LITTOK_RIGHT_PAREN, "')' after function arguments");
    function->body = parse_statement(parser);
    prs_end_scope(parser);
    prs_end_compiler(parser, &compiler);
    return (LitExpression*)function;
}

static LitExpression* parse_return(LitParser* parser)
{
    size_t line;
    LitExpression* expression;
    line = parser->previous.line;
    expression = NULL;
    if(!check(parser, LITTOK_NEW_LINE) && !check(parser, LITTOK_RIGHT_BRACE))
    {
        expression = parse_expression(parser, true);
    }
    return (LitExpression*)lit_ast_make_returnexpr(parser->state, line, expression);
}

static LitExpression* parse_field(LitParser* parser, LitString* name, bool is_static)
{
    size_t line;
    LitExpression* getter;
    LitExpression* setter;
    line = parser->previous.line;
    getter = NULL;
    setter = NULL;
    if(prs_match(parser, LITTOK_ARROW))
    {
        getter = parse_statement(parser);
    }
    else
    {
        prs_match(parser, LITTOK_LEFT_BRACE);// Will be LITTOK_LEFT_BRACE, otherwise this method won't be called
        ignore_new_lines(parser, true);
        if(prs_match(parser, LITTOK_GET))
        {
            prs_match(parser, LITTOK_ARROW);// Ignore it if it's present
            getter = parse_statement(parser);
        }
        ignore_new_lines(parser, true);
        if(prs_match(parser, LITTOK_SET))
        {
            prs_match(parser, LITTOK_ARROW);// Ignore it if it's present
            setter = parse_statement(parser);
        }
        if(getter == NULL && setter == NULL)
        {
            prs_error(parser, LITERROR_NO_GETTER_AND_SETTER);
        }
        ignore_new_lines(parser, true);
        consume(parser, LITTOK_RIGHT_BRACE, "'}' after field declaration");
    }
    return (LitExpression*)lit_ast_make_fieldexpr(parser->state, line, name, getter, setter, is_static);
}

static LitExpression* parse_method(LitParser* parser, bool is_static)
{
    size_t i;
    LitCompiler compiler;
    LitMethodExpr* method;
    LitString* name;
    if(prs_match(parser, LITTOK_STATIC))
    {
        is_static = true;
    }
    name = NULL;
    if(prs_match(parser, LITTOK_OPERATOR))
    {
        if(is_static)
        {
            prs_error(parser, LITERROR_STATIC_OPERATOR);
        }
        i = 0;
        while(operators[i] != LITTOK_EOF)
        {
            if(prs_match(parser, operators[i]))
            {
                break;
            }
            i++;
        }
        if(parser->previous.type == LITTOK_LEFT_BRACKET)
        {
            consume(parser, LITTOK_RIGHT_BRACKET, "']' after '[' in op method declaration");
            name = lit_string_copy(parser->state, "[]", 2);
        }
        else
        {
            name = lit_string_copy(parser->state, parser->previous.start, parser->previous.length);
        }
    }
    else
    {
        consume(parser, LITTOK_IDENTIFIER, "method name");
        name = lit_string_copy(parser->state, parser->previous.start, parser->previous.length);
        if(check(parser, LITTOK_LEFT_BRACE) || check(parser, LITTOK_ARROW))
        {
            return parse_field(parser, name, is_static);
        }
    }
    method = lit_ast_make_methodexpr(parser->state, parser->previous.line, name, is_static);
    prs_init_compiler(parser, &compiler);
    prs_begin_scope(parser);
    consume(parser, LITTOK_LEFT_PAREN, "'(' after method name");
    parse_parameters(parser, &method->parameters);
    if(method->parameters.count > 255)
    {
        prs_error(parser, LITERROR_TOO_MANY_FUNCTION_ARGS, (int)method->parameters.count);
    }
    consume(parser, LITTOK_RIGHT_PAREN, "')' after method arguments");
    method->body = parse_statement(parser);
    prs_end_scope(parser);
    prs_end_compiler(parser, &compiler);
    return (LitExpression*)method;
}

static LitExpression* parse_class(LitParser* parser)
{
    bool finished_parsing_fields;
    bool field_is_static;
    size_t line;
    bool is_static;
    LitString* name;
    LitString* super;
    LitClassExpr* klass;
    LitExpression* var;
    LitExpression* method;
    if(setjmp(prs_jmpbuffer))
    {
        return NULL;
    }
    line = parser->previous.line;
    is_static = parser->previous.type == LITTOK_STATIC;
    if(is_static)
    {
        consume(parser, LITTOK_CLASS, "'class' after 'static'");
    }
    consume(parser, LITTOK_IDENTIFIER, "class name after 'class'");
    name = lit_string_copy(parser->state, parser->previous.start, parser->previous.length);
    super = NULL;
    if(prs_match(parser, LITTOK_COLON))
    {
        consume(parser, LITTOK_IDENTIFIER, "super class name after ':'");
        super = lit_string_copy(parser->state, parser->previous.start, parser->previous.length);
        if(super == name)
        {
            prs_error(parser, LITERROR_SELF_INHERITED_CLASS);
        }
    }
    klass = lit_ast_make_classexpr(parser->state, line, name, super);
    ignore_new_lines(parser, true);
    consume(parser, LITTOK_LEFT_BRACE, "'{' before class body");
    ignore_new_lines(parser, true);
    finished_parsing_fields = false;
    while(!check(parser, LITTOK_RIGHT_BRACE))
    {
        field_is_static = false;
        if(prs_match(parser, LITTOK_STATIC))
        {
            field_is_static = true;
            if(prs_match(parser, LITTOK_VAR))
            {
                if(finished_parsing_fields)
                {
                    prs_error(parser, LITERROR_STATIC_FIELDS_AFTER_METHODS);
                }
                var = parse_var_declaration(parser, true);
                if(var != NULL)
                {
                    lit_exprlist_push(parser->state, &klass->fields, var);
                }
                ignore_new_lines(parser, true);
                continue;
            }
            else
            {
                finished_parsing_fields = true;
            }
        }
        method = parse_method(parser, is_static || field_is_static);
        if(method != NULL)
        {
            lit_exprlist_push(parser->state, &klass->fields, method);
        }
        ignore_new_lines(parser, true);
    }
    consume(parser, LITTOK_RIGHT_BRACE, "'}' after class body");
    return (LitExpression*)klass;
}

static void sync(LitParser* parser)
{
    parser->panic_mode = false;
    while(parser->current.type != LITTOK_EOF)
    {
        if(parser->previous.type == LITTOK_NEW_LINE)
        {
            longjmp(prs_jmpbuffer, 1);
            return;
        }
        switch(parser->current.type)
        {
            case LITTOK_CLASS:
            case LITTOK_FUNCTION:
            case LITTOK_EXPORT:
            case LITTOK_VAR:
            case LITTOK_CONST:
            case LITTOK_FOR:
            case LITTOK_STATIC:
            case LITTOK_IF:
            case LITTOK_WHILE:
            case LITTOK_RETURN:
            {
                longjmp(prs_jmpbuffer, 1);
                return;
            }
            default:
            {
                prs_advance(parser);
            }
        }
    }
}

static LitExpression* parse_declaration(LitParser* parser)
{
    LitExpression* statement;
    statement = NULL;
    if(prs_match(parser, LITTOK_CLASS) || prs_match(parser, LITTOK_STATIC))
    {
        statement = parse_class(parser);
    }
    else
    {
        statement = parse_statement(parser);
    }
    return statement;
}

bool lit_parse(LitParser* parser, const char* file_name, const char* source, LitExprList* statements)
{
    LitCompiler compiler;
    LitExpression* statement;
    parser->had_error = false;
    parser->panic_mode = false;
    lit_init_scanner(parser->state, parser->state->scanner, file_name, source);
    prs_init_compiler(parser, &compiler);
    prs_advance(parser);
    ignore_new_lines(parser, true);
    if(!prs_is_at_end(parser))
    {
        do
        {
            statement = parse_declaration(parser);
            if(statement != NULL)
            {
                lit_exprlist_push(parser->state, statements, statement);
            }
            if(!match_new_line(parser))
            {
                if(prs_match(parser, LITTOK_EOF))
                {
                    break;
                }
            }
        } while(!prs_is_at_end(parser));
    }
    return parser->had_error || parser->state->scanner->had_error;
}
