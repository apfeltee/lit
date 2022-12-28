


#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#ifdef LIT_OS_WINDOWS
    #include <windows.h>
    #define stat _stat
#endif
#include "dirwrap.h"
#include "lit.h"
#include "sds.h"

#if defined (S_IFDIR) && !defined (S_ISDIR)
    #define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)	/* directory */
#endif
#if defined (S_IFREG) && !defined (S_ISREG)
    #define	S_ISREG(m)	(((m)&S_IFMT) == S_IFREG)	/* file */
#endif

typedef struct LitFileData LitFileData;
typedef struct LitStdioHandle LitStdioHandle;

struct LitFileData
{
    char* path;
    FILE* handle;
    bool isopen;
};

struct LitStdioHandle
{
    FILE* handle;
    const char* name;
    bool canread;
    bool canwrite;
};

typedef void(*CleanupFunc)(LitState*, LitUserdata*, bool);

static void* LIT_INSERT_DATA(LitVM* vm, LitValue instance, size_t typsz, CleanupFunc cleanup)
{
    LitUserdata* userdata = lit_create_userdata(vm->state, typsz, false);
    userdata->cleanup_fn = cleanup;
    lit_table_set(vm->state, &AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), OBJECT_VALUE(userdata));
    return userdata->data;
}

static void* LIT_EXTRACT_DATA(LitVM* vm, LitValue instance)
{
    LitValue _d;
    if(!lit_table_get(&AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), &_d))
    {
        lit_runtime_error_exiting(vm, "failed to extract userdata");
    }
    return AS_USERDATA(_d)->data;
}




static uint8_t btmp;
static uint16_t stmp;
static uint32_t itmp;
static double dtmp;

