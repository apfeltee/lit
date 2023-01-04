
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "lit.h"

void lit_bytelist_init(LitByteList* bl)
{
    bl->values = NULL;
    bl->capacity = 0;
    bl->count = 0;
}

void lit_bytelist_destroy(LitState* state, LitByteList* bl)
{
    LIT_FREE_ARRAY(state, sizeof(uint8_t), bl->values, bl->capacity);
    lit_bytelist_init(bl);
}

void lit_bytelist_push(LitState* state, LitByteList* bl, uint8_t value)
{
    size_t oldcap;
    if(bl->capacity < bl->count + 1)
    {
        oldcap = bl->capacity;
        bl->capacity = LIT_GROW_CAPACITY(oldcap);
        bl->values = LIT_GROW_ARRAY(state, bl->values, sizeof(uint8_t), oldcap, bl->capacity);
    }
    bl->values[bl->count] = value;
    bl->count++;
}

void lit_lex_init(LitState* state, LitScanner* scanner, const char* file_name, const char* source)
{
    scanner->line = 1;
    scanner->start = source;
    scanner->current = source;
    scanner->file_name = file_name;
    scanner->state = state;
    scanner->num_braces = 0;
    scanner->had_error = false;
}

static LitToken lit_lex_maketoken(LitScanner* scanner, LitTokType type)
{
    LitToken token;
    token.type = type;
    token.start = scanner->start;
    token.length = (size_t)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static LitToken lit_lex_makeerrortoken(LitScanner* scanner, LitError lit_emitter_raiseerror, ...)
{
    va_list args;
    LitToken token;
    LitString* result;
    scanner->had_error = true;
    va_start(args, lit_emitter_raiseerror);
    result = lit_vformat_error(scanner->state, scanner->line, lit_emitter_raiseerror, args);
    va_end(args);
    token.type = LITTOK_ERROR;
    token.start = result->chars;
    token.length = lit_string_getlength(result);
    token.line = scanner->line;
    return token;
}

static bool lit_lex_isatend(LitScanner* scanner)
{
    return *scanner->current == '\0';
}

static char lit_lex_advance(LitScanner* scanner)
{
    scanner->current++;
    return scanner->current[-1];
}

static bool lit_lex_matchchar(LitScanner* scanner, char expected)
{
    if(lit_lex_isatend(scanner))
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

static LitToken lit_lex_matchtoken(LitScanner* scanner, char c, LitTokType a, LitTokType b)
{
    return lit_lex_maketoken(scanner, lit_lex_matchchar(scanner, c) ? a : b);
}

static LitToken lit_lex_matchntoken(LitScanner* scanner, char cr, char cb, LitTokType a, LitTokType b, LitTokType c)
{
    return lit_lex_maketoken(scanner, lit_lex_matchchar(scanner, cr) ? a : (lit_lex_matchchar(scanner, cb) ? b : c));
}

static char lit_lex_peekcurrent(LitScanner* scanner)
{
    return *scanner->current;
}

static char lit_lex_peeknext(LitScanner* scanner)
{
    if(lit_lex_isatend(scanner))
    {
        return '\0';
    }
    return scanner->current[1];
}

static bool lit_lex_skipspace(LitScanner* scanner)
{
    char a;
    char b;
    char c;
    (void)a;
    (void)b;
    while(true)
    {
        c = lit_lex_peekcurrent(scanner);
        switch(c)
        {
            case ' ':
            case '\r':
            case '\t':
                {
                    lit_lex_advance(scanner);
                }
                break;
            case '\n':
                {
                    scanner->start = scanner->current;
                    lit_lex_advance(scanner);
                    return true;
                }
                break;
            case '/':
                {
                    if(lit_lex_peeknext(scanner) == '/')
                    {
                        while(lit_lex_peekcurrent(scanner) != '\n' && !lit_lex_isatend(scanner))
                        {
                            lit_lex_advance(scanner);
                        }
                        return lit_lex_skipspace(scanner);
                    }
                    else if(lit_lex_peeknext(scanner) == '*')
                    {
                        lit_lex_advance(scanner);
                        lit_lex_advance(scanner);
                        a = lit_lex_peekcurrent(scanner);
                        b = lit_lex_peeknext(scanner);
                        while((lit_lex_peekcurrent(scanner) != '*' || lit_lex_peeknext(scanner) != '/') && !lit_lex_isatend(scanner))
                        {
                            if(lit_lex_peekcurrent(scanner) == '\n')
                            {
                                scanner->line++;
                            }
                            lit_lex_advance(scanner);
                        }
                        lit_lex_advance(scanner);
                        lit_lex_advance(scanner);
                        return lit_lex_skipspace(scanner);
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

static LitToken lit_lex_scanstring(LitScanner* scanner, bool interpolation)
{
    char c;
    LitState* state;
    LitByteList bytes;
    LitToken token;
    LitTokType string_type;
    state = scanner->state;
    string_type = LITTOK_STRING;
    lit_bytelist_init(&bytes);
    while(true)
    {
        c = lit_lex_advance(scanner);
        if(c == '\"')
        {
            break;
        }
        else if(interpolation && c == '{')
        {
            if(scanner->num_braces >= LIT_MAX_INTERPOLATION_NESTING)
            {
                return lit_lex_makeerrortoken(scanner, LITERROR_INTERPOLATION_NESTING_TOO_DEEP, LIT_MAX_INTERPOLATION_NESTING);
            }
            string_type = LITTOK_INTERPOLATION;
            scanner->braces[scanner->num_braces++] = 1;
            break;
        }
        switch(c)
        {
            case '\0':
                {
                    return lit_lex_makeerrortoken(scanner, LITERROR_UNTERMINATED_STRING);
                }
                break;
            case '\n':
                {
                    scanner->line++;
                    lit_bytelist_push(state, &bytes, c);
                }
                break;
            case '\\':
                {
                    switch(lit_lex_advance(scanner))
                    {
                        case '\"':
                            {
                                lit_bytelist_push(state, &bytes, '\"');
                            }
                            break;
                        case '\\':
                            {
                                lit_bytelist_push(state, &bytes, '\\');
                            }
                            break;
                        case '0':
                            {
                                lit_bytelist_push(state, &bytes, '\0');
                            }
                            break;
                        case '{':
                            {
                                lit_bytelist_push(state, &bytes, '{');
                            }
                            break;
                        case 'a':
                            {
                                lit_bytelist_push(state, &bytes, '\a');
                            }
                            break;
                        case 'b':
                            {
                                lit_bytelist_push(state, &bytes, '\b');
                            }
                            break;
                        case 'f':
                            {
                                lit_bytelist_push(state, &bytes, '\f');
                            }
                            break;
                        case 'n':
                            {
                                lit_bytelist_push(state, &bytes, '\n');
                            }
                            break;
                        case 'r':
                            {
                                lit_bytelist_push(state, &bytes, '\r');
                            }
                            break;
                        case 't':
                            {
                                lit_bytelist_push(state, &bytes, '\t');
                            }
                            break;
                        case 'v':
                            {
                                lit_bytelist_push(state, &bytes, '\v');
                            }
                            break;
                        default:
                            {
                                return lit_lex_makeerrortoken(scanner, LITERROR_INVALID_ESCAPE_CHAR, scanner->current[-1]);
                            }
                            break;
                    }
                }
                break;
            default:
                {
                    lit_bytelist_push(state, &bytes, c);
                }
                break;
        }
    }
    token = lit_lex_maketoken(scanner, string_type);
    token.value = lit_value_objectvalue(lit_string_copy(state, (const char*)bytes.values, bytes.count));
    lit_bytelist_destroy(state, &bytes);
    return token;
}

static int lit_lex_scanhexdigit(LitScanner* scanner)
{
    char c;
    c = lit_lex_advance(scanner);
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

static int lit_lex_scanbinarydigit(LitScanner* scanner)
{
    char c;
    c = lit_lex_advance(scanner);
    if(c >= '0' && c <= '1')
    {
        return c - '0';
    }
    scanner->current--;
    return -1;
}

static LitToken lit_lex_makenumbertoken(LitScanner* scanner, bool is_hex, bool is_binary)
{
    LitToken token;
    LitValue value;
    errno = 0;
    if(is_hex)
    {
        value = lit_value_numbertovalue(scanner->state, (double)strtoll(scanner->start, NULL, 16));
    }
    else if(is_binary)
    {
        value = lit_value_numbertovalue(scanner->state, (int)strtoll(scanner->start + 2, NULL, 2));
    }
    else
    {
        value = lit_value_numbertovalue(scanner->state, strtod(scanner->start, NULL));
    }

    if(errno == ERANGE)
    {
        errno = 0;
        return lit_lex_makeerrortoken(scanner, LITERROR_NUMBER_IS_TOO_BIG);
    }
    token = lit_lex_maketoken(scanner, LITTOK_NUMBER);
    token.value = value;
    return token;
}

static LitToken lit_lex_scannumber(LitScanner* scanner)
{
    if(lit_lex_matchchar(scanner, 'x'))
    {
        while(lit_lex_scanhexdigit(scanner) != -1)
        {
            continue;
        }
        return lit_lex_makenumbertoken(scanner, true, false);
    }
    if(lit_lex_matchchar(scanner, 'b'))
    {
        while(lit_lex_scanbinarydigit(scanner) != -1)
        {
            continue;
        }
        return lit_lex_makenumbertoken(scanner, false, true);
    }
    while(lit_is_digit(lit_lex_peekcurrent(scanner)))
    {
        lit_lex_advance(scanner);
    }
    // Look for a fractional part.
    if(lit_lex_peekcurrent(scanner) == '.' && lit_is_digit(lit_lex_peeknext(scanner)))
    {
        // Consume the '.'
        lit_lex_advance(scanner);
        while(lit_is_digit(lit_lex_peekcurrent(scanner)))
        {
            lit_lex_advance(scanner);
        }
    }
    return lit_lex_makenumbertoken(scanner, false, false);
}

static LitTokType lit_lex_checkkeyword(LitScanner* scanner, int start, int length, const char* rest, LitTokType type)
{
    if(scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0)
    {
        return type;
    }
    return LITTOK_IDENTIFIER;
}

static LitTokType lit_lex_scanidenttype(LitScanner* scanner)
{
    switch(scanner->start[0])
    {
        case 'b':
            return lit_lex_checkkeyword(scanner, 1, 4, "reak", LITTOK_BREAK);

        case 'c':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 'l':
                            return lit_lex_checkkeyword(scanner, 2, 3, "ass", LITTOK_CLASS);
                        case 'o':
                        {
                            if(scanner->current - scanner->start > 3)
                            {
                                switch(scanner->start[3])
                                {
                                    case 's':
                                        return lit_lex_checkkeyword(scanner, 2, 3, "nst", LITTOK_CONST);
                                    case 't':
                                        return lit_lex_checkkeyword(scanner, 2, 6, "ntinue", LITTOK_CONTINUE);
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
                            return lit_lex_checkkeyword(scanner, 2, 2, "se", LITTOK_ELSE);
                        case 'x':
                            return lit_lex_checkkeyword(scanner, 2, 4, "port", LITTOK_EXPORT);
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
                            return lit_lex_checkkeyword(scanner, 2, 3, "lse", LITTOK_FALSE);
                        case 'o':
                            return lit_lex_checkkeyword(scanner, 2, 1, "r", LITTOK_FOR);
                        case 'u':
                            return lit_lex_checkkeyword(scanner, 2, 6, "nction", LITTOK_FUNCTION);
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
                            return lit_lex_checkkeyword(scanner, 2, 0, "", LITTOK_IS);
                        case 'f':
                            return lit_lex_checkkeyword(scanner, 2, 0, "", LITTOK_IF);
                        case 'n':
                            return lit_lex_checkkeyword(scanner, 2, 0, "", LITTOK_IN);
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
                        return lit_lex_checkkeyword(scanner, 2, 2, "ll", LITTOK_NULL);
                    case 'e':
                        return lit_lex_checkkeyword(scanner, 2, 1, "w", LITTOK_NEW);
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
                        return lit_lex_checkkeyword(scanner, 3, 0, "", LITTOK_REF);
                    case 't':
                        return lit_lex_checkkeyword(scanner, 3, 3, "urn", LITTOK_RETURN);
                }
            }

            break;
        }

        case 'o':
            return lit_lex_checkkeyword(scanner, 1, 7, "perator", LITTOK_OPERATOR);

        case 's':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'u':
                        return lit_lex_checkkeyword(scanner, 2, 3, "per", LITTOK_SUPER);
                    case 't':
                        return lit_lex_checkkeyword(scanner, 2, 4, "atic", LITTOK_STATIC);
                    case 'e':
                        return lit_lex_checkkeyword(scanner, 2, 1, "t", LITTOK_SET);
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
                        return lit_lex_checkkeyword(scanner, 2, 2, "is", LITTOK_THIS);
                    case 'r':
                        return lit_lex_checkkeyword(scanner, 2, 2, "ue", LITTOK_TRUE);
                }
            }

            break;
        }

        case 'v':
            return lit_lex_checkkeyword(scanner, 1, 2, "ar", LITTOK_VAR);
        case 'w':
            return lit_lex_checkkeyword(scanner, 1, 4, "hile", LITTOK_WHILE);
        case 'g':
            return lit_lex_checkkeyword(scanner, 1, 2, "et", LITTOK_GET);
    }

    return LITTOK_IDENTIFIER;
}

static LitToken lit_lex_scanidentifier(LitScanner* scanner)
{
    while(lit_is_alpha(lit_lex_peekcurrent(scanner)) || lit_is_digit(lit_lex_peekcurrent(scanner)))
    {
        lit_lex_advance(scanner);
    }

    return lit_lex_maketoken(scanner, lit_lex_scanidenttype(scanner));
}

LitToken lit_lex_rollback(LitScanner* scanner)
{
    //scanner->current--;
}

LitToken lit_lex_scantoken(LitScanner* scanner)
{
    if(lit_lex_skipspace(scanner))
    {
        LitToken token = lit_lex_maketoken(scanner, LITTOK_NEW_LINE);
        scanner->line++;

        return token;
    }

    scanner->start = scanner->current;

    if(lit_lex_isatend(scanner))
    {
        return lit_lex_maketoken(scanner, LITTOK_EOF);
    }

    char c = lit_lex_advance(scanner);

    if(lit_is_digit(c))
    {
        return lit_lex_scannumber(scanner);
    }

    if(lit_is_alpha(c))
    {
        return lit_lex_scanidentifier(scanner);
    }

    switch(c)
    {
        case '(':
            return lit_lex_maketoken(scanner, LITTOK_LEFT_PAREN);
        case ')':
            return lit_lex_maketoken(scanner, LITTOK_RIGHT_PAREN);

        case '{':
        {
            if(scanner->num_braces > 0)
            {
                scanner->braces[scanner->num_braces - 1]++;
            }

            return lit_lex_maketoken(scanner, LITTOK_LEFT_BRACE);
        }

        case '}':
        {
            if(scanner->num_braces > 0 && --scanner->braces[scanner->num_braces - 1] == 0)
            {
                scanner->num_braces--;
                return lit_lex_scanstring(scanner, true);
            }

            return lit_lex_maketoken(scanner, LITTOK_RIGHT_BRACE);
        }

        case '[':
            return lit_lex_maketoken(scanner, LITTOK_LEFT_BRACKET);
        case ']':
            return lit_lex_maketoken(scanner, LITTOK_RIGHT_BRACKET);
        case ';':
            return lit_lex_maketoken(scanner, LITTOK_SEMICOLON);
        case ',':
            return lit_lex_maketoken(scanner, LITTOK_COMMA);
        case ':':
            return lit_lex_maketoken(scanner, LITTOK_COLON);
        case '~':
            return lit_lex_maketoken(scanner, LITTOK_TILDE);

        case '+':
            return lit_lex_matchntoken(scanner, '=', '+', LITTOK_PLUS_EQUAL, LITTOK_PLUS_PLUS, LITTOK_PLUS);
        case '-':
            return lit_lex_matchchar(scanner, '>') ? lit_lex_maketoken(scanner, LITTOK_SMALL_ARROW) :
                                         lit_lex_matchntoken(scanner, '=', '-', LITTOK_MINUS_EQUAL, LITTOK_MINUS_MINUS, LITTOK_MINUS);
        case '/':
            return lit_lex_matchtoken(scanner, '=', LITTOK_SLASH_EQUAL, LITTOK_SLASH);
        case '#':
            return lit_lex_matchtoken(scanner, '=', LITTOK_SHARP_EQUAL, LITTOK_SHARP);
        case '!':
            return lit_lex_matchtoken(scanner, '=', LITTOK_BANG_EQUAL, LITTOK_BANG);
        case '?':
            return lit_lex_matchtoken(scanner, '?', LITTOK_QUESTION_QUESTION, LITTOK_QUESTION);
        case '%':
            return lit_lex_matchtoken(scanner, '=', LITTOK_PERCENT_EQUAL, LITTOK_PERCENT);
        case '^':
            return lit_lex_matchtoken(scanner, '=', LITTOK_CARET_EQUAL, LITTOK_CARET);

        case '>':
            return lit_lex_matchntoken(scanner, '=', '>', LITTOK_GREATER_EQUAL, LITTOK_GREATER_GREATER, LITTOK_GREATER);
        case '<':
            return lit_lex_matchntoken(scanner, '=', '<', LITTOK_LESS_EQUAL, LITTOK_LESS_LESS, LITTOK_LESS);
        case '*':
            return lit_lex_matchntoken(scanner, '=', '*', LITTOK_STAR_EQUAL, LITTOK_STAR_STAR, LITTOK_STAR);
        case '=':
            return lit_lex_matchntoken(scanner, '=', '>', LITTOK_EQUAL_EQUAL, LITTOK_ARROW, LITTOK_EQUAL);
        case '|':
            return lit_lex_matchntoken(scanner, '=', '|', LITTOK_BAR_EQUAL, LITTOK_BAR_BAR, LITTOK_BAR);
        case '&':
            return lit_lex_matchntoken(scanner, '=', '&', LITTOK_AMPERSAND_EQUAL, LITTOK_AMPERSAND_AMPERSAND, LITTOK_AMPERSAND);


        case '.':
        {
            if(!lit_lex_matchchar(scanner, '.'))
            {
                return lit_lex_maketoken(scanner, LITTOK_DOT);
            }

            return lit_lex_matchtoken(scanner, '.', LITTOK_DOT_DOT_DOT, LITTOK_DOT_DOT);
        }

        case '$':
        {
            if(!lit_lex_matchchar(scanner, '\"'))
            {
                return lit_lex_makeerrortoken(scanner, LITERROR_CHAR_EXPECTATION_UNMET, '\"', '$', lit_lex_peekcurrent(scanner));
            }

            return lit_lex_scanstring(scanner, true);
        }

        case '"':
            return lit_lex_scanstring(scanner, false);
    }

    return lit_lex_makeerrortoken(scanner, LITERROR_UNEXPECTED_CHAR, c);
}