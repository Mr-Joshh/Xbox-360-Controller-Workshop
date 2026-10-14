// system.cpp - see system.h
#include <xtl.h>
#include <stdio.h>
#include "system.h"
#include "remap.h"

extern "C"
{
    typedef struct _KSTRING
    {
        USHORT Length;
        USHORT MaximumLength;
        PCHAR  Buffer;
    } KSTRING, *PKSTRING;

    VOID  NTAPI RtlInitAnsiString(PKSTRING DestinationString, const char* SourceString);
    LONG  NTAPI ObCreateSymbolicLink(PKSTRING SymbolicLinkName, PKSTRING DeviceName);
    DWORD NTAPI XexGetModuleHandle(PCSTR ModuleName, PHANDLE Module);
}

void Sys_MountHdd()
{
    KSTRING link, device;
    RtlInitAnsiString(&link, "\\??\\Hdd:");
    RtlInitAnsiString(&device, "\\Device\\Harddisk0\\Partition1");
    ObCreateSymbolicLink(&link, &device);   // fails harmlessly if it already exists
}

bool Sys_PluginLoaded()
{
    HANDLE module = NULL;
    return XexGetModuleHandle(CT_PLUGIN_NAME, &module) == 0 && module != NULL;
}

int Sys_ReadFile(const char* path, char* buf, int size)
{
    buf[0] = 0;
    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    DWORD read = 0;
    ReadFile(h, buf, size - 1, &read, NULL);
    CloseHandle(h);
    buf[read] = 0;
    return (int)read;
}

bool Sys_WriteFile(const char* path, const char* data, int len)
{
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, len, &written, NULL);
    CloseHandle(h);
    return ok && written == (DWORD)len;
}

bool Sys_DeleteFile(const char* path)
{
    return DeleteFile(path) != 0;
}

void Sys_CreateDir(const char* path)
{
    CreateDirectory(path, NULL);
}

void Sys_ListDir(const char* dir, DirCallback cb, void* ctx)
{
    char pattern[MAX_PATH];
    _snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
            cb(fd.cFileName, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, ctx);
    } while (FindNextFile(h, &fd));
    FindClose(h);
}

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

#define OLD_DIR_HDD "Hdd:\\ControllerTool"

static char g_MigrateBuf[64 * 1024], g_MigrateBuf2[64 * 1024];

static void CopyIfMissing(const char* from, const char* to)
{
    if (GetFileAttributes(to) != INVALID_FILE_ATTRIBUTES)
        return;
    int n = Sys_ReadFile(from, g_MigrateBuf, sizeof(g_MigrateBuf));
    if (n >= 0)
        Sys_WriteFile(to, g_MigrateBuf, n);
}

static void OnOldProfile(const char* name, bool isDir, void*)
{
    if (isDir)
        return;
    char from[MAX_PATH], to[MAX_PATH];
    _snprintf(from, sizeof(from), OLD_DIR_HDD "\\Remap\\%s", name);
    _snprintf(to, sizeof(to), CT_DIR_HDD "\\Remap\\%s", name);
    CopyIfMissing(from, to);
}

void Sys_MigrateOldData()
{
    if (GetFileAttributes(OLD_DIR_HDD) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributes(CT_DIR_HDD "\\migrated.txt") != INVALID_FILE_ATTRIBUTES)
        return;

    Sys_CreateDir(CT_DIR_HDD);
    Sys_CreateDir(CT_DIR_HDD "\\Remap");
    CopyIfMissing(OLD_DIR_HDD "\\settings.ini", CT_DIR_HDD "\\settings.ini");
    Sys_ListDir(OLD_DIR_HDD "\\Remap", OnOldProfile, NULL);

    // titles.txt: the new plugin may already have started a list, so add the old lines it lacks.
    int oldLen = Sys_ReadFile(OLD_DIR_HDD "\\titles.txt", g_MigrateBuf, sizeof(g_MigrateBuf));
    if (oldLen > 0)
    {
        int newLen = Sys_ReadFile(CT_DIR_HDD "\\titles.txt", g_MigrateBuf2, sizeof(g_MigrateBuf2));
        if (newLen < 0)
            newLen = 0;
        for (char* line = g_MigrateBuf; *line;)
        {
            char* end = strchr(line, '\n');
            int len = end ? (int)(end - line + 1) : (int)strlen(line);
            if (len > 9 && line[8] == '|')
            {
                char key[10];
                memcpy(key, line, 9);
                key[9] = 0;
                if (!strstr(g_MigrateBuf2, key) && newLen + len < (int)sizeof(g_MigrateBuf2) - 1)
                {
                    memcpy(g_MigrateBuf2 + newLen, line, len);
                    newLen += len;
                    g_MigrateBuf2[newLen] = 0;
                }
            }
            line += len;
        }
        Sys_WriteFile(CT_DIR_HDD "\\titles.txt", g_MigrateBuf2, newLen);
    }

    static const char note[] = "Copied from Hdd:\\ControllerTool. Delete that folder once Controller Workshop works.\r\n";
    Sys_WriteFile(CT_DIR_HDD "\\migrated.txt", note, sizeof(note) - 1);
}
