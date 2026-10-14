// kfile.cpp - see kfile.h
#include "kfile.h"
#include <stdio.h>
#include <stdarg.h>

#define FILE_SHARE_READ_WRITE     0x3
#define FILE_OPEN_DISPOSITION     1
#define FILE_OPEN_IF_DISPOSITION  3
#define FILE_OVERWRITE_IF_DISP    5
#define FILE_DIRECTORY_FILE_OPT   0x01
#define FILE_SYNC_NONDIR          (0x20 | 0x40)  // synchronous I/O, non-directory
#define KOBJ_CASE_INSENSITIVE     0x40

static CRITICAL_SECTION g_LogLock;
static char g_LogPath[MAX_PATH];
static BOOL g_LogReady = FALSE;

static LONG OpenFile(const char* path, ACCESS_MASK access, ULONG disposition, ULONG options, HANDLE* out)
{
    KSTRING name;
    RtlInitAnsiString(&name, path);
    KOBJECT_ATTRIBUTES attrs = { NULL, &name, KOBJ_CASE_INSENSITIVE };
    KIO_STATUS_BLOCK io;
    return NtCreateFile(out, access | SYNCHRONIZE, &attrs, &io, NULL, FILE_ATTRIBUTE_NORMAL,
                        FILE_SHARE_READ_WRITE, disposition, options);
}

LONG KFileWrite(const char* path, const void* data, DWORD len, BOOL append)
{
    HANDLE h = NULL;
    LONG status = OpenFile(path, GENERIC_WRITE, append ? FILE_OPEN_IF_DISPOSITION : FILE_OVERWRITE_IF_DISP,
                           FILE_SYNC_NONDIR, &h);
    if (status < 0)
        return status;
    KIO_STATUS_BLOCK io;
    LARGE_INTEGER offset;
    offset.HighPart = -1;
    offset.LowPart = 0xFFFFFFFF;  // FILE_WRITE_TO_END_OF_FILE
    if (len)
        status = NtWriteFile(h, NULL, NULL, NULL, &io, (PVOID)data, len, append ? &offset : NULL);
    NtClose(h);
    return status;
}

DWORD KFileRead(const char* path, char* buf, DWORD bufSize)
{
    HANDLE h = NULL;
    buf[0] = 0;
    if (OpenFile(path, GENERIC_READ, FILE_OPEN_DISPOSITION, FILE_SYNC_NONDIR, &h) < 0)
        return 0;
    KIO_STATUS_BLOCK io;
    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    DWORD n = 0;
    if (NtReadFile(h, NULL, NULL, NULL, &io, buf, bufSize - 1, &offset) >= 0)
        n = (DWORD)io.Information;
    NtClose(h);
    buf[n] = 0;
    return n;
}

LONG KCreateDir(const char* path)
{
    HANDLE h = NULL;
    LONG status = OpenFile(path, GENERIC_READ, FILE_OPEN_IF_DISPOSITION, FILE_DIRECTORY_FILE_OPT | 0x20, &h);
    if (status >= 0)
        NtClose(h);
    return status;
}

LONG LogInit(const char* path)
{
    static BOOL lockReady = FALSE;
    if (!lockReady)
    {
        InitializeCriticalSection(&g_LogLock);
        lockReady = TRUE;
    }
    strncpy_s(g_LogPath, sizeof(g_LogPath), path, _TRUNCATE);
    static const char header[] = "Controller Workshop plugin log\r\n";
    LONG status = KFileWrite(g_LogPath, header, sizeof(header) - 1, FALSE);
    g_LogReady = status >= 0;
    return status;
}

VOID Log(const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "[%08X] ", GetTickCount());
    int m = _vsnprintf_s(line + n, sizeof(line) - n - 2, _TRUNCATE, fmt, args);
    va_end(args);
    if (m < 0)
        m = (int)strlen(line + n);
    OutputDebugStringA(line + n);
    OutputDebugStringA("\n");
    if (!g_LogReady)
        return;
    line[n + m] = '\r';
    line[n + m + 1] = '\n';
    EnterCriticalSection(&g_LogLock);
    KFileWrite(g_LogPath, line, n + m + 2, TRUE);
    LeaveCriticalSection(&g_LogLock);
}
