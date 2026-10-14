#pragma once
// Kernel/xam declarations the public XDK headers leave out (copied from XeniaLink). All of these are
// exported by xboxkrnl.lib / xapilib.lib from the XDK, so xkelib isn't needed.
#include <xtl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _KSTRING {
  USHORT Length;
  USHORT MaximumLength;
  PCHAR Buffer;
} KSTRING, *PKSTRING;

// Loader entry for a module (layout from Xenia's X_LDR_DATA_TABLE_ENTRY).
typedef struct _LDR_DATA_TABLE_ENTRY {
  LIST_ENTRY InLoadOrderLinks;            // 0x00
  LIST_ENTRY InClosureOrderLinks;         // 0x08
  LIST_ENTRY InInitializationOrderLinks;  // 0x10
  PVOID NtHeadersBase;                    // 0x18
  PVOID ImageBase;                        // 0x1C
  DWORD SizeOfNtImage;                    // 0x20
  DWORD FullDllName[2];                   // 0x24 UNICODE_STRING
  DWORD BaseDllName[2];                   // 0x2C UNICODE_STRING
  DWORD Flags;                            // 0x34
  DWORD SizeOfFullImage;                  // 0x38
  PVOID EntryPoint;                       // 0x3C
  WORD LoadCount;                         // 0x40
  WORD ModuleIndex;                       // 0x42
  PVOID DllBaseOriginal;                  // 0x44
  DWORD CheckSum;                         // 0x48
  DWORD ModuleLoadFlags;                  // 0x4C
  DWORD TimeDateStamp;                    // 0x50
  PVOID LoadedImports;                    // 0x54
  PVOID XexHeaderBase;                    // 0x58
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

// XEX optional header 0x000103FF (import libraries).
typedef struct _XEX_IMPORT_DESCRIPTOR {
  DWORD Size;
  DWORD NameTableSize;
  DWORD ModuleCount;
} XEX_IMPORT_DESCRIPTOR, *PXEX_IMPORT_DESCRIPTOR;

typedef struct _XEX_IMPORT_TABLE {
  DWORD TableSize;          // 0x00
  BYTE NextImportDigest[20];// 0x04
  DWORD ModuleNumber;       // 0x18
  DWORD Version[2];         // 0x1C
  WORD NameIndex;           // 0x24
  WORD ImportCount;         // 0x26
  // DWORD ImportStubAddr[ImportCount] follows at 0x28
} XEX_IMPORT_TABLE, *PXEX_IMPORT_TABLE;

// Native file I/O, so the plugin doesn't depend on drive-letter links.
typedef struct _KOBJECT_ATTRIBUTES {
  HANDLE RootDirectory;
  PKSTRING ObjectName;
  ULONG Attributes;
} KOBJECT_ATTRIBUTES, *PKOBJECT_ATTRIBUTES;

typedef struct _KIO_STATUS_BLOCK {
  union { LONG Status; PVOID Pointer; };
  ULONG_PTR Information;
} KIO_STATUS_BLOCK, *PKIO_STATUS_BLOCK;

LONG NTAPI NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
                        PKOBJECT_ATTRIBUTES ObjectAttributes, PKIO_STATUS_BLOCK IoStatusBlock,
                        PLARGE_INTEGER AllocationSize, ULONG FileAttributes,
                        ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions);
LONG NTAPI NtWriteFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext,
                       PKIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length,
                       PLARGE_INTEGER ByteOffset);
LONG NTAPI NtReadFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext,
                      PKIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length,
                      PLARGE_INTEGER ByteOffset);
LONG NTAPI NtClose(HANDLE Handle);

VOID NTAPI RtlInitAnsiString(PKSTRING DestinationString, const char* SourceString);
LONG NTAPI ObCreateSymbolicLink(PKSTRING SymbolicLinkName, PKSTRING DeviceName);
LONG NTAPI ObDeleteSymbolicLink(PKSTRING SymbolicLinkName);

DWORD NTAPI XexGetModuleHandle(PCSTR ModuleName, PHANDLE Module);
DWORD NTAPI XexGetProcedureAddress(HANDLE Module, DWORD Ordinal, PVOID OutAddress);
PVOID NTAPI RtlImageXexHeaderField(PVOID XexHeaderBase, DWORD Key);

DWORD NTAPI ExCreateThread(PHANDLE Handle, DWORD StackSize, LPDWORD ThreadId,
                           PVOID ApiThreadStartup, LPTHREAD_START_ROUTINE StartAddress,
                           LPVOID Parameter, DWORD CreationFlags);
VOID XapiThreadStartup(VOID (__cdecl* StartRoutine)(VOID*), VOID* StartContext);

DWORD XamGetCurrentTitleId();

#ifdef __cplusplus
}
#endif

// The running title's loader entry. Resolved at runtime (kernel export 0x193)
// to avoid declaring a dllimport data symbol.
PLDR_DATA_TABLE_ENTRY GetTitleModule();
