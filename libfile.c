

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__unix__) || defined(__linux__)
    
    #include <dirent.h>
#endif
#ifdef LIT_OS_WINDOWS
    #include <windows.h>
    #define stat _stat
#endif
#include "lit.h"

#if defined (S_IFDIR) && !defined (S_ISDIR)
    #define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)	/* directory */
#endif
#if defined (S_IFREG) && !defined (S_ISREG)
    #define	S_ISREG(m)	(((m)&S_IFMT) == S_IFREG)	/* file */
#endif

/*
#define LIT_INSERT_DATA(type, cleanup)                                                                                      \
    (                                                                                                                       \
    {                                                                                                                       \
        LitUserdata* userdata = lit_create_userdata(vm->state, sizeof(type));                                               \
        userdata->cleanup_fn = cleanup;                                                                                     \
        lit_table_set(vm->state, &AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), OBJECT_VALUE(userdata)); \
        (type*)userdata->data;                                                                                              \
    })
*/
typedef void(*CleanupFunc)(LitState*, LitUserdata*, bool);


static void* LIT_INSERT_DATA(LitVm* vm, LitValue instance, size_t typsz, CleanupFunc cleanup)
{
    LitUserdata* userdata = lit_create_userdata(vm->state, typsz);
    userdata->cleanup_fn = cleanup;
    lit_table_set(vm->state, &AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), OBJECT_VALUE(userdata));
    return userdata->data;
}

#define LIT_EXTRACT_DATA(type) \
    ({ \
        LitValue _d; \
        if(!lit_table_get(&AS_INSTANCE(instance)->fields, CONST_STRING(vm->state, "_data"), &_d)) \
        { \
            lit_runtime_error_exiting(vm, "failed to extract userdata"); \
        } \
        (type*)AS_USERDATA(_d)->data; \
    })

#define LIT_EXTRACT_DATA_FROM(from, type) \
    ({ \
        LitValue _d; \
        if(!lit_table_get(&AS_INSTANCE(from)->fields, CONST_STRING(vm->state, "_data"), &_d)) \
        { \
            lit_runtime_error_exiting(vm, "failed to extract userdata"); \
        } \
        (type*)AS_USERDATA(_d)->data; \
    })


/*
 * File
 */

typedef struct
{
    char* path;
    FILE* file;
} LitFileData;


void cleanup_file(LitState* state, LitUserdata* data, bool mark)
{
    (void)state;
    if(mark)
    {
        return;
    }

    LitFileData* file_data = ((LitFileData*)data->data);

    if(file_data->file != NULL)
    {
        fclose(file_data->file);
        file_data->file = NULL;
    }
}

static LitValue file_constructor(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    const char* path = LIT_CHECK_STRING(0);
    const char* mode = LIT_GET_STRING(1, "r");

    FILE* file = fopen(path, mode);

    if(file == NULL)
    {
        lit_runtime_error_exiting(vm, "Failed to open file %s with mode %s (C error: %s)", path, mode, strerror(errno));
    }

    LitFileData* data = (LitFileData*)LIT_INSERT_DATA(vm, instance, sizeof(LitFileData), cleanup_file);

    data->path = (char*)path;
    data->file = file;

    return instance;
}

static LitValue file_close(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    LitFileData* data = LIT_EXTRACT_DATA(LitFileData);
    fclose(data->file);

    return NULL_VALUE;
}

static LitValue file_exists(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    char* file_name = NULL;

    if(IS_INSTANCE(instance))
    {
        file_name = LIT_EXTRACT_DATA(LitFileData)->path;
    }
    else
    {
        file_name = (char*)LIT_CHECK_STRING(0);
    }

    return BOOL_VALUE(lit_file_exists(file_name));
}

/*
 * ==
 * File writing
 */

static LitValue file_write(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)

    LitString* value = lit_to_string(vm->state, argv[0]);
    fwrite(value->chars, value->length, 1, LIT_EXTRACT_DATA(LitFileData)->file);

    return NULL_VALUE;
}

static LitValue file_writeByte(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint8_t byte = (uint8_t)LIT_CHECK_NUMBER(vm, argv, argc, 0);
    lit_write_uint8_t(LIT_EXTRACT_DATA(LitFileData)->file, byte);

    return NULL_VALUE;
}

static LitValue file_writeShort(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint16_t shrt = (uint16_t)LIT_CHECK_NUMBER(vm, argv, argc, 0);
    lit_write_uint16_t(LIT_EXTRACT_DATA(LitFileData)->file, shrt);

    return NULL_VALUE;
}

static LitValue file_writeNumber(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    float num = (float)LIT_CHECK_NUMBER(vm, argv, argc, 0);
    lit_write_uint32_t(LIT_EXTRACT_DATA(LitFileData)->file, num);

    return NULL_VALUE;
}

static LitValue file_writeBool(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    bool value = LIT_CHECK_BOOL(0);

    lit_write_uint8_t(LIT_EXTRACT_DATA(LitFileData)->file, (uint8_t)value ? '1' : '0');
    return NULL_VALUE;
}

static LitValue file_writeString(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    if(LIT_CHECK_STRING(0) == NULL)
    {
        return NULL_VALUE;
    }

    LitString* string = AS_STRING(argv[0]);
    LitFileData* data = LIT_EXTRACT_DATA(LitFileData);

    lit_write_string(data->file, string);
    return NULL_VALUE;
}

/*
 * ==
 * File reading
 */

