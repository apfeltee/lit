
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define LITDIR_PATHSIZE 1024
#if defined(__unix__) || defined(__linux__)
    #define LITDIR_ISUNIX
#endif

#if defined(LITDIR_ISUNIX)
    #include <dirent.h>
#else
    #include <windows.h>
#endif

#if defined (S_IFDIR) && !defined (S_ISDIR)
    #define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)	/* directory */
#endif
#if defined (S_IFREG) && !defined (S_ISREG)
    #define	S_ISREG(m)	(((m)&S_IFMT) == S_IFREG)	/* file */
#endif

typedef struct LitDirReader LitDirReader;
typedef struct LitDirItem LitDirItem;

struct LitDirReader
{
    void* handle;
};

struct LitDirItem
{
    char name[LITDIR_PATHSIZE + 1];
    bool isdir;
    bool isfile;
};

bool lit_fs_diropen(LitDirReader* rd, const char* path)
{
    #if defined(LITDIR_ISUNIX)
        if((rd->handle = opendir(path)) == NULL)
        {
            return false;
        }
        return true;
    #endif
    return false;
}

bool lit_fs_dirread(LitDirReader* rd, LitDirItem* itm)
{
    itm->isdir = false;
    itm->isfile = false;
    memset(itm->name, 0, LITDIR_PATHSIZE);
    #if defined(LITDIR_ISUNIX)
        struct dirent* ent;
        if((ent = readdir((DIR*)(rd->handle))) == NULL)
        {
            return false;
        }
        if(ent->d_type == DT_DIR)
        {
            itm->isdir = true;
        }
        if(ent->d_type == DT_REG)
        {
            itm->isfile = true;
        }
        strcpy(itm->name, ent->d_name);
        return true;
    #endif
    return false;
}

bool lit_fs_dirclose(LitDirReader* rd)
{
    #if defined(LITDIR_ISUNIX)
        closedir((DIR*)(rd->handle));
    #endif
    return false;
}


