
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#if defined(__unix__) || defined(__linux__)
    #include <dirent.h>
#endif

#ifndef __TINYC__
    #if __has_include(<readline/readline.h>)
        #include <readline/readline.h>
        #include <readline/history.h>
        #define LIT_HAVE_READLINE
    #endif
#endif

#include "lit.h"

enum
{
    MAX_RESTARGS = 1024,
    MAX_OPTS = 1024,
};

typedef struct Flag_t Flag_t;
typedef struct FlagContext_t FlagContext_t;

struct Flag_t
{
    char flag;
    char* value;
};

struct FlagContext_t
{
    int nargc;
    int fcnt;
    int poscnt;
    char* positional[MAX_RESTARGS + 1];
    Flag_t flags[MAX_OPTS + 1];
};

typedef struct Options_t Options_t;

// Used for clean up on Ctrl+C / Ctrl+Z
static LitState* repl_state;

static bool populate_flags(int argc, int begin, char** argv, const char* expectvalue, FlagContext_t* fx)
{
    int i;
    int nextch;
    int psidx;
    int flidx;
    char* arg;
    char* nextarg;
    psidx = 0;
    flidx = 0;
    fx->fcnt = 0;
    fx->poscnt = 0;
    for(i=begin; i<argc; i++)
    {
        arg = argv[i];
        nextarg = NULL;
        if((i+1) < argc)
        {
            nextarg = argv[i+1];
        }
        if(arg[0] == '-')
        {
            fx->flags[flidx].flag = arg[1];
            fx->flags[flidx].value = NULL;
            if(strchr(expectvalue, arg[1]) != NULL)
            {
                nextch = arg[2];
                /* -e "somecode(...)" */
                /* -e is followed by text: -e"somecode(...)" */
                if(nextch != 0)
                {
                    fx->flags[flidx].value = arg + 2;
                }
                else if(nextarg != NULL)
                {
                    if(nextarg[0] != '-')
                    {
                        fx->flags[flidx].value = nextarg;
                        i++;
                    }
                }
                else
                {
                    fx->flags[flidx].value = NULL;
                }
            }
            flidx++;
        }
        else
        {
            fx->positional[psidx] = arg;
            psidx++;
        }
    }
    fx->fcnt = flidx;
    fx->poscnt = psidx;
    fx->nargc = i;
    return true;
}

static void show_help()
{
    printf("lit [options] [files]\n");
    printf("    -o --output [file]  Instead of running the file the compiled bytecode will be saved.\n");
    printf(" -O[name] [string] Enables given optimization. For the list of aviable optimizations run with -Ohelp\n");
    printf(" -D[name]  Defines given symbol.\n");
    printf(" -e --eval [string] Runs the given code string.\n");
    printf(" -p --pass [args] Passes the rest of the arguments to the script.\n");
    printf(" -i --interactive Starts an interactive shell.\n");
    printf(" -d --dump  Dumps all the bytecode chunks from the given file.\n");
    printf(" -t --time  Measures and prints the compilation timings.\n");
    printf(" -h --help  I wonder, what this option does.\n");
    printf(" If no code to run is provided, lit will try to run either main.lbc or main.lit and, if fails, default to an interactive shell will start.\n");
}

static void show_optimization_help()
{
    int i;
    printf(
        "Lit has a lot of optimzations.\n"
        "You can turn each one on or off or use a predefined optimization level to set them to a default value.\n"
        "The more optimizations are enabled, the longer it takes to compile, but the program should run better.\n"
        "So I recommend using low optimization for development and high optimization for release.\n"
        "To enable an optimization, run lit with argument -O[optimization], for example -Oconstant-folding.\n"
        "Using flag -Oall will enable all optimizations.\n"
        "To disable an optimization, run lit with argument -Ono-[optimization], for example -Ono-constant-folding.\n"
        "Using flag -Oall will disable all optimizations.\n"
    );
    printf("Here is a list of all supported optimizations:\n\n");
    for(i = 0; i < LITOPTSTATE_TOTAL; i++)
    {
        printf(" %s  %s\n", lit_get_optimization_name((LitOptimization)i),
               lit_get_optimization_description((LitOptimization)i));
    }
    printf("\nIf you want to use a predefined optimization level (recommended), run lit with argument -O[optimization level], for example -O1.\n\n");
    for(i = 0; i < LITOPTLEVEL_TOTAL; i++)
    {
        printf("\t-O%i\t\t%s\n", i, lit_get_optimization_level_description((LitOptimizationLevel)i));
    }
}

