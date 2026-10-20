// games.cpp - see games.h
#include "games.h"
#include "remap.h"
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
    struct Builder
    {
        GameEntry* list;
        int        count, max;
        DWORD      titleId;     // scratch for the content scan
        const char* folder;     // profile folder under CT_DIR_HDD
    };

    char g_Buf[64 * 1024];

    GameEntry* Find(Builder& b, DWORD id)
    {
        for (int i = 0; i < b.count; i++)
            if (b.list[i].titleId == id)
                return &b.list[i];
        return NULL;
    }

    // Adds a game or fills in a missing name. Earlier sources win.
    void Add(Builder& b, DWORD id, const char* name)
    {
        if (id == 0 || id == 0xFFFFFFFF)
            return;
        GameEntry* e = Find(b, id);
        if (!e)
        {
            if (b.count >= b.max)
                return;
            e = &b.list[b.count++];
            e->titleId = id;
            e->name[0] = 0;
            e->hasProfile = false;
        }
        if (!e->name[0] && name && name[0])
        {
            // The font only has ASCII.
            int n = 0;
            for (const unsigned char* p = (const unsigned char*)name; *p && n < (int)sizeof(e->name) - 1; p++)
                e->name[n++] = (*p >= 0x20 && *p < 0x7F) ? *p : '?';
            e->name[n] = 0;
        }
    }

    bool ParseHex(const char* s, DWORD* out)
    {
        DWORD v = 0;
        for (int i = 0; i < 8; i++)
        {
            char c = s[i];
            int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 :
                    (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
            if (d < 0)
                return false;
            v = (v << 4) | d;
        }
        *out = v;
        return true;
    }

    bool IsHidden(DWORD id)
    {
        return id == 0xFFFE07D1 || id == 0xFFFF0055 || id == 0xFFFF0C70 || id == 0xC0DE9999 ||
               id == 0xF5D20000 || id == 0xF5D10000;
    }

    // titles.txt: "TITLEID|Name" per line, written by the plugin.
    void LoadSeenTitles(Builder& b)
    {
        if (Sys_ReadFile(CT_DIR_HDD "\\titles.txt", g_Buf, sizeof(g_Buf)) <= 0)
            return;
        char* line = g_Buf;
        while (*line)
        {
            char* end = strchr(line, '\n');
            if (end) *end = 0;
            char* cr = strchr(line, '\r');
            if (cr) *cr = 0;
            DWORD id;
            if (strlen(line) > 9 && line[8] == '|' && ParseHex(line, &id) && !IsHidden(id))
                Add(b, id, line + 9);
            if (!end) break;
            line = end + 1;
        }
    }

    // Package headers (GOD, XBLA, installs) hold the title ID at 0x360 and the
    // display name (UTF-16 big-endian) at 0x411.
    void OnPackage(const char* name, bool isDir, void* ctx)
    {
        Builder& b = *(Builder*)ctx;
        if (isDir || Find(b, b.titleId))
            return;
        char path[MAX_PATH];
        _snprintf(path, sizeof(path), "Hdd:\\Content\\0000000000000000\\%08X\\%s", b.titleId, name);
        static char header[0x600];
        if (Sys_ReadFile(path, header, sizeof(header)) < 0x491)
            return;
        const unsigned char* h = (const unsigned char*)header;
        if (memcmp(h, "CON ", 4) && memcmp(h, "LIVE", 4) && memcmp(h, "PIRS", 4))
            return;
        char display[64];
        int n = 0;
        for (int i = 0; i < 63; i++)
        {
            unsigned c = (h[0x411 + i * 2] << 8) | h[0x412 + i * 2];
            if (!c) break;
            display[n++] = c < 0x7F ? (char)c : '?';
        }
        display[n] = 0;
        Add(b, b.titleId, display);
    }

    void OnContentTitle(const char* name, bool isDir, void* ctx)
    {
        Builder& b = *(Builder*)ctx;
        DWORD id;
        if (!isDir || strlen(name) != 8 || !ParseHex(name, &id) || IsHidden(id) || Find(b, id))
            return;
        // 00007000 = Games on Demand, 000D0000 = Arcade, 00004000 = installed disc games.
        static const char* kinds[] = { "00007000", "000D0000", "00004000" };
        for (int k = 0; k < 3 && !Find(b, id); k++)
        {
            char dir[MAX_PATH];
            _snprintf(dir, sizeof(dir), "Hdd:\\Content\\0000000000000000\\%s\\%s", name, kinds[k]);
            b.titleId = id;
            Sys_ListDir(dir, OnPackage, &b);
        }
    }

    void OnProfile(const char* name, bool isDir, void* ctx)
    {
        Builder& b = *(Builder*)ctx;
        DWORD id;
        if (isDir || strlen(name) != 12 || _stricmp(name + 8, ".ini") || !ParseHex(name, &id))
            return;
        char path[MAX_PATH], profileName[64];
        _snprintf(path, sizeof(path), CT_DIR_HDD "\\%s\\%s", b.folder, name);
        if (Sys_ReadFile(path, g_Buf, sizeof(g_Buf)) < 0)
            return;
        // Only the name= line is used, which remap and dead zone profiles both have.
        RemapProfile p;
        Remap_Parse(g_Buf, &p, profileName, sizeof(profileName));
        Add(b, id, profileName);
        GameEntry* e = Find(b, id);
        if (e)
            e->hasProfile = true;
    }

    int CompareNames(const void* a, const void* b)
    {
        return _stricmp(((const GameEntry*)a)->name, ((const GameEntry*)b)->name);
    }
}

void Games_ProfilePath(const char* folder, DWORD titleId, char* out, int size)
{
    if (titleId == 0)
        _snprintf(out, size, CT_DIR_HDD "\\%s\\" CT_REMAP_ALL ".ini", folder);
    else
        _snprintf(out, size, CT_DIR_HDD "\\%s\\%08X.ini", folder, titleId);
}

int Games_Load(GameEntry* out, int max, const char* folder)
{
    if (max < 1)
        return 0;
    // Entry 0 is always "All games".
    out[0].titleId = 0;
    strcpy(out[0].name, "All games (default)");
    char path[MAX_PATH];
    Games_ProfilePath(folder, 0, path, sizeof(path));
    out[0].hasProfile = Sys_ReadFile(path, g_Buf, sizeof(g_Buf)) >= 0;

    Builder b = { out + 1, 0, max - 1, 0, folder };
    LoadSeenTitles(b);
    Sys_ListDir("Hdd:\\Content\\0000000000000000", OnContentTitle, &b);
    char dir[MAX_PATH];
    _snprintf(dir, sizeof(dir), CT_DIR_HDD "\\%s", folder);
    Sys_ListDir(dir, OnProfile, &b);

    for (int i = 0; i < b.count; i++)
        if (!out[1 + i].name[0])
            _snprintf(out[1 + i].name, sizeof(out[1 + i].name), "Title %08X", out[1 + i].titleId);
    qsort(out + 1, b.count, sizeof(GameEntry), CompareNames);
    return b.count + 1;
}
