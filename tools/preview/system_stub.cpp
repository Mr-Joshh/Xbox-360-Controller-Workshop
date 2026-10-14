// Preview stand-in for src/system.cpp: a fake hard drive with a few games and one profile.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../../src/system.h"

static const char kTitles[] =
    "4D5307E6|Halo 3\r\n"
    "4D530877|Halo: Reach\r\n"
    "415607F7|Assassin's Creed II\r\n"
    "45410914|Battlefield 3\r\n"
    "58411294|Star Wars: First Assault\r\n"
    "584111F7|Minecraft: Xbox 360 Edition\r\n"
    "4D5307D6|Shadowrun\r\n"
    "425307D6|Fallout 3\r\n"
    "4541080F|Mirror's Edge\r\n"
    "41560817|Call of Duty: Modern Warfare 2\r\n"
    "545407F2|Grand Theft Auto IV\r\n";

static const char kHalo3[] =
    "name=Halo 3\r\nA=B\r\nB=A\r\nLT=RT\r\nRT=LT\r\nBACK=NONE\r\ninvert_ry=1\r\n";

void Sys_MountHdd() {}
void Sys_MigrateOldData() {}
bool Sys_PluginLoaded() { return true; }

int Sys_ReadFile(const char* path, char* buf, int size)
{
    const char* src = NULL;
    if (strstr(path, "titles.txt")) src = kTitles;
    else if (strstr(path, "4D5307E6.ini")) src = kHalo3;
    if (!src)
    {
        buf[0] = 0;
        return -1;
    }
    strncpy(buf, src, size - 1);
    buf[size - 1] = 0;
    return (int)strlen(buf);
}

bool Sys_WriteFile(const char*, const char*, int) { return true; }
bool Sys_DeleteFile(const char*) { return true; }
void Sys_CreateDir(const char*) {}

void Sys_ListDir(const char* dir, DirCallback cb, void* ctx)
{
    if (strstr(dir, "Remap"))
        cb("4D5307E6.ini", false, ctx);
}
