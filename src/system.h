// system.h - hard drive access and plugin detection for the app.
#pragma once

// Creates the Hdd: drive link (titles don't get one by default).
void Sys_MountHdd();

// True when ControllerWorkshopPlugin.xex is loaded (DashLaunch plugin).
bool Sys_PluginLoaded();

// Reads a whole file as text (NUL-terminated). Returns the length, or -1 if it can't be opened.
int  Sys_ReadFile(const char* path, char* buf, int size);
bool Sys_WriteFile(const char* path, const char* data, int len);
bool Sys_DeleteFile(const char* path);
void Sys_CreateDir(const char* path);

// Calls cb for every entry in a folder ("." and ".." skipped).
typedef void (*DirCallback)(const char* name, bool isDir, void* ctx);
void Sys_ListDir(const char* dir, DirCallback cb, void* ctx);

// One-time copy of profiles, the game list and settings from the old Hdd:\ControllerTool folder
// (the app's name before it became Controller Workshop). Existing new files are kept.
void Sys_MigrateOldData();
