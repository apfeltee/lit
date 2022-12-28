
#include <stdio.h>
#include "lit.h"
#include "priv.h"

void lit_init_preprocessor(LitState* state, LitPreprocessor* preprocessor)
{
    preprocessor->state = state;

    lit_init_table(&preprocessor->defined);
    lit_vallist_init(&preprocessor->open_ifs);
}

void lit_free_preprocessor(LitPreprocessor* preprocessor)
{
    lit_free_table(preprocessor->state, &preprocessor->defined);
    lit_vallist_destroy(preprocessor->state, &preprocessor->open_ifs);
}

void lit_add_definition(LitState* state, const char* name)
{
    lit_table_set(state, &state->preprocessor->defined, CONST_STRING(state, name), TRUE_VALUE);
}

static void override(char* source, int length)
{
    while(length-- > 0)
    {
        if(*source != '\n')
        {
            *source = ' ';
        }
        source++;
    }
}

bool lit_preprocess(LitPreprocessor* preprocessor, char* source)
{
    bool close;
    bool in_macro;
    bool in_arg;
    bool on_new_line;
    int ignore_depth;
    int depth;
    char c;
    char* branch_start;
    char* macro_start;
    char* arg_start;
    char* current;
    LitState* state;
    LitValue tmp;
    LitString* arg;

    state = preprocessor->state;
    macro_start = source;
    arg_start = source;
    current = source;
    in_macro = false;
    in_arg = false;
    on_new_line = true;
    ignore_depth = -1;
    depth = 0;
    do
    {
        c = current[0];
        current++;
        // Single line comment
        if(c == '/' && current[0] == '/')
        {
            current++;
            do
            {
                c = current[0];
                current++;
            } while(c != '\n' && c != '\0');
            in_macro = false;
            on_new_line = true;
            continue;
        }
        else if(c == '/' && current[0] == '*')
        {
            // Multiline comment
            current++;
            do
            {
                c = current[0];
                current++;
            } while(c != '*' && c != '\0' && current[0] != '/');
            in_macro = false;
            on_new_line = true;
            continue;
        }
        if(in_macro)
        {
            if(!lit_is_alpha(c) && !(((current - macro_start) > 1) && lit_is_digit(c)))
            {
                if(in_arg)
                {
                    arg = lit_string_copy(state, arg_start, (int)(current - arg_start) - 1);
                    if(memcmp(macro_start, "define", 6) == 0 || memcmp(macro_start, "undef", 5) == 0)
                    {
                        if(ignore_depth < 0)
                        {
                            close = macro_start[0] == 'u';
                            if(close)
                            {
                                lit_table_delete(&preprocessor->defined, arg);
                            }
                            else
                            {
                                lit_table_set(state, &preprocessor->defined, arg, TRUE_VALUE);
                            }
                        }
                    }
                    else
                    {// ifdef || ifndef
                        depth++;
                        if(ignore_depth < 0)
                        {
                            close = macro_start[2] == 'n';
                            if((lit_table_get(&preprocessor->defined, arg, &tmp) ^ close) == false)
                            {
                                ignore_depth = depth;
                            }

                            lit_vallist_push(preprocessor->state, &preprocessor->open_ifs, (LitValue)macro_start);
                        }
                    }
                    // Remove the macro from code
                    override(macro_start - 1, (int)(current - macro_start));
                    in_macro = false;
                    in_arg = false;
                }
                else
                {
                    if(memcmp(macro_start, "define", 6) == 0 || memcmp(macro_start, "undef", 5) == 0
                       || memcmp(macro_start, "ifdef", 5) == 0 || memcmp(macro_start, "ifndef", 6) == 0)
                    {
                        arg_start = current;
                        in_arg = true;
                    }
                    else if(memcmp(macro_start, "else", 4) == 0 || memcmp(macro_start, "endif", 5) == 0)
                    {
                        in_macro = false;
                        // If this is endif
                        if(macro_start[1] == 'n')
                        {
                            depth--;
                            if(ignore_depth > -1)
                            {
                                // Remove the whole if branch from code
                                branch_start = (char*)lit_vallist_get(&preprocessor->open_ifs, lit_vallist_count(&preprocessor->open_ifs) - 1);
                                override(branch_start - 1, (int)(current - branch_start));
                                if(ignore_depth == depth + 1)
                                {
                                    ignore_depth = -1;
                                    lit_vallist_deccount(&preprocessor->open_ifs);
                                }
                            }
                            else
                            {
                                lit_vallist_deccount(&preprocessor->open_ifs);
                                // Remove #endif
                                override(macro_start - 1, (int)(current - macro_start));
                            }
                        }
                        else if(ignore_depth < 0 || depth <= ignore_depth)
                        {
                            // #else
                            if(ignore_depth == depth)
                            {
                                // Remove the macro from code
                                branch_start = (char*)lit_vallist_get(&preprocessor->open_ifs, lit_vallist_count(&preprocessor->open_ifs) - 1);
                                override(branch_start - 1, (int)(current - branch_start));
                                ignore_depth = -1;
                            }
                            else
                            {
                                lit_vallist_set(&preprocessor->open_ifs, lit_vallist_count(&preprocessor->open_ifs) - 1, (LitValue)macro_start);
                                ignore_depth = depth;
                            }
                        }
                    }
                    else
                    {
                        lit_error(preprocessor->state, (LitErrorType)0,
                                  lit_format_error(preprocessor->state, 0, LITERROR_UNKNOWN_MACRO, (int)(current - macro_start) - 1, macro_start)
                                  ->chars);
                        return false;
                    }
                }
            }
        }
        else
        {
            macro_start = current;
            if(c == '\n')
            {
                on_new_line = true;
            }
            else if(!(c == '\t' || c == ' ' || c == '#'))
            {
                on_new_line = false;
            }
            else
            {
                in_macro = on_new_line && c == '#';
            }
        }
    } while(c != '\0');
    if(in_macro || lit_vallist_count(&preprocessor->open_ifs) > 0 || depth > 0)
    {
        lit_error(preprocessor->state, (LitErrorType)0, lit_format_error(preprocessor->state, 0, LITERROR_UNCLOSED_MACRO)->chars);
        return false;
    }
    lit_vallist_destroy(preprocessor->state, &preprocessor->open_ifs);
    return true;
}
