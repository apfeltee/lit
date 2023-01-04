
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
    lit_table_set(vm->state, &lit_value_asinstance(instance)->fields, CONST_STRING(vm->state, "_data"), lit_value_objectvalue(userdata));
    return userdata->data;
}

static void* LIT_EXTRACT_DATA(LitVM* vm, LitValue instance)
{
    LitValue _d;
    if(!lit_table_get(&lit_value_asinstance(instance)->fields, CONST_STRING(vm->state, "_data"), &_d))
    {
        lit_vm_raiseexitingerror(vm, "failed to extract userdata");
    }
    return lit_value_asuserdata(_d)->data;
}




static uint8_t btmp;
static uint16_t stmp;
static uint32_t itmp;
static double dtmp;

char* lit_util_readfile(const char* path, size_t* dlen)
{
    size_t fileSize;
    char* buffer;
    size_t bytes_read;
    FILE* file;
    file = fopen(path, "rb");
    if(file == NULL)
    {
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    fileSize = ftell(file);
    rewind(file);
    buffer = (char*)malloc(fileSize + 1);
    bytes_read = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    *dlen = bytes_read;
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
    c = lit_string_getlength(string);
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

void lit_init_emulated_file(LitEmulatedFile* file, const char* source, size_t len)
{
    file->source = source;
    file->length = len;
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

        if(lit_value_isobject(constant))
        {
            LitObjType type = lit_value_asobject(constant)->type;
            lit_write_uint8_t(file, (uint8_t)(type + 1));

            switch(type)
            {
                case LITTYPE_STRING:
                {
                    lit_write_string(file, lit_value_asstring(constant));
                    break;
                }

                case LITTYPE_FUNCTION:
                {
                    save_function(file, lit_value_asfunction(constant));
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
            lit_write_double(file, lit_value_asnumber(constant));
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
    chunk->code = (uint8_t*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(uint8_t) * count);
    chunk->count = count;
    chunk->capacity = count;
    for(i = 0; i < count; i++)
    {
        chunk->code[i] = lit_read_euint8_t(file);
    }
    count = lit_read_euint32_t(file);
    if(count > 0)
    {
        chunk->lines = (uint16_t*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(uint16_t) * count);
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
    chunk->constants.values = (LitValue*)lit_gcmem_memrealloc(state, NULL, 0, sizeof(LitValue) * count);
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
            //chunk->constants.values[i] = lit_value_numbertovalue(vm->state, lit_read_edouble(file));
            lit_vallist_set(&chunk->constants, i, lit_value_numbertovalue(state, lit_read_edouble(file)));

        }
        else
        {
            switch((LitObjType)(type - 1))
            {
                case LITTYPE_STRING:
                    {
                        //chunk->constants.values[i] = lit_value_objectvalue(lit_read_estring(state, file));
                        lit_vallist_set(&chunk->constants, i, lit_value_objectvalue(lit_read_estring(state, file)));

                    }
                    break;
                case LITTYPE_FUNCTION:
                    {
                        //chunk->constants.values[i] = lit_value_objectvalue(load_function(state, file, module));
                        lit_vallist_set(&chunk->constants, i, lit_value_objectvalue(load_function(state, file, module)));

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
    disabled = lit_astopt_isoptenabled(LITOPTSTATE_PRIVATE_NAMES);
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
                lit_write_uint16_t(file, (uint16_t)lit_value_asnumber(privates->entries[i].value));
            }
        }
    }
    save_function(file, module->main_function);
}

LitModule* lit_load_module(LitState* state, const char* input, size_t len)
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
    lit_init_emulated_file(&file, input, len);
    if(lit_read_euint16_t(&file) != LIT_BYTECODE_MAGIC_NUMBER)
    {
        lit_state_raiseerror(state, COMPILE_ERROR, "Failed to read compiled code, unknown magic number");
        return NULL;
    }
    bytecode_version = lit_read_euint8_t(&file);
    if(bytecode_version > LIT_BYTECODE_VERSION)
    {
        lit_state_raiseerror(state, COMPILE_ERROR, "Failed to read compiled code, unknown bytecode version '%i'", (int)bytecode_version);
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
                lit_table_set(state, privates, name, lit_value_numbertovalue(state, lit_read_euint16_t(&file)));
            }
        }
        module->main_function = load_function(state, &file, module);
        lit_table_set(state, &state->vm->modules->values, module->name, lit_value_objectvalue(module));
        if(j == 0)
        {
            first = module;
        }
    }
    if(lit_read_euint16_t(&file) != LIT_BYTECODE_END_NUMBER)
    {
        lit_state_raiseerror(state, COMPILE_ERROR, "Failed to read compiled code, unknown end number");
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
        if(lit_value_isuserdata(argv[0]))
        {
            hstd = (LitStdioHandle*)(lit_value_asuserdata(argv[0])->data);
            hnd = hstd->handle;
            fprintf(stderr, "FILE: hnd=%p name=%s\n", hstd->handle, hstd->name);
            data = (LitFileData*)LIT_INSERT_DATA(vm, instance, sizeof(LitFileData), NULL);
            data->path = NULL;
            data->handle = hnd;
            data->isopen = true;
        }
        else
        {
            path = lit_value_checkstring(vm, argv, argc, 0);
            mode = lit_value_getstring(vm, argv, argc, 1, "r");
            hnd = fopen(path, mode);
            if(hnd == NULL)
            {
                lit_vm_raiseexitingerror(vm, "Failed to open file %s with mode %s (C lit_emitter_raiseerror: %s)", path, mode, strerror(errno));
            }
            data = (LitFileData*)LIT_INSERT_DATA(vm, instance, sizeof(LitFileData), cleanup_file);
            data->path = (char*)path;
            data->handle = hnd;
            data->isopen = true;
        }
    }
    else
    {
        lit_vm_raiseexitingerror(vm, "File() expects either string|string, or userdata|string");
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
    if(lit_value_isinstance(instance))
    {
        file_name = ((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->path;
    }
    else
    {
        file_name = (char*)lit_value_checkstring(vm, argv, argc, 0);
    }
    return lit_bool_to_value(vm->state, lit_file_exists(file_name));
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
    value = lit_value_tostring(vm->state, argv[0]);
    rt = fwrite(value->chars, lit_string_getlength(value), 1, ((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle);
    return lit_value_numbertovalue(vm->state, rt);
}

static LitValue objmethod_file_writebyte(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint8_t rt;
    uint8_t byte;
    byte = (uint8_t)lit_value_checknumber(vm, argv, argc, 0);
    rt = lit_write_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, byte);
    return lit_value_numbertovalue(vm->state, rt);
}

static LitValue objmethod_file_writeshort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint16_t rt;
    uint16_t shrt;
    shrt = (uint16_t)lit_value_checknumber(vm, argv, argc, 0);
    rt = lit_write_uint16_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, shrt);
    return lit_value_numbertovalue(vm->state, rt);
}

static LitValue objmethod_file_writenumber(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint32_t rt;
    float num;
    num = (float)lit_value_checknumber(vm, argv, argc, 0);
    rt = lit_write_uint32_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, num);
    return lit_value_numbertovalue(vm->state, rt);
}

static LitValue objmethod_file_writebool(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    bool value;
    uint8_t rt;
    value = lit_value_checkbool(vm, argv, argc, 0);
    rt = lit_write_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, (uint8_t)value ? '1' : '0');
    return lit_value_numbertovalue(vm->state, rt);
}

static LitValue objmethod_file_writestring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* string;
    LitFileData* data;
    if(lit_value_checkstring(vm, argv, argc, 0) == NULL)
    {
        return NULL_VALUE;
    }
    string = lit_value_asstring(argv[0]);
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
        result = lit_string_makeempty(vm->state, 0, false);
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
        result = lit_string_makeempty(vm->state, length, false);
        actuallength = fread(result->chars, sizeof(char), length, data->handle);
        /*
        * after reading, THIS actually sets the correct length.
        * before that, it would be 0.
        */
        sdsIncrLen(result->chars, actuallength);
    }
    result->hash = lit_util_hashstring(result->chars, actuallength);
    lit_state_regstring(vm->state, result);
    return lit_value_objectvalue(result);
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
    max_length = (size_t)lit_value_getnumber(vm, argv, argc, 0, 128);
    data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    line = LIT_ALLOCATE(vm->state, sizeof(char), max_length + 1);
    if(!fgets(line, max_length, data->handle))
    {
        LIT_FREE(vm->state, sizeof(char), line);
        return NULL_VALUE;
    }
    return lit_value_objectvalue(lit_string_take(vm->state, line, strlen(line) - 1, false));
}

