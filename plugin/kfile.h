#pragma once
// Native file access on \Device\Harddisk0\Partition1 paths (no drive letters needed),
// plus a small log. Based on XeniaLink's log.cpp.
#include "kernel.h"

LONG  KFileWrite(const char* path, const void* data, DWORD len, BOOL append);
DWORD KFileRead(const char* path, char* buf, DWORD bufSize);   // 0 if missing
LONG  KCreateDir(const char* path);

LONG LogInit(const char* path);
VOID Log(const char* fmt, ...);