static LitValue file_readAll(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    LitFileData* data = LIT_EXTRACT_DATA(LitFileData);

    fseek(data->file, 0, SEEK_END);
    size_t length = ftell(data->file);
    fseek(data->file, 0, SEEK_SET);

    LitString* result = lit_allocate_empty_string(vm->state, length);

    result->chars = LIT_ALLOCATE(vm->state, char, length + 1);
    result->chars[length] = '\0';

    fread(result->chars, 1, length, data->file);

    result->hash = lit_hash_string(result->chars, result->length);
    lit_register_string(vm->state, result);

    return OBJECT_VALUE(result);
}

static LitValue file_readLine(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    size_t max_length = (size_t)LIT_GET_NUMBER(0, 128);
    LitFileData* data = LIT_EXTRACT_DATA(LitFileData);

    char line[max_length];

    if(!fgets(line, max_length, data->file))
    {
        return NULL_VALUE;
    }

    return OBJECT_VALUE(lit_copy_string(vm->state, line, strlen(line) - 1));
}

static LitValue file_readByte(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint8_t(LIT_EXTRACT_DATA(LitFileData)->file));
}

static LitValue file_readShort(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint16_t(LIT_EXTRACT_DATA(LitFileData)->file));
}

static LitValue file_readNumber(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint32_t(LIT_EXTRACT_DATA(LitFileData)->file));
}

static LitValue file_readBool(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return BOOL_VALUE((char)lit_read_uint8_t(LIT_EXTRACT_DATA(LitFileData)->file) == '1');
}

static LitValue file_readString(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    LitFileData* data = LIT_EXTRACT_DATA(LitFileData);
    LitString* string = lit_read_string(vm->state, data->file);

    return string == NULL ? NULL_VALUE : OBJECT_VALUE(string);
}

static LitValue file_getLastModified(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    struct stat buffer;
    char* file_name = NULL;
    (void)vm;
    (void)argc;
    (void)argv;
    if(IS_INSTANCE(instance))
    {
        file_name = LIT_EXTRACT_DATA(LitFileData)->path;
    }
    else
    {
        file_name = (char*)LIT_CHECK_STRING(0);
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

static LitValue directory_exists(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    const char* directory_name = LIT_CHECK_STRING(0);
    struct stat buffer;
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return BOOL_VALUE(stat(directory_name, &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

static LitValue directory_listFiles(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitState* state;
    LitArray* array;
    (void)instance;
    state = vm->state;
    array = lit_create_array(state);
    #if defined(__unix__) || defined(__linux__)
    {
        struct dirent* ep;
        DIR* dir = opendir(LIT_CHECK_STRING(0));
        if(dir == NULL)
        {
            return OBJECT_VALUE(array);
        }
        while((ep = readdir(dir)))
        {
            if(ep->d_type == DT_REG)
            {
                lit_values_write(state, &array->values, OBJECT_CONST_STRING(state, ep->d_name));
            }
        }


        closedir(dir);
    }
    #endif
    return OBJECT_VALUE(array);
}

static LitValue directory_listDirectories(LitVm* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitArray* array;
    LitState* state;
    (void)instance;
    state = vm->state;
    array = lit_create_array(state);
    #if defined(__unix__) || defined(__linux__)
    {
        struct dirent* ep;

        DIR* dir = opendir(LIT_CHECK_STRING(0));

        if(dir == NULL)
        {
            return OBJECT_VALUE(array);
        }

        while((ep = readdir(dir)))
        {
            if(ep->d_type == DT_DIR && strcmp(ep->d_name, "..") != 0 && strcmp(ep->d_name, ".") != 0)
            {
                lit_values_write(state, &array->values, OBJECT_CONST_STRING(state, ep->d_name));
            }
        }

        closedir(dir);
    }
    #endif
    return OBJECT_VALUE(array);
}

void lit_open_file_library(LitState* state)
{
    {
        LIT_BEGIN_CLASS("File");
        {
            LIT_BIND_STATIC_METHOD("exists", file_exists);
            LIT_BIND_STATIC_METHOD("getLastModified", file_getLastModified);
            LIT_BIND_CONSTRUCTOR(file_constructor);
            LIT_BIND_METHOD("close", file_close);
            LIT_BIND_METHOD("write", file_write);
            LIT_BIND_METHOD("writeByte", file_writeByte);
            LIT_BIND_METHOD("writeShort", file_writeShort);
            LIT_BIND_METHOD("writeNumber", file_writeNumber);
            LIT_BIND_METHOD("writeBool", file_writeBool);
            LIT_BIND_METHOD("writeString", file_writeString);
            LIT_BIND_METHOD("readAll", file_readAll);
            LIT_BIND_METHOD("readLine", file_readLine);
            LIT_BIND_METHOD("readByte", file_readByte);
            LIT_BIND_METHOD("readShort", file_readShort);
            LIT_BIND_METHOD("readNumber", file_readNumber);
            LIT_BIND_METHOD("readBool", file_readBool);
            LIT_BIND_METHOD("readString", file_readString);
            LIT_BIND_METHOD("getLastModified", file_getLastModified);
            LIT_BIND_GETTER("exists", file_exists);
        }
        LIT_END_CLASS();
    }
    {
        LIT_BEGIN_CLASS("Directory");
        {
            LIT_BIND_STATIC_METHOD("exists", directory_exists);
            LIT_BIND_STATIC_METHOD("listFiles", directory_listFiles);
            LIT_BIND_STATIC_METHOD("listDirectories", directory_listDirectories);
        }
        LIT_END_CLASS();
    }
}