static LitValue objmethod_file_readbyte(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_numbertovalue(vm->state, lit_read_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue objmethod_file_readshort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_numbertovalue(vm->state, lit_read_uint16_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue objmethod_file_readnumber(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_numbertovalue(vm->state, lit_read_uint32_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue objmethod_file_readbool(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_bool_to_value(vm->state, (char)lit_read_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle) == '1');
}

static LitValue objmethod_file_readstring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    LitFileData* data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    LitString* string = lit_read_string(vm->state, data->handle);

    return string == NULL ? NULL_VALUE : lit_value_objectvalue(string);
}

static LitValue objmethod_file_getlastmodified(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    struct stat buffer;
    char* file_name = NULL;
    (void)vm;
    (void)argc;
    (void)argv;
    if(lit_value_isinstance(instance))
    {
        file_name = ((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->path;
    }
    else
    {
        file_name = (char*)lit_value_checkstring(vm, argv, argc, 0);
    }

    if(stat(file_name, &buffer) != 0)
    {
        return lit_value_numbertovalue(vm->state, 0);
    }
    #if defined(__unix__) || defined(__linux__)
        return lit_value_numbertovalue(vm->state, buffer.st_mtim.tv_sec);
    #else
        return lit_value_numbertovalue(vm->state, 0);
    #endif
}


/*
* Directory
*/

static LitValue objfunction_directory_exists(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    const char* directory_name = lit_value_checkstring(vm, argv, argc, 0);
    struct stat buffer;
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_bool_to_value(vm->state, stat(directory_name, &buffer) == 0 && S_ISDIR(buffer.st_mode));
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
        DIR* dir = opendir(lit_value_checkstring(vm, argv, argc, 0));
        if(dir == NULL)
        {
            return lit_value_objectvalue(array);
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
    return lit_value_objectvalue(array);
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

    if(lit_dir_open(&rd, lit_value_checkstring(vm, argv, argc, 0)))
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
    return lit_value_objectvalue(array);
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
        args[0] = lit_value_objectvalue(userhnd);
        args[1] = lit_value_objectvalue(descname);
        res = lit_state_callvalue(state, fileval, args, 2, false);
        //fprintf(stderr, "make_handle(%s, hnd=%p): res.type=%d, res.result=%s\n", name, hnd, res.type, lit_tostring_typename(res.result));
        lit_state_setglobal(state, varname, res.result);
    }
    state->vm->fiber = oldfiber;
}

static void make_stdhandles(LitState* state)
{
    LitValue fileval;
    fileval = lit_state_getglobalvalue(state, CONST_STRING(state, "File"));
    fprintf(stderr, "fileval=%s\n", lit_tostring_typename(fileval));
    {
        make_handle(state, fileval, "STDIN", stdin, true, false);
        make_handle(state, fileval, "STDOUT", stdout, false, true);
        make_handle(state, fileval, "STDERR", stderr, false, true);
    }
}

void lit_open_file_library(LitState* state)
{
    LitClass* klass;
    {
        klass = lit_create_classobject(state, "File");
        {
            lit_class_bindstaticmethod(state, klass, "exists", objmethod_file_exists);
            lit_class_bindstaticmethod(state, klass, "getLastModified", objmethod_file_getlastmodified);
            lit_class_bindconstructor(state, klass, objmethod_file_constructor);
            lit_class_bindmethod(state, klass, "close", objmethod_file_close);
            lit_class_bindmethod(state, klass, "write", objmethod_file_write);
            lit_class_bindmethod(state, klass, "writeByte", objmethod_file_writebyte);
            lit_class_bindmethod(state, klass, "writeShort", objmethod_file_writeshort);
            lit_class_bindmethod(state, klass, "writeNumber", objmethod_file_writenumber);
            lit_class_bindmethod(state, klass, "writeBool", objmethod_file_writebool);
            lit_class_bindmethod(state, klass, "writeString", objmethod_file_writestring);
            lit_class_bindmethod(state, klass, "readAll", objmethod_file_readall);
            lit_class_bindmethod(state, klass, "readLine", objmethod_file_readline);
            lit_class_bindmethod(state, klass, "readByte", objmethod_file_readbyte);
            lit_class_bindmethod(state, klass, "readShort", objmethod_file_readshort);
            lit_class_bindmethod(state, klass, "readNumber", objmethod_file_readnumber);
            lit_class_bindmethod(state, klass, "readBool", objmethod_file_readbool);
            lit_class_bindmethod(state, klass, "readString", objmethod_file_readstring);
            lit_class_bindmethod(state, klass, "getLastModified", objmethod_file_getlastmodified);
            lit_class_bindgetset(state, klass, "exists", objmethod_file_exists, NULL, false);
        }
        lit_state_setglobal(state, klass->name, lit_value_objectvalue(klass));
        if(klass->super == NULL)
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
        };
    }
    {
        klass = lit_create_classobject(state, "Directory");
        {
            lit_class_bindstaticmethod(state, klass, "exists", objfunction_directory_exists);
            lit_class_bindstaticmethod(state, klass, "listFiles", objfunction_directory_listfiles);
            lit_class_bindstaticmethod(state, klass, "listDirectories", objfunction_directory_listdirs);
        }
        lit_state_setglobal(state, klass->name, lit_value_objectvalue(klass));
        if(klass->super == NULL)
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
        };
    }
    make_stdhandles(state);
}