char* lit_read_file(const char* path)
{
    FILE* file;
    file = fopen(path, "rb");
    if(file == NULL)
    {
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    char* buffer = (char*)malloc(fileSize + 1);
    size_t bytes_read = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

bool lit_file_exists(const char* path)
{
    struct stat buffer;
    return stat(path, &buffer) == 0 && S_ISREG(buffer.st_mode);
}

bool lit_dir_exists(const char* path)
{
    struct stat buffer;
    return stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode);
}

size_t lit_write_uint8_t(FILE* file, uint8_t byte)
{
    return fwrite(&byte, sizeof(uint8_t), 1, file);
}

size_t lit_write_uint16_t(FILE* file, uint16_t byte)
{
    return fwrite(&byte, sizeof(uint16_t), 1, file);
}

size_t lit_write_uint32_t(FILE* file, uint32_t byte)
{
    return fwrite(&byte, sizeof(uint32_t), 1, file);
}

size_t lit_write_double(FILE* file, double byte)
{
    return fwrite(&byte, sizeof(double), 1, file);
}

size_t lit_write_string(FILE* file, LitString* string)
{
    uint16_t i;
    uint16_t c;
    size_t rt;
    c = lit_string_length(string);
    rt = fwrite(&c, 2, 1, file);
    for(i = 0; i < c; i++)
    {
        lit_write_uint8_t(file, (uint8_t)string->chars[i] ^ LIT_STRING_KEY);
    }
    return (rt + i);
}

uint8_t lit_read_uint8_t(FILE* file)
{
    size_t rt;
    (void)rt;
    rt = fread(&btmp, sizeof(uint8_t), 1, file);
    return btmp;
}

uint16_t lit_read_uint16_t(FILE* file)
{
    size_t rt;
    (void)rt;
    rt = fread(&stmp, sizeof(uint16_t), 1, file);
    return stmp;
}

uint32_t lit_read_uint32_t(FILE* file)
{
    size_t rt;
    (void)rt;
    rt = fread(&itmp, sizeof(uint32_t), 1, file);
    return itmp;
}

double lit_read_double(FILE* file)
{
    size_t rt;
    (void)rt;
    rt = fread(&dtmp, sizeof(double), 1, file);
    return dtmp;
}

LitString* lit_read_string(LitState* state, FILE* file)
{
    size_t rt;
    uint16_t i;
    uint16_t length;
    char* line;
    (void)rt;
    rt = fread(&length, 2, 1, file);
    if(length < 1)
    {
        return NULL;
    }
    line = (char*)malloc(length + 1);
    for(i = 0; i < length; i++)
    {
        line[i] = (char)lit_read_uint8_t(file) ^ LIT_STRING_KEY;
    }
    return lit_string_take(state, line, length, false);
}

void lit_init_emulated_file(LitEmulatedFile* file, const char* source)
{
    file->source = source;
    file->position = 0;
}

uint8_t lit_read_euint8_t(LitEmulatedFile* file)
{
    return (uint8_t)file->source[file->position++];
}

uint16_t lit_read_euint16_t(LitEmulatedFile* file)
{
    return (uint16_t)(lit_read_euint8_t(file) | (lit_read_euint8_t(file) << 8u));
}

uint32_t lit_read_euint32_t(LitEmulatedFile* file)
{
    return (uint32_t)(
        lit_read_euint8_t(file) |
        (lit_read_euint8_t(file) << 8u) |
        (lit_read_euint8_t(file) << 16u) |
        (lit_read_euint8_t(file) << 24u)
    );
}

double lit_read_edouble(LitEmulatedFile* file)
{
    size_t i;
    double result;
    uint8_t values[8];
    for(i = 0; i < 8; i++)
    {
        values[i] = lit_read_euint8_t(file);
    }
    memcpy(&result, values, 8);
    return result;
}

LitString* lit_read_estring(LitState* state, LitEmulatedFile* file)
{
    uint16_t i;
    uint16_t length;
    char* line;
    length = lit_read_euint16_t(file);
    if(length < 1)
    {
        return NULL;
    }
    line = (char*)malloc(length + 1);
    for(i = 0; i < length; i++)
    {
        line[i] = (char)lit_read_euint8_t(file) ^ LIT_STRING_KEY;
    }
    return lit_string_take(state, line, length, false);
}

static void save_chunk(FILE* file, LitChunk* chunk);
static void load_chunk(LitState* state, LitEmulatedFile* file, LitModule* module, LitChunk* chunk);

static void save_function(FILE* file, LitFunction* function)
{
    save_chunk(file, &function->chunk);
    lit_write_string(file, function->name);
    lit_write_uint8_t(file, function->arg_count);
    lit_write_uint16_t(file, function->upvalue_count);
    lit_write_uint8_t(file, (uint8_t)function->vararg);
    lit_write_uint16_t(file, (uint16_t)function->max_slots);
}

static LitFunction* load_function(LitState* state, LitEmulatedFile* file, LitModule* module)
{
    LitFunction* function = lit_create_function(state, module);

    load_chunk(state, file, module, &function->chunk);
    function->name = lit_read_estring(state, file);

    function->arg_count = lit_read_euint8_t(file);
    function->upvalue_count = lit_read_euint16_t(file);
    function->vararg = (bool)lit_read_euint8_t(file);
    function->max_slots = lit_read_euint16_t(file);

    return function;
}

static void save_chunk(FILE* file, LitChunk* chunk)
{
    lit_write_uint32_t(file, chunk->count);

    for(size_t i = 0; i < chunk->count; i++)
    {
        lit_write_uint8_t(file, chunk->code[i]);
    }

    if(chunk->has_line_info)
    {
        size_t c = chunk->line_count * 2 + 2;
        lit_write_uint32_t(file, c);

        for(size_t i = 0; i < c; i++)
        {
            lit_write_uint16_t(file, chunk->lines[i]);
        }
    }
    else
    {
        lit_write_uint32_t(file, 0);
    }

    lit_write_uint32_t(file, lit_vallist_count(&chunk->constants));

    for(size_t i = 0; i < lit_vallist_count(&chunk->constants); i++)
    {
        LitValue constant = lit_vallist_get(&chunk->constants, i);

        if(IS_OBJECT(constant))
        {
            LitObjectType type = lit_as_object(constant)->type;
            lit_write_uint8_t(file, (uint8_t)(type + 1));

            switch(type)
            {
                case LITTYPE_STRING:
                {
                    lit_write_string(file, lit_as_string(constant));
                    break;
                }

                case LITTYPE_FUNCTION:
                {
                    save_function(file, AS_FUNCTION(constant));
                    break;
                }

                default:
                {
                    UNREACHABLE
                    break;
                }
            }
        }
        else
        {
            lit_write_uint8_t(file, 0);
            lit_write_double(file, lit_value_to_number(constant));
        }
    }
}

static void load_chunk(LitState* state, LitEmulatedFile* file, LitModule* module, LitChunk* chunk)
{
    size_t i;
    size_t count;
    uint8_t type;
    lit_init_chunk(chunk);
    count = lit_read_euint32_t(file);
    chunk->code = (uint8_t*)lit_reallocate(state, NULL, 0, sizeof(uint8_t) * count);
    chunk->count = count;
    chunk->capacity = count;
    for(i = 0; i < count; i++)
    {
        chunk->code[i] = lit_read_euint8_t(file);
    }
    count = lit_read_euint32_t(file);
    if(count > 0)
    {
        chunk->lines = (uint16_t*)lit_reallocate(state, NULL, 0, sizeof(uint16_t) * count);
        chunk->line_count = count;
        chunk->line_capacity = count;
        for(i = 0; i < count; i++)
        {
            chunk->lines[i] = lit_read_euint16_t(file);
        }
    }
    else
    {
        chunk->has_line_info = false;
    }
    count = lit_read_euint32_t(file);
    /*
    chunk->constants.values = (LitValue*)lit_reallocate(state, NULL, 0, sizeof(LitValue) * count);
    chunk->constants.count = count;
    chunk->constants.capacity = count;
    */
    lit_vallist_init(&chunk->constants);
    lit_vallist_ensuresize(state, &chunk->constants, count);

    for(i = 0; i < count; i++)
    {
        type = lit_read_euint8_t(file);
        if(type == 0)
        {
            //chunk->constants.values[i] = lit_number_to_value(lit_read_edouble(file));
            lit_vallist_set(&chunk->constants, i, lit_number_to_value(lit_read_edouble(file)));

        }
        else
        {
            switch((LitObjectType)(type - 1))
            {
                case LITTYPE_STRING:
                    {
                        //chunk->constants.values[i] = OBJECT_VALUE(lit_read_estring(state, file));
                        lit_vallist_set(&chunk->constants, i, OBJECT_VALUE(lit_read_estring(state, file)));

                    }
                    break;
                case LITTYPE_FUNCTION:
                    {
                        //chunk->constants.values[i] = OBJECT_VALUE(load_function(state, file, module));
                        lit_vallist_set(&chunk->constants, i, OBJECT_VALUE(load_function(state, file, module)));

                    }
                    break;
                default:
                    {
                        UNREACHABLE
                    }
                    break;

            }
        }
    }
}

void lit_save_module(LitModule* module, FILE* file)
{
    size_t i;
    bool disabled;
    LitTable* privates;
    disabled = lit_is_optimization_enabled(LITOPTSTATE_PRIVATE_NAMES);
    lit_write_string(file, module->name);
    lit_write_uint16_t(file, module->private_count);
    lit_write_uint8_t(file, (uint8_t)disabled);
    if(!disabled)
    {
        privates = &module->private_names->values;
        for(i = 0; i < module->private_count; i++)
        {
            if(privates->entries[i].key != NULL)
            {
                lit_write_string(file, privates->entries[i].key);
                lit_write_uint16_t(file, (uint16_t)lit_value_to_number(privates->entries[i].value));
            }
        }
    }
    save_function(file, module->main_function);
}

LitModule* lit_load_module(LitState* state, const char* input)
{
    bool enabled;
    uint16_t i;
    uint16_t j;
    uint16_t module_count;
    uint16_t privates_count;
    uint8_t bytecode_version;
    LitString* name;
    LitTable* privates;
    LitModule* module;
    LitEmulatedFile file;
    lit_init_emulated_file(&file, input);
    if(lit_read_euint16_t(&file) != LIT_BYTECODE_MAGIC_NUMBER)
    {
        lit_error(state, COMPILE_ERROR, "Failed to read compiled code, unknown magic number");
        return NULL;
    }
    bytecode_version = lit_read_euint8_t(&file);
    if(bytecode_version > LIT_BYTECODE_VERSION)
    {
        lit_error(state, COMPILE_ERROR, "Failed to read compiled code, unknown bytecode version '%i'", (int)bytecode_version);
        return NULL;
    }
    module_count = lit_read_euint16_t(&file);
    LitModule* first = NULL;
    for(j = 0; j < module_count; j++)
    {
        module = lit_create_module(state, lit_read_estring(state, &file));
        privates = &module->private_names->values;
        privates_count = lit_read_euint16_t(&file);
        enabled = !((bool)lit_read_euint8_t(&file));
        module->privates = LIT_ALLOCATE(state, sizeof(LitValue), privates_count);
        module->private_count = privates_count;
        for(i = 0; i < privates_count; i++)
        {
            module->privates[i] = NULL_VALUE;
            if(enabled)
            {
                name = lit_read_estring(state, &file);
                lit_table_set(state, privates, name, lit_number_to_value(lit_read_euint16_t(&file)));
            }
        }
        module->main_function = load_function(state, &file, module);
        lit_table_set(state, &state->vm->modules->values, module->name, OBJECT_VALUE(module));
        if(j == 0)
        {
            first = module;
        }
    }
    if(lit_read_euint16_t(&file) != LIT_BYTECODE_END_NUMBER)
    {
        lit_error(state, COMPILE_ERROR, "Failed to read compiled code, unknown end number");
        return NULL;
    }
    return first;
}



/*
 * File
 */
void cleanup_file(LitState* state, LitUserdata* data, bool mark)
{
    (void)state;
    LitFileData* fd;
    if(mark)
    {
        return;
    }
    if(data != NULL)
    {
        fd = ((LitFileData*)data->data);
        if(fd != NULL)
        {
            if((fd->handle != NULL) && (fd->isopen == true))
            {
                fclose(fd->handle);
                fd->handle = NULL;
                fd->isopen = false;
            }
        }
    }
}

static LitValue objmethod_file_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    const char* path;
    const char* mode;
    FILE* hnd;
    LitFileData* data;
    LitStdioHandle* hstd;    
    if(argc > 1)
    {
        if(IS_USERDATA(argv[0]))
        {
            hstd = (LitStdioHandle*)(AS_USERDATA(argv[0])->data);
            hnd = hstd->handle;
            fprintf(stderr, "FILE: hnd=%p name=%s\n", hstd->handle, hstd->name);
            data = (LitFileData*)LIT_INSERT_DATA(vm, instance, sizeof(LitFileData), NULL);
            data->path = NULL;
            data->handle = hnd;
            data->isopen = true;
        }
        else
        {
            path = lit_check_string(vm, argv, argc, 0);
            mode = lit_get_string(vm, argv, argc, 1, "r");
            hnd = fopen(path, mode);
            if(hnd == NULL)
            {
                lit_runtime_error_exiting(vm, "Failed to open file %s with mode %s (C error: %s)", path, mode, strerror(errno));
            }
            data = (LitFileData*)LIT_INSERT_DATA(vm, instance, sizeof(LitFileData), cleanup_file);
            data->path = (char*)path;
            data->handle = hnd;
            data->isopen = true;
        }
    }
    else
    {
        lit_runtime_error_exiting(vm, "File() expects either string|string, or userdata|string");
        return NULL_VALUE;
    }
    return instance;
}