static bool match_arg(const char* arg, const char* a, const char* b)
{
    return strcmp(arg, a) == 0 || strcmp(arg, b) == 0;
}

int exitstate(LitState* state, LitInterpretResultType result)
{
    int64_t amount;
    amount = lit_free_state(state);
    if((result != LITRESULT_COMPILE_ERROR) && amount != 0)
    {
        fprintf(stderr, "gc: freed residual %i bytes\n", (int)amount);
        //return LIT_EXIT_CODE_MEM_LEAK;
        return 0;
    }
    if(result != LITRESULT_OK)
    {
        /*
        if(result == LITRESULT_RUNTIME_ERROR)
        {
            return LIT_EXIT_CODE_RUNTIME_ERROR;
        }
        else
        {
            return LIT_EXIT_CODE_COMPILE_ERROR;
        }
        */
        return 1;
    }
    return 0;
}

struct Options_t
{
    char* debugmode;
    char* codeline;
};


static bool parse_options(Options_t* opts, Flag_t* flags, int fcnt)
{
    int i;
    opts->codeline = NULL;
    opts->debugmode = NULL;
    for(i=0; i<fcnt; i++)
    {
        switch(flags[i].flag)
        {
            case 'h':
                {
                    show_help();
                    return false;
                }
                break;
            case 'e':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-e' expects a string\n");
                        return false;
                    }
                    opts->codeline = flags[i].value;
                }
                break;
            case 'd':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-d' expects a value. run '-h' for possible values\n");
                        return false;
                    }
                    opts->debugmode = flags[i].value;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

void interupt_handler(int signal_id)
{
    (void)signal_id;
    lit_free_state(repl_state);
    printf("\nExiting.\n");
    exit(0);
}

#if defined(LIT_HAVE_READLINE)
static int run_repl(LitState* state)
{
    fprintf(stderr, "in repl...\n");
    char* line;
    repl_state = state;
    signal(SIGINT, interupt_handler);
    //signal(SIGTSTP, interupt_handler);
    lit_set_optimization_level(LITOPTLEVEL_REPL);
    printf("lit v%s, developed by @egordorichev\n", LIT_VERSION_STRING);
    while(true)
    {
        line = readline("> ");
        if(line == NULL)
        {
            return 0;
        }
        add_history(line);
        LitInterpretResult result = lit_interpret_source(state, "repl", line, strlen(line));
        if(result.type == LITRESULT_OK && result.result != NULL_VALUE)
        {
            printf("%s%s%s\n", COLOR_GREEN, lit_to_string(state, result.result)->chars, COLOR_RESET);
        }
    }
    return 0;
}
#endif

