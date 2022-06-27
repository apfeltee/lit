
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

// Used for clean up on Ctrl+C / Ctrl+Z
static LitState* repl_state;

void interupt_handler(int signal_id)
{
    (void)signal_id;
    lit_free_state(repl_state);
    printf("\nExiting.\n");

    exit(0);
}

static int run_repl(LitState* state)
{
    fprintf(stderr, "in repl...\n");
    #if defined(LIT_HAVE_READLINE)
    char* line;
    repl_state = state;
    signal(SIGINT, interupt_handler);
    //signal(SIGTSTP, interupt_handler);
    lit_set_optimization_level(LITOPTLEVEL_REPL);
    printf("lit v%s, developed by @egordorichev\n", LIT_VERSION_STRING);
    while(true)
    {
        printf("%s>%s ", COLOR_BLUE, COLOR_RESET);
        line = readline("");
        if(line == NULL)
        {
            return 0;
        }
        add_history(line);
        LitInterpretResult result = lit_interpret(state, "repl", line);
        if(result.type == LITRESULT_OK && result.result != NULL_VALUE)
        {
            printf("%s%s%s\n", COLOR_GREEN, lit_to_string(state, result.result)->chars, COLOR_RESET);
        }
    }
    #endif
    return 0;
}

static void run_tests(LitState* state)
{
    #if defined(__unix__) || defined(__linux__)
    DIR* dir = opendir(LIT_TESTS_DIRECTORY);

    if(dir == NULL)
    {
        fprintf(stderr, "Could not find '%s' directory\n", LIT_TESTS_DIRECTORY);
        return;
    }

    struct dirent* ep;
    struct dirent* node;

    size_t tests_dir_length = strlen(LIT_TESTS_DIRECTORY);

    while((ep = readdir(dir)))
    {
        if(ep->d_type == DT_DIR)
        {
            const char* dir_name = ep->d_name;

            if(strcmp(dir_name, "..") == 0 || strcmp(dir_name, ".") == 0)
            {
                continue;
            }

            size_t dir_name_length = strlen(dir_name);
            size_t total_length = dir_name_length + tests_dir_length + 2;

            char subdir_name[total_length];

            memcpy(subdir_name, LIT_TESTS_DIRECTORY, tests_dir_length);
            memcpy(subdir_name + tests_dir_length + 1, dir_name, dir_name_length);

            subdir_name[tests_dir_length] = '/';
            subdir_name[total_length - 1] = '\0';

            DIR* subdir = opendir(subdir_name);

            if(subdir == NULL)
            {
                fprintf(stderr, "Failed to open tests subdirectory '%s'\n", subdir_name);
                continue;
            }

            while((node = readdir(subdir)))
            {
                if(node->d_type == DT_REG)
                {
                    const char* file_name = node->d_name;
                    size_t name_length = strlen(file_name);

                    if(name_length < 4 || memcmp(".lit", file_name + name_length - 4, 4) != 0)
                    {
                        continue;
                    }

                    char file_path[total_length + name_length + 1];

                    memcpy(file_path, subdir_name, total_length - 1);
                    memcpy(file_path + total_length, file_name, name_length);

                    file_path[total_length - 1] = '/';
                    file_path[total_length + name_length] = '\0';

                    printf("Testing %s...\n", file_path);
                    lit_interpret_file(state, file_path);
                }
            }

            closedir(subdir);
        }
    }

    closedir(dir);
    #endif
}

