#pragma once
// Import-table patching, the same method XeniaLink uses (proven on this console).
#include "kernel.h"

#define MODULE_XAM      "xam.xex"
#define MODULE_KERNEL   "xboxkrnl.exe"

// xam.xex ordinals (from Xenia's xam_table.inc)
#define ORD_XamInputGetState            0x191

// xboxkrnl.exe ordinals
#define ORD_XexExecutableModuleHandle   0x193

// Returns the address of an export, or 0.
DWORD ResolveFunction(const char* module_name, DWORD ordinal);

// Redirects one import of 'module' (e.g. the running title) to 'hook'.
// Returns S_OK if at least one thunk was patched.
HRESULT PatchModuleImport(PLDR_DATA_TABLE_ENTRY module, const char* imported_module,
                          DWORD ordinal, DWORD hook);