int main(int argc, char* argv[])
{
    int i;
    bool cmdfailed;
    bool replexit;
    const char* dm;
    const char* filename;
    LitArray* arg_array;
    LitState* state;
    FlagContext_t fx;
    Options_t opts;
    LitInterpretResultType result;
    replexit = false;
    cmdfailed = false;
    result = LITRESULT_OK;
    populate_flags(argc, 1, argv, "ed", &fx);
    state = lit_new_state();
    lit_open_libraries(state);

    if(!parse_options(&opts, fx.flags, fx.fcnt))
    {
        cmdfailed = true;
    }
    else
    {
        if(opts.debugmode != NULL)
        {
            dm = opts.debugmode;
            {
                fprintf(stderr, "unrecognized dump mode '%s'\n", dm);
                cmdfailed = true;
            }
        }
    }
    if(!cmdfailed)
    {
        if((fx.poscnt > 0) || (opts.codeline != NULL))
        {
            arg_array = lit_create_array(state);
            for(i=0; i<fx.poscnt; i++)
            {
                lit_vallist_push(state, &arg_array->list, OBJECT_CONST_STRING(state, fx.positional[i]));
            }
            lit_set_global(state, CONST_STRING(state, "args"), OBJECT_VALUE(arg_array));
            if(opts.codeline)
            {
                result = lit_interpret_source(state, "<-e>", opts.codeline, strlen(opts.codeline)).type;
            }
            else
            {
                filename = fx.positional[0];
                result = lit_interpret_file(state, filename).type;
            }
        }
        else
        {
            #if defined(LIT_HAVE_READLINE)
                run_repl(state);
            #else
                fprintf(stderr, "no repl support compiled in\n");
            #endif
        }
    }
    return exitstate(state, result);
}