static LitValue objmethod_file_close(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LitFileData* data;
    data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    fclose(data->handle);
    data->handle = NULL;
    data->isopen = false;
    return NULL_VALUE;
}

static LitValue objmethod_file_exists(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    char* file_name;
    file_name = NULL;
    if(IS_INSTANCE(instance))
    {
        file_name = ((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->path;
    }
    else
    {
        file_name = (char*)lit_check_string(vm, argv, argc, 0);
    }
    return BOOL_VALUE(lit_file_exists(file_name));
}

/*
 * ==
 * File writing
 */

static LitValue objmethod_file_write(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)
    size_t rt;
    LitString* value;
    value = lit_to_string(vm->state, argv[0]);
    rt = fwrite(value->chars, lit_string_length(value), 1, ((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle);
    return lit_number_to_value(rt);
}

static LitValue objmethod_file_writebyte(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint8_t rt;
    uint8_t byte;
    byte = (uint8_t)lit_check_number(vm, argv, argc, 0);
    rt = lit_write_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, byte);
    return lit_number_to_value(rt);
}

static LitValue objmethod_file_writeshort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint16_t rt;
    uint16_t shrt;
    shrt = (uint16_t)lit_check_number(vm, argv, argc, 0);
    rt = lit_write_uint16_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, shrt);
    return lit_number_to_value(rt);
}

