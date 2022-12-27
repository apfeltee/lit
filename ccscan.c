
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "lit.h"
#include "priv.h"

void lit_init_scanner(LitState* state, LitScanner* scanner, const char* file_name, const char* source)
{
    scanner->line = 1;
    scanner->start = source;
    scanner->current = source;
    scanner->file_name = file_name;
    scanner->state = state;
    scanner->num_braces = 0;
    scanner->had_error = false;
}

static LitToken make_token(LitScanner* scanner, LitTokenType type)
{
    LitToken token;
    token.type = type;
    token.start = scanner->start;
    token.length = (size_t)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static LitToken make_error_token(LitScanner* scanner, LitError error, ...)
{
    va_list args;
    LitToken token;
    LitString* result;
    scanner->had_error = true;
    va_start(args, error);
    result = lit_vformat_error(scanner->state, scanner->line, error, args);
    va_end(args);
    token.type = LITTOK_ERROR;
    token.start = result->chars;
    token.length = lit_string_length(result);
    token.line = scanner->line;
    return token;
}

static bool is_at_end(LitScanner* scanner)
{
    return *scanner->current == '\0';
}

static char advance(LitScanner* scanner)
{
    scanner->current++;
    return scanner->current[-1];
}

static bool match(LitScanner* scanner, char expected)
{
    if(is_at_end(scanner))
    {
        return false;
    }

    if(*scanner->current != expected)
    {
        return false;
    }
    scanner->current++;
    return true;
}

static LitToken match_token(LitScanner* scanner, char c, LitTokenType a, LitTokenType b)
{
    return make_token(scanner, match(scanner, c) ? a : b);
}

static LitToken match_tokens(LitScanner* scanner, char cr, char cb, LitTokenType a, LitTokenType b, LitTokenType c)
{
    return make_token(scanner, match(scanner, cr) ? a : (match(scanner, cb) ? b : c));
}

static char peek(LitScanner* scanner)
{
    return *scanner->current;
}

static char peek_next(LitScanner* scanner)
{
    if(is_at_end(scanner))
    {
        return '\0';
    }
    return scanner->current[1];
}

static bool skip_whitespace(LitScanner* scanner)
{
    char a;
    char b;
    char c;
    (void)a;
    (void)b;
    while(true)
    {
        c = peek(scanner);
        switch(c)
        {
            case ' ':
            case '\r':
            case '\t':
                {
                    advance(scanner);
                }
                break;
            case '\n':
                {
                    scanner->start = scanner->current;
                    advance(scanner);
                    return true;
                }
                break;
            case '/':
                {
                    if(peek_next(scanner) == '/')
                    {
                        while(peek(scanner) != '\n' && !is_at_end(scanner))
                        {
                            advance(scanner);
                        }
                        return skip_whitespace(scanner);
                    }
                    else if(peek_next(scanner) == '*')
                    {
                        advance(scanner);
                        advance(scanner);
                        a = peek(scanner);
                        b = peek_next(scanner);
                        while((peek(scanner) != '*' || peek_next(scanner) != '/') && !is_at_end(scanner))
                        {
                            if(peek(scanner) == '\n')
                            {
                                scanner->line++;
                            }
                            advance(scanner);
                        }
                        advance(scanner);
                        advance(scanner);
                        return skip_whitespace(scanner);
                    }
                    return false;
                }
                break;
            default:
                {
                    return false;
                }
        }
    }
}

static LitToken scan_string(LitScanner* scanner, bool interpolation)
{
    char c;
    LitState* state;
    LitBytes bytes;
    LitToken token;
    LitTokenType string_type;
    state = scanner->state;
    string_type = LITTOK_STRING;
    lit_init_bytes(&bytes);
    while(true)
    {
        c = advance(scanner);
        if(c == '\"')
        {
            break;
        }
        else if(interpolation && c == '{')
        {
            if(scanner->num_braces >= LIT_MAX_INTERPOLATION_NESTING)
            {
                return make_error_token(scanner, LITERROR_INTERPOLATION_NESTING_TOO_DEEP, LIT_MAX_INTERPOLATION_NESTING);
            }
            string_type = LITTOK_INTERPOLATION;
            scanner->braces[scanner->num_braces++] = 1;
            break;
        }
        switch(c)
        {
            case '\0':
                {
                    return make_error_token(scanner, LITERROR_UNTERMINATED_STRING);
                }
                break;
            case '\n':
                {
                    scanner->line++;
                    lit_bytes_write(state, &bytes, c);
                }
                break;
            case '\\':
                {
                    switch(advance(scanner))
                    {
                        case '\"':
                            {
                                lit_bytes_write(state, &bytes, '\"');
                            }
                            break;
                        case '\\':
                            {
                                lit_bytes_write(state, &bytes, '\\');
                            }
                            break;
                        case '0':
                            {
                                lit_bytes_write(state, &bytes, '\0');
                            }
                            break;
                        case '{':
                            {
                                lit_bytes_write(state, &bytes, '{');
                            }
                            break;
                        case 'a':
                            {
                                lit_bytes_write(state, &bytes, '\a');
                            }
                            break;
                        case 'b':
                            {
                                lit_bytes_write(state, &bytes, '\b');
                            }
                            break;
                        case 'f':
                            {
                                lit_bytes_write(state, &bytes, '\f');
                            }
                            break;
                        case 'n':
                            {
                                lit_bytes_write(state, &bytes, '\n');
                            }
                            break;
                        case 'r':
                            {
                                lit_bytes_write(state, &bytes, '\r');
                            }
                            break;
                        case 't':
                            {
                                lit_bytes_write(state, &bytes, '\t');
                            }
                            break;
                        case 'v':
                            {
                                lit_bytes_write(state, &bytes, '\v');
                            }
                            break;
                        default:
                            {
                                return make_error_token(scanner, LITERROR_INVALID_ESCAPE_CHAR, scanner->current[-1]);
                            }
                            break;
                    }
                }
                break;
            default:
                {
                    lit_bytes_write(state, &bytes, c);
                }
                break;
        }
    }
    token = make_token(scanner, string_type);
    token.value = OBJECT_VALUE(lit_string_copy(state, (const char*)bytes.values, bytes.count));
    lit_free_bytes(state, &bytes);
    return token;
}

static int scan_hexdigit(LitScanner* scanner)
{
    char c;
    c = advance(scanner);
    if((c >= '0') && (c <= '9'))
    {
        return (c - '0');
    }
    if((c >= 'a') && (c <= 'f'))
    {
        return (c - 'a' + 10);
    }
    if((c >= 'A') && (c <= 'F'))
    {
        return (c - 'A' + 10);
    }
    scanner->current--;
    return -1;
}

static int scan_binarydigit(LitScanner* scanner)
{
    char c;
    c = advance(scanner);
    if(c >= '0' && c <= '1')
    {
        return c - '0';
    }
    scanner->current--;
    return -1;
}

static LitToken make_number_token(LitScanner* scanner, bool is_hex, bool is_binary)
{
    LitToken token;
    LitValue value;
    errno = 0;
    if(is_hex)
    {
        value = lit_number_to_value((double)strtoll(scanner->start, NULL, 16));
    }
    else if(is_binary)
    {
        value = lit_number_to_value((int)strtoll(scanner->start + 2, NULL, 2));
    }
    else
    {
        value = lit_number_to_value(strtod(scanner->start, NULL));
    }

    if(errno == ERANGE)
    {
        errno = 0;
        return make_error_token(scanner, LITERROR_NUMBER_IS_TOO_BIG);
    }
    token = make_token(scanner, LITTOK_NUMBER);
    token.value = value;
    return token;
}

static LitToken scan_number(LitScanner* scanner)
{
    if(match(scanner, 'x'))
    {
        while(scan_hexdigit(scanner) != -1)
        {
            continue;
        }
        return make_number_token(scanner, true, false);
    }
    if(match(scanner, 'b'))
    {
        while(scan_binarydigit(scanner) != -1)
        {
            continue;
        }
        return make_number_token(scanner, false, true);
    }
    while(lit_is_digit(peek(scanner)))
    {
        advance(scanner);
    }
    // Look for a fractional part.
    if(peek(scanner) == '.' && lit_is_digit(peek_next(scanner)))
    {
        // Consume the '.'
        advance(scanner);
        while(lit_is_digit(peek(scanner)))
        {
            advance(scanner);
        }
    }
    return make_number_token(scanner, false, false);
}

static LitTokenType check_keyword(LitScanner* scanner, int start, int length, const char* rest, LitTokenType type)
{
    if(scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0)
    {
        return type;
    }
    return LITTOK_IDENTIFIER;
}

static LitTokenType scan_identtype(LitScanner* scanner)
{
    switch(scanner->start[0])
    {
        case 'b':
            return check_keyword(scanner, 1, 4, "reak", LITTOK_BREAK);

        case 'c':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 'l':
                            return check_keyword(scanner, 2, 3, "ass", LITTOK_CLASS);
                        case 'o':
                        {
                            if(scanner->current - scanner->start > 3)
                            {
                                switch(scanner->start[3])
                                {
                                    case 's':
                                        return check_keyword(scanner, 2, 3, "nst", LITTOK_CONST);
                                    case 't':
                                        return check_keyword(scanner, 2, 6, "ntinue", LITTOK_CONTINUE);
                                }
                            }
                        }
                    }
                }
            }
            break;
        case 'e':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 'l':
                            return check_keyword(scanner, 2, 2, "se", LITTOK_ELSE);
                        case 'x':
                            return check_keyword(scanner, 2, 4, "port", LITTOK_EXPORT);
                    }
                }
            }
            break;
        case 'f':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 'a':
                            return check_keyword(scanner, 2, 3, "lse", LITTOK_FALSE);
                        case 'o':
                            return check_keyword(scanner, 2, 1, "r", LITTOK_FOR);
                        case 'u':
                            return check_keyword(scanner, 2, 6, "nction", LITTOK_FUNCTION);
                    }
                }
            }
            break;
        case 'i':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 's':
                            return check_keyword(scanner, 2, 0, "", LITTOK_IS);
                        case 'f':
                            return check_keyword(scanner, 2, 0, "", LITTOK_IF);
                        case 'n':
                            return check_keyword(scanner, 2, 0, "", LITTOK_IN);
                    }
                }
            }
            break;
        case 'n':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'u':
                        return check_keyword(scanner, 2, 2, "ll", LITTOK_NULL);
                    case 'e':
                        return check_keyword(scanner, 2, 1, "w", LITTOK_NEW);
                }
            }

            break;
        }

        case 'r':
        {
            if(scanner->current - scanner->start > 2)
            {
                switch(scanner->start[2])
                {
                    case 'f':
                        return check_keyword(scanner, 3, 0, "", LITTOK_REF);
                    case 't':
                        return check_keyword(scanner, 3, 3, "urn", LITTOK_RETURN);
                }
            }

            break;
        }

        case 'o':
            return check_keyword(scanner, 1, 7, "perator", LITTOK_OPERATOR);

        case 's':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'u':
                        return check_keyword(scanner, 2, 3, "per", LITTOK_SUPER);
                    case 't':
                        return check_keyword(scanner, 2, 4, "atic", LITTOK_STATIC);
                    case 'e':
                        return check_keyword(scanner, 2, 1, "t", LITTOK_SET);
                }
            }

            break;
        }

        case 't':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'h':
                        return check_keyword(scanner, 2, 2, "is", LITTOK_THIS);
                    case 'r':
                        return check_keyword(scanner, 2, 2, "ue", LITTOK_TRUE);
                }
            }

            break;
        }

        case 'v':
            return check_keyword(scanner, 1, 2, "ar", LITTOK_VAR);
        case 'w':
            return check_keyword(scanner, 1, 4, "hile", LITTOK_WHILE);
        case 'g':
            return check_keyword(scanner, 1, 2, "et", LITTOK_GET);
    }

    return LITTOK_IDENTIFIER;
}