int oldmain(int argc, const char* argv[])
{
    int i;
    int ec;
    int args_left;
    int num_files_to_run;
    char c;
    bool found;
    bool dump;
    bool show_repl;
    bool evaled;
    bool showed_help;
    bool enable_optimization;
    size_t j;
    size_t length;
    char* file;
    char* bytecode_file;
    char* source;
    char* optimization_name;
    const char* arg_string;
    const char* string;
    const char* arg;
    const char* module_name;
    char* files_to_run[128];
    LitState* state;
    LitModule* module;
    LitInterpretResultType result;
    LitArray* arg_array;

    ec = 0;
    state = lit_new_state();
    lit_open_libraries(state);
    num_files_to_run = 0;
    result = LITRESULT_OK;
    dump = false;
    for(i = 1; i < argc; i++)
    {
        arg = argv[i];
        if(arg[0] == '-')
        {
            if(match_arg(arg, "-e", "--eval") || match_arg(arg, "-o", "--output"))
            {
                // It takes an extra argument, count it or we will use it as the file name to run :P
                i++;
            }
            else if(match_arg(arg, "-p", "--pass"))
            {
                // The rest of the args go to the script, go home pls
                break;
            }
            else if(match_arg(arg, "-d", "--dump"))
            {
                dump = true;
            }
            continue;
        }
        files_to_run[num_files_to_run++] = (char*)arg;
        fprintf(stderr, "num_files_to_run=%d, (argc+1)=%d\n", num_files_to_run, argc+1);
    }
    arg_array = NULL;
    show_repl = false;
    evaled = false;
    showed_help = false;
    bytecode_file = NULL;
    for(i = 1; i < argc; i++)
    {
        args_left = argc - i - 1;
        arg = argv[i];
        if(arg[0] == '-' && arg[1] == 'D')
        {
            lit_preproc_setdef(state, arg + 2);
        }
        else if(arg[0] == '-' && arg[1] == 'O')
        {
            enable_optimization = true;

            // -Ono-whatever
            if(memcmp((char*)(arg + 2), "no-", 3) == 0)
            {
                enable_optimization = false;
                optimization_name = (char*)(arg + 5);
            }
            else
            {
                optimization_name = (char*)(arg + 2);
            }

            if(strlen(optimization_name) == 1)
            {
                c = optimization_name[0];

                if(c >= '0' && c <= '4')
                {
                    lit_set_optimization_level((LitOptimizationLevel)(c - '0'));
                    continue;
                }
            }

            if(enable_optimization && strcmp(optimization_name, "help") == 0)
            {
                show_optimization_help();
                showed_help = true;
            }
            else if(strcmp(optimization_name, "all") == 0)
            {
                lit_set_all_optimization_enabled(enable_optimization);
            }
            else
            {
                found = false;
                // Yes I know, this is not the fastest way, and what now?
                for(j = 0; j < LITOPTSTATE_TOTAL; j++)
                {
                    if(strcmp(lit_get_optimization_name((LitOptimization)j), optimization_name) == 0)
                    {
                        found = true;
                        lit_set_optimization_enabled((LitOptimization)j, enable_optimization);

                        break;
                    }
                }
                if(!found)
                {
                    fprintf(stderr, "Unknown optimization '%s'. Run with -Ohelp for a list of all optimizations.\n", optimization_name);
                    return 1;
                }
            }
        }
        else if(match_arg(arg, "-e", "--eval"))
        {
            evaled = true;
            if(args_left == 0)
            {
                fprintf(stderr, "Expected code to run for the eval argument.\n");
                return exitstate(state, result);
            }
            string = argv[++i];
            length = strlen(string) + 1;
            source = (char*)malloc(length + 1);
            memcpy(source, string, length);
            module_name = num_files_to_run == 0 ? "repl" : files_to_run[0];
            if(dump)
            {
                module = lit_compile_module(state, CONST_STRING(state, module_name), source, length);
                if(module == NULL)
                {
                    break;
                }
                lit_disassemble_module(state, module, source);
                free(source);
            }
            else
            {
                result = lit_interpret_source(state, module_name, source, length).type;
                free(source);
                if(result != LITRESULT_OK)
                {
                    break;
                }
            }
        }
        else if(match_arg(arg, "-h", "--help"))
        {
            show_help();
            showed_help = true;
        }
        else if(match_arg(arg, "-t", "--time"))
        {
            lit_enable_compilation_time_measurement();
        }
        else if(match_arg(arg, "-i", "--interactive"))
        {
            show_repl = true;
        }
        else if(match_arg(arg, "-d", "--dump"))
        {
            dump = true;
        }
        else if(match_arg(arg, "-o", "--output"))
        {
            if(args_left == 0)
            {
                fprintf(stderr, "Expected file name where to save the bytecode.\n");
                //return LIT_EXIT_CODE_ARGUMENT_ERROR;
                return exitstate(state, result);
            }

            bytecode_file = (char*)argv[++i];
            lit_set_optimization_level(LITOPTLEVEL_EXTREME);
        }
        else if(match_arg(arg, "-p", "--pass"))
        {
            arg_array = lit_create_array(state);

            for(j = 0; j < (size_t)args_left; j++)
            {
                arg_string = argv[i + j + 1];
                lit_vallist_push(state, &arg_array->list, OBJECT_CONST_STRING(state, arg_string));
            }

            lit_set_global(state, CONST_STRING(state, "args"), OBJECT_VALUE(arg_array));
            break;
        }
        else if(arg[0] == '-')
        {
            fprintf(stderr, "Unknown argument '%s', run 'lit --help' for help.\n", arg);
            //return LIT_EXIT_CODE_ARGUMENT_ERROR;
            return exitstate(state, result);
        }
    }

    if(num_files_to_run > 0)
    {
        if(bytecode_file != NULL)
        {
            if(!lit_compile_and_save_files(state, files_to_run, num_files_to_run, bytecode_file))
            {
                result = LITRESULT_COMPILE_ERROR;
            }
        }
        else
        {
            if(arg_array == NULL)
            {
                arg_array = lit_create_array(state);
            }
            lit_set_global(state, CONST_STRING(state, "args"), OBJECT_VALUE(arg_array));
            for(i = 0; i < num_files_to_run; i++)
            {
                file = files_to_run[i];
                result = LITRESULT_OK;
                if(dump)
                {
                    result = lit_dump_file(state, file).type;
                }
                else
                {
                    result = lit_interpret_file(state, file).type;
                }
                if(result != LITRESULT_OK)
                {
                    return exitstate(state, result);
                }
            }
        }
    }
    if(show_repl)
    {
        run_repl(state);
    }
    else if((showed_help == false) && (evaled == false) && (num_files_to_run == 0))
    {
        run_repl(state);
    }
    return exitstate(state, result);
}