static LitValue objmethod_file_writenumber(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint32_t rt;
    float num;
    num = (float)lit_check_number(vm, argv, argc, 0);
    rt = lit_write_uint32_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, num);
    return lit_number_to_value(rt);
}

static LitValue objmethod_file_writebool(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    bool value;
    uint8_t rt;
    value = lit_check_bool(vm, argv, argc, 0);
    rt = lit_write_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, (uint8_t)value ? '1' : '0');
    return lit_number_to_value(rt);
}

static LitValue objmethod_file_writestring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* string;
    LitFileData* data;
    if(lit_check_string(vm, argv, argc, 0) == NULL)
    {
        return NULL_VALUE;
    }
    string = lit_as_string(argv[0]);
    data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    lit_write_string(data->handle, string);
    return NULL_VALUE;
}

/*
 * ==
 * File reading
 */

static LitValue objmethod_file_readall(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    char c;
    long length;
    long actuallength;
    LitFileData* data;
    LitString* result;
    data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    if(fseek(data->handle, 0, SEEK_END) == -1)
    {
        /*
        * cannot seek, must read each byte.
        */
        result = lit_string_alloc_empty(vm->state, 0, false);
        actuallength = 0;
        while((c = fgetc(data->handle)) != EOF)
        {
            result->chars = sdscatlen(result->chars, &c, 1);
            actuallength++;
        }
    }
    else
    {
        length = ftell(data->handle);
        fseek(data->handle, 0, SEEK_SET);
        result = lit_string_alloc_empty(vm->state, length, false);
        actuallength = fread(result->chars, sizeof(char), length, data->handle);
        /*
        * after reading, THIS actually sets the correct length.
        * before that, it would be 0.
        */
        sdsIncrLen(result->chars, actuallength);
    }
    result->hash = lit_hash_string(result->chars, actuallength);
    lit_register_string(vm->state, result);
    return OBJECT_VALUE(result);
}

