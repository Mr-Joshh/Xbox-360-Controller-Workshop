// Controller Workshop plugin - DashLaunch system plugin.
// When a game starts, it loads that game's remap profile (or ALL.ini) written by the Controller
// Tool app and redirects the game's XamInputGetState import through Remap_Apply. It also records
// each game it sees (title ID and name) in titles.txt so the app can list them.
#include <xtl.h>
#include <stdio.h>
#include "kernel.h"
#include "hooking.h"
#include "kfile.h"
#include "../src/remap.h"

#define REMAP_DIR   CT_DIR_DEVICE "\\Remap"
#define TITLES_FILE CT_DIR_DEVICE "\\titles.txt"
#define LOG_FILE    CT_DIR_DEVICE "\\plugin.log"

PLDR_DATA_TABLE_ENTRY GetTitleModule();

typedef DWORD (NTAPI *XamInputGetState_t)(DWORD userIndex, DWORD flags, PXINPUT_STATE state);

static XamInputGetState_t g_OrigGetState;
static RemapProfile       g_Profile;
static volatile LONG      g_RemapOn;
static BOOL               g_Unloading;
static char               g_FileBuf[64 * 1024];

static DWORD NTAPI Hook_XamInputGetState(DWORD userIndex, DWORD flags, PXINPUT_STATE state)
{
    DWORD result = g_OrigGetState(userIndex, flags, state);
    if (result == ERROR_SUCCESS && state && g_RemapOn)
        Remap_Apply(&g_Profile, &state->Gamepad);
    return result;
}

// Dashboards, launchers and this tool never get the ALL.ini profile and aren't listed.
static bool IsSystemTitle(DWORD id)
{
    switch (id)
    {
    case 0x00000000:
    case 0xFFFE07D1:    // dashboard
    case 0xFFFF0055:    // Aurora
    case 0xFFFF0C70:    // Controller Workshop
    case 0xC0DE9999:    // XeXMenu
    case 0xF5D20000:    // FreeStyle Dash
    case 0xF5D10000:
        return true;
    }
    return false;
}

static DWORD ModuleTitleId(PLDR_DATA_TABLE_ENTRY module)
{
    // XEX_EXECUTION_ID: MediaID, Version, BaseVersion, TitleID, ...
    const DWORD* exec = (const DWORD*)RtlImageXexHeaderField(module->XexHeaderBase, 0x00040006);
    return exec ? exec[3] : 0;
}

