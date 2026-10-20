// gamelist_ui.cpp - see gamelist_ui.h
#include "gamelist_ui.h"
#include "theme.h"
#include <string.h>

void GameList_Load(GameList& list, const char* folder)
{
    list.count = Games_Load(list.games, GAMELIST_MAX, folder);
    if (list.sel >= list.count)
        list.sel = list.count - 1;
}

void GameList_Move(GameList& list, char nav)
{
    if (nav == 'U') list.sel = (list.sel + list.count - 1) % list.count;
    if (nav == 'D') list.sel = (list.sel + 1) % list.count;
    if (nav == 'L') list.sel = list.sel > GAMELIST_ROWS ? list.sel - GAMELIST_ROWS : 0;
    if (nav == 'R') list.sel = list.sel + GAMELIST_ROWS < list.count ? list.sel + GAMELIST_ROWS : list.count - 1;
}

void GameList_Draw(GameList& list, const char* title, bool pluginLoaded, const char* allGamesNote)
{
    R::Text(64, 40, 3, COL_TEXT, ALIGN_LEFT, title);
    if (pluginLoaded)
        R::Text(64, 80, 2, COL_LIT, ALIGN_LEFT, "Plugin running - profiles load automatically when a game starts");
    else
    {
        R::Text(64, 80, 2, COL_YELLOW, ALIGN_LEFT, "Plugin not running - profiles won't be applied yet");
        R::Text(64, 102, 1, COL_DIM, ALIGN_LEFT,
                "Add Hdd:\\ControllerWorkshop\\ControllerWorkshopPlugin.xex as a DashLaunch plugin and restart the console (see README).");
    }

    R::RoundRect(64, 124, 1152, GAMELIST_ROWS * 44 + 16, 12, COL_PANEL);
    if (list.sel < list.top) list.top = list.sel;
    if (list.sel >= list.top + GAMELIST_ROWS) list.top = list.sel - GAMELIST_ROWS + 1;
    for (int r = 0; r < GAMELIST_ROWS && list.top + r < list.count; r++)
    {
        int i = list.top + r;
        const GameEntry& g = list.games[i];
        float y = 132 + r * 44.0f;
        bool sel = i == list.sel;
        if (sel)
            R::RoundRect(72, y, 1120, 40, 8, COL_ACCENT);
        DWORD c = sel ? COL_SELECTED : COL_TEXT;
        UI_TextClipped(90, y + 12, 2, c, 46, g.name);
        if (g.hasProfile)
        {
            R::RoundRect(880, y + 8, 104, 24, 6, sel ? RGBA(255, 255, 255, 60) : RGBA(16, 124, 16, 120));
            R::Text(932, y + 12, 2, sel ? COL_SELECTED : COL_LIT, ALIGN_CENTER, "CUSTOM");
        }
        if (g.titleId)
            R::Textf(1176, y + 12, 2, sel ? c : COL_DIM, ALIGN_RIGHT, "%08X", g.titleId);
    }
    // Scrollbar
    if (list.count > GAMELIST_ROWS)
    {
        float h = GAMELIST_ROWS * 44.0f, bar = h * GAMELIST_ROWS / list.count;
        float pos = (h - bar) * list.top / (list.count - GAMELIST_ROWS);
        R::Rect(1200, 132, 4, h, COL_PANEL_EDGE);
        R::Rect(1200, 132 + pos, 4, bar, COL_DIM);
    }

    R::Text(64, 572, 2, COL_TEXT, ALIGN_LEFT, "A: edit    Y: delete profile    B: back");
    R::Text(64, 602, 1, COL_DIM, ALIGN_LEFT,
            "Games appear here once they have been started with the plugin running, or when installed to the hard drive.");
    R::Text(64, 618, 1, COL_DIM, ALIGN_LEFT, allGamesNote);
}

void UI_DrawModal(const char* title, const char* line, const char* keys)
{
    R::Rect(0, 0, SCREEN_W, SCREEN_H, COL_SHADE);
    R::RoundRect(337, 257, 606, 206, 16, COL_ACCENT);
    R::RoundRect(340, 260, 600, 200, 14, COL_PANEL);
    R::Text(640, 285, 3, COL_TEXT, ALIGN_CENTER, title);
    R::Text(640, 345, 1, COL_DIM, ALIGN_CENTER, line);
    R::Text(640, 400, 2, COL_TEXT, ALIGN_CENTER, keys);
}

void UI_TextClipped(float x, float y, float scale, DWORD color, int maxChars, const char* s)
{
    char buf[96];
    int n = (int)strlen(s);
    if (n > maxChars && maxChars > 3)
    {
        memcpy(buf, s, maxChars - 3);
        strcpy(buf + maxChars - 3, "...");
    }
    else
        strncpy(buf, s, sizeof(buf) - 1), buf[sizeof(buf) - 1] = 0;
    R::Text(x, y, scale, color, ALIGN_LEFT, buf);
}

char UI_Nav(NavRepeat& repeat, const XINPUT_GAMEPAD& g, float dt, bool useStick)
{
    SHORT lx = useStick ? g.sThumbLX : 0, ly = useStick ? g.sThumbLY : 0;
    int dir = 0;
    if ((g.wButtons & XINPUT_GAMEPAD_DPAD_UP) || ly > 20000)         dir = 'U';
    else if ((g.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || ly < -20000) dir = 'D';
    else if ((g.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || lx < -20000) dir = 'L';
    else if ((g.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || lx > 20000) dir = 'R';
    if (!dir)
    {
        repeat.dir = 0;
        return 0;
    }
    if (dir != repeat.dir)
    {
        repeat.dir = dir;
        repeat.timer = 0.4f;
        return (char)dir;
    }
    repeat.timer -= dt;
    if (repeat.timer <= 0)
    {
        repeat.timer = 0.08f;
        return (char)dir;
    }
    return 0;
}