static LitValue objmethod_file_readline(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    size_t max_length;
    char* line;
    LitFileData* data;
    max_length = (size_t)lit_get_number(vm, argv, argc, 0, 128);
    data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    line = LIT_ALLOCATE(vm->state, sizeof(char), max_length + 1);
    if(!fgets(line, max_length, data->handle))
    {
        LIT_FREE(vm->state, sizeof(char), line);
        return NULL_VALUE;
    }
    return OBJECT_VALUE(lit_string_take(vm->state, line, strlen(line) - 1, false));
}

static LitValue objmethod_file_readbyte(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue objmethod_file_readshort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint16_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue objmethod_file_readnumber(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint32_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue objmethod_file_readbool(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return BOOL_VALUE((char)lit_read_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle) == '1');
}

static LitValue objmethod_file_readstring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    LitFileData* data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    LitString* string = lit_read_string(vm->state, data->handle);

    return string == NULL ? NULL_VALUE : OBJECT_VALUE(string);
}

static LitValue objmethod_file_getlastmodified(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    struct stat buffer;
    char* file_name = NULL;
    (void)vm;
    (void)argc;
    (void)argv;
    if(IS_INSTANCE(instance))
    {
        file_name = ((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->path;
    }
    else
    {
        file_name = (char*)lit_check_string(vm, argv, argc, 0);
    }

    if(stat(file_name, &buffer) != 0)
    {
        return lit_number_to_value(0);
    }
    #if defined(__unix__) || defined(__linux__)
    return lit_number_to_value(buffer.st_mtim.tv_sec);
    #else
        return lit_number_to_value(0);
    #endif
}


/*
* Directory
*/

static LitValue objfunction_directory_exists(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    const char* directory_name = lit_check_string(vm, argv, argc, 0);
    struct stat buffer;
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return BOOL_VALUE(stat(directory_name, &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

static LitValue objfunction_directory_listfiles(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitState* state;
    LitArray* array;
    (void)instance;
    state = vm->state;
    array = lit_create_array(state);
    #if defined(__unix__) || defined(__linux__)
    {
        struct dirent* ep;
        DIR* dir = opendir(lit_check_string(vm, argv, argc, 0));
        if(dir == NULL)
        {
            return OBJECT_VALUE(array);
        }
        while((ep = readdir(dir)))
        {
            if(ep->d_type == DT_REG)
            {
                lit_vallist_push(state, &array->list, OBJECT_CONST_STRING(state, ep->d_name));
            }
        }
        closedir(dir);
    }
    #endif
    return OBJECT_VALUE(array);
}

static LitValue objfunction_directory_listdirs(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitArray* array;
    LitState* state;
    LitDirReader rd;
    LitDirItem ent;
    (void)instance;
    state = vm->state;
    array = lit_create_array(state);

    if(lit_dir_open(&rd, lit_check_string(vm, argv, argc, 0)))
    {
        while(true)
        {
            if(lit_dir_read(&rd, &ent))
            {
                if(ent.isdir && ((strcmp(ent.name, ".") != 0) || (strcmp(ent.name, "..") != 0)))
                {
                    lit_vallist_push(state, &array->list, OBJECT_CONST_STRING(state, ent.name));
                }
            }
            else
            {
                break;
            }
        }
        lit_dir_close(&rd);
    }
    return OBJECT_VALUE(array);
}

static void free_handle(LitState* state, LitUserdata* userdata, bool mark)
{
    LitStdioHandle* hstd;
    (void)mark;
    hstd = (LitStdioHandle*)(userdata->data);
    LIT_FREE(state, sizeof(LitStdioHandle), hstd);
}

static void make_handle(LitState* state, LitValue fileval, const char* name, FILE* hnd, bool canread, bool canwrite)
{
    LitUserdata* userhnd;
    LitValue args[5];
    LitString* varname;
    LitString* descname;
    LitInterpretResult res;
    LitFiber* oldfiber;
    LitStdioHandle* hstd;
    oldfiber = state->vm->fiber;
    state->vm->fiber = lit_create_fiber(state, state->last_module, NULL);
    {
        hstd = LIT_ALLOCATE(state, sizeof(LitStdioHandle), 1);
        hstd->handle = hnd;
        hstd->name = name;
        hstd->canread = canread;
        hstd->canwrite = canwrite; 
        userhnd = lit_create_userdata(state, sizeof(LitStdioHandle), true);
        userhnd->data = hstd;
        userhnd->canfree = false;
        userhnd->cleanup_fn = free_handle;
        varname = CONST_STRING(state, name);
        descname = CONST_STRING(state, name);
        args[0] = OBJECT_VALUE(userhnd);
        args[1] = OBJECT_VALUE(descname);
        res = lit_call(state, fileval, args, 2, false);
        //fprintf(stderr, "make_handle(%s, hnd=%p): res.type=%d, res.result=%s\n", name, hnd, res.type, lit_get_value_type(res.result));
        lit_set_global(state, varname, res.result);
    }
    state->vm->fiber = oldfiber;
}

static void make_stdhandles(LitState* state)
{
    LitValue fileval;
    fileval = lit_get_global(state, CONST_STRING(state, "File"));
    fprintf(stderr, "fileval=%s\n", lit_get_value_type(fileval));
    {
        make_handle(state, fileval, "STDIN", stdin, true, false);
        make_handle(state, fileval, "STDOUT", stdout, false, true);
        make_handle(state, fileval, "STDERR", stderr, false, true);
    }
}

void lit_open_file_library(LitState* state)
{
    {
        LIT_BEGIN_CLASS("File");
        {
            LIT_BIND_STATIC_METHOD("exists", objmethod_file_exists);
            LIT_BIND_STATIC_METHOD("getLastModified", objmethod_file_getlastmodified);
            LIT_BIND_CONSTRUCTOR(objmethod_file_constructor);
            LIT_BIND_METHOD("close", objmethod_file_close);
            LIT_BIND_METHOD("write", objmethod_file_write);
            LIT_BIND_METHOD("writeByte", objmethod_file_writebyte);
            LIT_BIND_METHOD("writeShort", objmethod_file_writeshort);
            LIT_BIND_METHOD("writeNumber", objmethod_file_writenumber);
            LIT_BIND_METHOD("writeBool", objmethod_file_writebool);
            LIT_BIND_METHOD("writeString", objmethod_file_writestring);
            LIT_BIND_METHOD("readAll", objmethod_file_readall);
            LIT_BIND_METHOD("readLine", objmethod_file_readline);
            LIT_BIND_METHOD("readByte", objmethod_file_readbyte);
            LIT_BIND_METHOD("readShort", objmethod_file_readshort);
            LIT_BIND_METHOD("readNumber", objmethod_file_readnumber);
            LIT_BIND_METHOD("readBool", objmethod_file_readbool);
            LIT_BIND_METHOD("readString", objmethod_file_readstring);
            LIT_BIND_METHOD("getLastModified", objmethod_file_getlastmodified);
            LIT_BIND_GETTER("exists", objmethod_file_exists);
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Directory");
        {
            LIT_BIND_STATIC_METHOD("exists", objfunction_directory_exists);
            LIT_BIND_STATIC_METHOD("listFiles", objfunction_directory_listfiles);
            LIT_BIND_STATIC_METHOD("listDirectories", objfunction_directory_listdirs);
        }
        LIT_END_CLASS();
    }
    make_stdhandles(state);
}

