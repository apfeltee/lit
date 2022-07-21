

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef LIT_OS_WINDOWS
    #include <windows.h>
    #define stat _stat
#endif
#include "dirwrap.h"
#include "lit.h"

typedef struct LitFileData LitFileData;
struct LitFileData
{
    char* path;
    FILE* handle;
    bool isopen;
};

typedef void(*CleanupFunc)(LitState*, LitUserdata*, bool);

static void* LIT_INSERT_DATA(LitVM* vm, LitValue instance, size_t typsz, CleanupFunc cleanup)
{
    LitUserdata* userdata = lit_create_userdata(vm->state, typsz);
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

static LitValue file_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    const char* path;
    const char* mode;
    FILE* hnd;
    LitFileData* data;
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
    return instance;
}

static LitValue file_close(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
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

static LitValue file_exists(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
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

static LitValue file_write(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(1)
    size_t rt;
    LitString* value;
    value = lit_to_string(vm->state, argv[0]);
    rt = fwrite(value->chars, lit_string_length(value), 1, ((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle);
    return lit_number_to_value(rt);
}

static LitValue file_writeByte(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint8_t rt;
    uint8_t byte;
    byte = (uint8_t)lit_check_number(vm, argv, argc, 0);
    rt = lit_write_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, byte);
    return lit_number_to_value(rt);
}

static LitValue file_writeShort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint16_t rt;
    uint16_t shrt;
    shrt = (uint16_t)lit_check_number(vm, argv, argc, 0);
    rt = lit_write_uint16_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, shrt);
    return lit_number_to_value(rt);
}

static LitValue file_writeNumber(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    uint32_t rt;
    float num;
    num = (float)lit_check_number(vm, argv, argc, 0);
    rt = lit_write_uint32_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, num);
    return lit_number_to_value(rt);
}

static LitValue file_writeBool(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    bool value;
    uint8_t rt;
    value = lit_check_bool(vm, argv, argc, 0);
    rt = lit_write_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle, (uint8_t)value ? '1' : '0');
    return lit_number_to_value(rt);
}

static LitValue file_writeString(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitString* string;
    LitFileData* data;
    if(lit_check_string(vm, argv, argc, 0) == NULL)
    {
        return NULL_VALUE;
    }
    string = AS_STRING(argv[0]);
    data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    lit_write_string(data->handle, string);
    return NULL_VALUE;
}

/*
 * ==
 * File reading
 */

static LitValue file_readAll(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    size_t length;
    size_t actuallength;
    LitFileData* data;
    LitString* result;
    data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    fseek(data->handle, 0, SEEK_END);
    length = ftell(data->handle);
    fseek(data->handle, 0, SEEK_SET);
    result = lit_string_alloc_empty(vm->state, length, false);
    result->chars = LIT_ALLOCATE(vm->state, char, length + 1);
    result->chars[length] = '\0';
    actuallength = fread(result->chars, 1, length, data->handle);
    result->hash = lit_hash_string(result->chars, actuallength);
    lit_register_string(vm->state, result);
    return OBJECT_VALUE(result);
}

static LitValue file_readLine(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
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
    line = LIT_ALLOCATE(vm->state, char, max_length + 1);
    if(!fgets(line, max_length, data->handle))
    {
        LIT_FREE(vm->state, char, line);
        return NULL_VALUE;
    }
    return OBJECT_VALUE(lit_string_take(vm->state, line, strlen(line) - 1, false));
}

static LitValue file_readByte(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue file_readShort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint16_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue file_readNumber(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_number_to_value(lit_read_uint32_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle));
}

static LitValue file_readBool(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return BOOL_VALUE((char)lit_read_uint8_t(((LitFileData*)LIT_EXTRACT_DATA(vm, instance))->handle) == '1');
}

static LitValue file_readString(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    LitFileData* data = (LitFileData*)LIT_EXTRACT_DATA(vm, instance);
    LitString* string = lit_read_string(vm->state, data->handle);

    return string == NULL ? NULL_VALUE : OBJECT_VALUE(string);
}

static LitValue file_getLastModified(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
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

static LitValue directory_exists(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    const char* directory_name = lit_check_string(vm, argv, argc, 0);
    struct stat buffer;
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return BOOL_VALUE(stat(directory_name, &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

static LitValue directory_listFiles(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
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
                lit_values_write(state, &array->values, OBJECT_CONST_STRING(state, ep->d_name));
            }
        }
        closedir(dir);
    }
    #endif
    return OBJECT_VALUE(array);
}

static LitValue directory_listDirectories(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
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
                    lit_values_write(state, &array->values, OBJECT_CONST_STRING(state, ent.name));
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