static void show_help()
{
    printf("lit [options] [files]\n");
    printf("    -o --output [file]  Instead of running the file the compiled bytecode will be saved.\n");
    printf(" -n --native [file] Instead of running the file the compiled code will be embeded into a native runner.\n");
    printf(" -O[name] [string] Enables given optimization. For the list of aviable optimizations run with -Ohelp\n");
    printf(" -D[name]  Defines given symbol.\n");
    printf(" -e --eval [string] Runs the given code string.\n");
    printf(" -p --pass [args] Passes the rest of the arguments to the script.\n");
    printf(" -i --interactive Starts an interactive shell.\n");
    printf(" -d --dump  Dumps all the bytecode chunks from the given file.\n");
    printf(" -t --time  Measures and prints the compilation timings.\n");
    printf(" -c --test  Runs all tests (useful for code coverage testing).\n");
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

int main(int argc, const char* argv[])
{
    int i;
    int args_left;
    int num_files_to_run;
    bool dump;
    bool show_repl;
    bool evaled;
    bool showed_help;
    bool create_native;
    bool perform_tests;
    char* bytecode_file;
    const char* arg;
    char* files_to_run[argc - 1];
    LitInterpretResultType result;
    LitArray* arg_array;
    LitState* state;
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
            if(match_arg(arg, "-e", "--eval") || match_arg(arg, "-o", "--output") || match_arg(arg, "-n", "--native"))
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
    }
    arg_array = NULL;
    show_repl = false;
    evaled = false;
    showed_help = false;
    create_native = false;
    perform_tests = false;
    bytecode_file = NULL;
    for(i = 1; i < argc; i++)
    {
        args_left = argc - i - 1;
        arg = argv[i];
        if(arg[0] == '-' && arg[1] == 'D')
        {
            lit_add_definition(state, arg + 2);
        }
        else if(arg[0] == '-' && arg[1] == 'O')
        {
            bool enable_optimization = true;
            char* optimization_name;

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
                char c = optimization_name[0];

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
                bool found = false;
                // Yes I know, this is not the fastest way, and what now?
                for(size_t j = 0; j < LITOPTSTATE_TOTAL; j++)
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
                    return LIT_EXIT_CODE_ARGUMENT_ERROR;
                }
            }
        }
        else if(match_arg(arg, "-e", "--eval"))
        {
            evaled = true;
            if(args_left == 0)
            {
                fprintf(stderr, "Expected code to run for the eval argument.\n");
                return LIT_EXIT_CODE_ARGUMENT_ERROR;
            }
            const char* string = argv[++i];
            size_t length = strlen(string) + 1;
            char source[length];
            memcpy(source, string, length);
            const char* module_name = num_files_to_run == 0 ? "repl" : files_to_run[0];
            if(dump)
            {
                LitModule* module = lit_compile_module(state, CONST_STRING(state, module_name), source);

                if(module == NULL)
                {
                    break;
                }

                lit_disassemble_module(module, source);
            }
            else
            {
                result = lit_interpret(state, module_name, source).type;
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
        else if(match_arg(arg, "-c", "--test"))
        {
            perform_tests = true;
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
                return LIT_EXIT_CODE_ARGUMENT_ERROR;
            }

            bytecode_file = (char*)argv[++i];
            lit_set_optimization_level(LITOPTLEVEL_EXTREME);
        }
        else if(match_arg(arg, "-n", "--native"))
        {
            if(args_left == 0)
            {
                fprintf(stderr, "Expected file name where to save the native.\n");
                return LIT_EXIT_CODE_ARGUMENT_ERROR;
            }

            bytecode_file = (char*)argv[++i];
            create_native = true;

            lit_set_optimization_level(LITOPTLEVEL_EXTREME);
        }
        else if(match_arg(arg, "-p", "--pass"))
        {
            arg_array = lit_create_array(state);

            for(int j = 0; j < args_left; j++)
            {
                const char* arg_string = argv[i + j + 1];
                lit_values_write(state, &arg_array->values, OBJECT_CONST_STRING(state, arg_string));
            }

            lit_set_global(state, CONST_STRING(state, "args"), OBJECT_VALUE(arg_array));
            break;
        }
        else if(arg[0] == '-')
        {
            fprintf(stderr, "Unknown argument '%s', run 'lit --help' for help.\n", arg);
            return LIT_EXIT_CODE_ARGUMENT_ERROR;
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

            if(create_native)
            {
                lit_build_native_runner(bytecode_file);
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
                char* file = files_to_run[i];
                result = (dump ? lit_dump_file(state, file) : lit_interpret_file(state, file)).type;
                if(result != LITRESULT_OK)
                {
                    return 0;
                }
            }
        }
    }
    if(perform_tests)
    {
        run_tests(state);
        return 0;
    }
    if(show_repl)
    {
        run_repl(state);
    }
    else if((showed_help == false) && (evaled == false) && (num_files_to_run == 0))
    {
        run_repl(state);
    }

    int64_t amount = lit_free_state(state);

    if(result != LITRESULT_COMPILE_ERROR && amount != 0)
    {
        fprintf(stderr, "Error: memory leak of %i bytes!\n", (int)amount);
        return LIT_EXIT_CODE_MEM_LEAK;
    }

    if(result != LITRESULT_OK)
    {
        return result == LITRESULT_RUNTIME_ERROR ? LIT_EXIT_CODE_RUNTIME_ERROR : LIT_EXIT_CODE_COMPILE_ERROR;
    }

    return 0;
}