static DWORD Be32(const BYTE* p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static WORD  Be16(const BYTE* p) { return (WORD)((p[0] << 8) | p[1]); }

// Reads the game's name from the SPA (XDBF) resource named after its title ID.
// Layout from Xenia's xdbf_utils: string 0x8000 of the default language's string table.
static bool GetTitleName(PLDR_DATA_TABLE_ENTRY module, DWORD titleId, char* out, int outSize)
{
    char section[16];
    _snprintf(section, sizeof(section), "%08X", titleId);
    PVOID data = NULL;
    ULONG size = 0;
    if (!XGetModuleSection((HANDLE)module, section, &data, &size) || !data || size < 24)
        return false;

    const BYTE* base = (const BYTE*)data;
    if (Be32(base) != 'XDBF')
        return false;
    DWORD entryCount = Be32(base + 8), entryUsed = Be32(base + 12), freeCount = Be32(base + 16);
    const BYTE* entries = base + 24;
    const BYTE* content = entries + entryCount * 18 + freeCount * 8;

    // Default language from the XSTC block (section 1); English (1) otherwise.
    DWORD language = 1;
    for (int pass = 0; pass < 2; pass++)
    {
        for (DWORD i = 0; i < entryUsed; i++)
        {
            const BYTE* e = entries + i * 18;
            WORD sec = Be16(e);
            DWORD idHigh = Be32(e + 2), idLow = Be32(e + 6);
            const BYTE* block = content + Be32(e + 10);
            if (block + 16 > base + size)
                continue;
            if (pass == 0 && sec == 1 && idHigh == 0 && idLow == 'XSTC')
                language = Be32(block + 12);
            if (pass == 1 && sec == 3 && idHigh == 0 && idLow == language && Be32(block) == 'XSTR')
            {
                WORD count = Be16(block + 12);
                const BYTE* p = block + 14;
                for (WORD s = 0; s < count && p + 4 <= base + size; s++)
                {
                    WORD id = Be16(p), len = Be16(p + 2);
                    p += 4;
                    if (id == 0x8000)
                    {
                        int n = len < outSize - 1 ? len : outSize - 1;
                        memcpy(out, p, n);
                        out[n] = 0;
                        return n > 0;
                    }
                    p += len;
                }
            }
        }
    }
    return false;
}

// Adds "TITLEID|Name" to titles.txt the first time a game is seen.
static void RecordTitle(DWORD titleId, const char* name)
{
    char key[16];
    _snprintf(key, sizeof(key), "%08X|", titleId);
    KFileRead(TITLES_FILE, g_FileBuf, sizeof(g_FileBuf));
    if (strstr(g_FileBuf, key))
        return;
    char line[160];
    int n = _snprintf(line, sizeof(line) - 1, "%s%s\r\n", key, name);
    if (n < 0) n = sizeof(line) - 1;
    KFileWrite(TITLES_FILE, line, n, TRUE);
}

// Loads the game's own profile, or ALL.ini for regular games. Returns false if neither applies.
static bool LoadProfile(DWORD titleId, char* which, int whichSize)
{
    char path[128];
    _snprintf(path, sizeof(path), REMAP_DIR "\\%08X.ini", titleId);
    if (KFileRead(path, g_FileBuf, sizeof(g_FileBuf)))
    {
        _snprintf(which, whichSize, "%08X.ini", titleId);
    }
    else if (!IsSystemTitle(titleId) && KFileRead(REMAP_DIR "\\" CT_REMAP_ALL ".ini", g_FileBuf, sizeof(g_FileBuf)))
    {
        _snprintf(which, whichSize, CT_REMAP_ALL ".ini");
    }
    else
        return false;
    Remap_Parse(g_FileBuf, &g_Profile, NULL, 0);
    return !Remap_IsDefault(&g_Profile);
}

static DWORD WINAPI MainThread(LPVOID)
{
    // Plugins load early in boot; wait until the hard drive accepts files.
    LONG status = -1;
    for (int i = 0; i < 120 && status < 0; i++)
    {
        status = KCreateDir(CT_DIR_DEVICE);
        if (status < 0)
            Sleep(500);
    }
    KCreateDir(REMAP_DIR);
    LogInit(LOG_FILE);
    g_OrigGetState = (XamInputGetState_t)ResolveFunction(MODULE_XAM, ORD_XamInputGetState);
    Log("Controller Workshop plugin 0.1 loaded. XamInputGetState at %08X", (DWORD)g_OrigGetState);

    // Per title: wait for its module to load, read its name and profile, then hook (retrying,
    // because the title ID changes before the loader has resolved the game's imports).
    enum { IDLE, WAIT_MODULE, WAIT_HOOK } state = IDLE;
    DWORD lastTitle = 0xFFFFFFFF;
    DWORD giveUpAt = 0;
    while (!g_Unloading)
    {
        DWORD titleId = XamGetCurrentTitleId();
        if (titleId != lastTitle)
        {
            g_RemapOn = 0;
            lastTitle = titleId;
            state = titleId != 0 ? WAIT_MODULE : IDLE;
            giveUpAt = GetTickCount() + 10000;
        }

        PLDR_DATA_TABLE_ENTRY module = state != IDLE ? GetTitleModule() : NULL;
        bool moduleReady = module && module->XexHeaderBase && ModuleTitleId(module) == titleId;

        if (state == WAIT_MODULE && moduleReady)
        {
            char name[128];
            if (!GetTitleName(module, titleId, name, sizeof(name)))
                _snprintf(name, sizeof(name), "Title %08X", titleId);
            if (!IsSystemTitle(titleId))
                RecordTitle(titleId, name);

            char which[32];
            if (LoadProfile(titleId, which, sizeof(which)) && g_OrigGetState)
            {
                Log("%08X %s: using %s", titleId, name, which);
                state = WAIT_HOOK;
            }
            else
            {
                Log("%08X %s: no remap", titleId, name);
                state = IDLE;
            }
        }
        if (state == WAIT_HOOK && moduleReady &&
            PatchModuleImport(module, MODULE_XAM, ORD_XamInputGetState, (DWORD)Hook_XamInputGetState) == S_OK)
        {
            g_RemapOn = 1;
            state = IDLE;
            Log("%08X: remap active", titleId);
        }
        if (state != IDLE && (LONG)(GetTickCount() - giveUpAt) > 0)
        {
            Log("%08X: gave up (%s)", titleId, state == WAIT_MODULE ? "module not found" : "import not hooked");
            state = IDLE;
        }
        Sleep(20);
    }
    return 0;
}

BOOL APIENTRY DllMain(HANDLE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        HANDLE thread = NULL;
        DWORD threadId = 0;
        // System thread (flag 2) so it survives title changes.
        ExCreateThread(&thread, 0, &threadId, (PVOID)XapiThreadStartup,
                       (LPTHREAD_START_ROUTINE)MainThread, NULL, 2);
        if (!thread)
            thread = CreateThread(NULL, 0, MainThread, NULL, 0, &threadId);
        if (thread)
            CloseHandle(thread);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        g_Unloading = TRUE;
        g_RemapOn = 0;
        Sleep(50);
    }
    return TRUE;
}