static LitToken scan_identifier(LitScanner* scanner)
{
    while(lit_is_alpha(peek(scanner)) || lit_is_digit(peek(scanner)))
    {
        advance(scanner);
    }

    return make_token(scanner, scan_identtype(scanner));
}

LitToken lit_scan_rollback(LitScanner* scanner)
{
    //scanner->current--;
}

LitToken lit_scan_token(LitScanner* scanner)
{
    if(skip_whitespace(scanner))
    {
        LitToken token = make_token(scanner, LITTOK_NEW_LINE);
        scanner->line++;

        return token;
    }

    scanner->start = scanner->current;

    if(is_at_end(scanner))
    {
        return make_token(scanner, LITTOK_EOF);
    }

    char c = advance(scanner);

    if(lit_is_digit(c))
    {
        return scan_number(scanner);
    }

    if(lit_is_alpha(c))
    {
        return scan_identifier(scanner);
    }

    switch(c)
    {
        case '(':
            return make_token(scanner, LITTOK_LEFT_PAREN);
        case ')':
            return make_token(scanner, LITTOK_RIGHT_PAREN);

        case '{':
        {
            if(scanner->num_braces > 0)
            {
                scanner->braces[scanner->num_braces - 1]++;
            }

            return make_token(scanner, LITTOK_LEFT_BRACE);
        }

        case '}':
        {
            if(scanner->num_braces > 0 && --scanner->braces[scanner->num_braces - 1] == 0)
            {
                scanner->num_braces--;
                return scan_string(scanner, true);
            }

            return make_token(scanner, LITTOK_RIGHT_BRACE);
        }

        case '[':
            return make_token(scanner, LITTOK_LEFT_BRACKET);
        case ']':
            return make_token(scanner, LITTOK_RIGHT_BRACKET);
        case ';':
            return make_token(scanner, LITTOK_SEMICOLON);
        case ',':
            return make_token(scanner, LITTOK_COMMA);
        case ':':
            return make_token(scanner, LITTOK_COLON);
        case '~':
            return make_token(scanner, LITTOK_TILDE);

        case '+':
            return match_tokens(scanner, '=', '+', LITTOK_PLUS_EQUAL, LITTOK_PLUS_PLUS, LITTOK_PLUS);
        case '-':
            return match(scanner, '>') ? make_token(scanner, LITTOK_SMALL_ARROW) :
                                         match_tokens(scanner, '=', '-', LITTOK_MINUS_EQUAL, LITTOK_MINUS_MINUS, LITTOK_MINUS);
        case '/':
            return match_token(scanner, '=', LITTOK_SLASH_EQUAL, LITTOK_SLASH);
        case '#':
            return match_token(scanner, '=', LITTOK_SHARP_EQUAL, LITTOK_SHARP);
        case '!':
            return match_token(scanner, '=', LITTOK_BANG_EQUAL, LITTOK_BANG);
        case '?':
            return match_token(scanner, '?', LITTOK_QUESTION_QUESTION, LITTOK_QUESTION);
        case '%':
            return match_token(scanner, '=', LITTOK_PERCENT_EQUAL, LITTOK_PERCENT);
        case '^':
            return match_token(scanner, '=', LITTOK_CARET_EQUAL, LITTOK_CARET);

        case '>':
            return match_tokens(scanner, '=', '>', LITTOK_GREATER_EQUAL, LITTOK_GREATER_GREATER, LITTOK_GREATER);
        case '<':
            return match_tokens(scanner, '=', '<', LITTOK_LESS_EQUAL, LITTOK_LESS_LESS, LITTOK_LESS);
        case '*':
            return match_tokens(scanner, '=', '*', LITTOK_STAR_EQUAL, LITTOK_STAR_STAR, LITTOK_STAR);
        case '=':
            return match_tokens(scanner, '=', '>', LITTOK_EQUAL_EQUAL, LITTOK_ARROW, LITTOK_EQUAL);
        case '|':
            return match_tokens(scanner, '=', '|', LITTOK_BAR_EQUAL, LITTOK_BAR_BAR, LITTOK_BAR);
        case '&':
            return match_tokens(scanner, '=', '&', LITTOK_AMPERSAND_EQUAL, LITTOK_AMPERSAND_AMPERSAND, LITTOK_AMPERSAND);


        case '.':
        {
            if(!match(scanner, '.'))
            {
                return make_token(scanner, LITTOK_DOT);
            }

            return match_token(scanner, '.', LITTOK_DOT_DOT_DOT, LITTOK_DOT_DOT);
        }

        case '$':
        {
            if(!match(scanner, '\"'))
            {
                return make_error_token(scanner, LITERROR_CHAR_EXPECTATION_UNMET, '\"', '$', peek(scanner));
            }

            return scan_string(scanner, true);
        }

        case '"':
            return scan_string(scanner, false);
    }

    return make_error_token(scanner, LITERROR_UNEXPECTED_CHAR, c);
}