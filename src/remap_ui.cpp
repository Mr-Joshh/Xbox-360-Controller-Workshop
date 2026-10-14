// remap_ui.cpp - see remap_ui.h
#include "remap_ui.h"
#include "remap.h"
#include "games.h"
#include "system.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

namespace
{
    enum Screen { SCR_LIST, SCR_EDIT, SCR_TRY, SCR_CONFIRM_DISCARD, SCR_CONFIRM_DELETE };

    const int MAX_GAMES = 256;
    const int LIST_ROWS = 9;
    const int OPT_SWAP = IN_COUNT, OPT_INV_LY = IN_COUNT + 1, OPT_INV_RY = IN_COUNT + 2;
    const int EDIT_ROWS = IN_COUNT + 3;

    Screen       g_Screen;
    GameEntry    g_Games[MAX_GAMES];
    int          g_GameCount;
    int          g_ListSel, g_ListTop;
    bool         g_PluginLoaded;

    int          g_Game;            // index into g_Games being edited
    RemapProfile g_Edit, g_Saved;
    int          g_EditSel;
    float        g_BackHold;
    char         g_Status[96];
    DWORD        g_StatusColor;
    char         g_FileBuf[8 * 1024];

    int          g_RepeatDir;
    float        g_RepeatTimer;

    // ----------------------------------------------------------------------------------
    // Input helpers
    // ----------------------------------------------------------------------------------
    const XINPUT_STATE* g_Cur;
    const XINPUT_STATE* g_Prev;

    bool Pressed(WORD mask)
    {
        return (g_Cur->Gamepad.wButtons & mask) && !(g_Prev->Gamepad.wButtons & mask);
    }

    // D-pad / left stick with key repeat. Returns 'U', 'D', 'L', 'R' or 0.
    char Nav(float dt)
    {
        const XINPUT_GAMEPAD& g = g_Cur->Gamepad;
        int dir = 0;
        if ((g.wButtons & XINPUT_GAMEPAD_DPAD_UP) || g.sThumbLY > 20000)         dir = 'U';
        else if ((g.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || g.sThumbLY < -20000) dir = 'D';
        else if ((g.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || g.sThumbLX < -20000) dir = 'L';
        else if ((g.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || g.sThumbLX > 20000) dir = 'R';
        if (!dir)
        {
            g_RepeatDir = 0;
            return 0;
        }
        if (dir != g_RepeatDir)
        {
            g_RepeatDir = dir;
            g_RepeatTimer = 0.4f;
            return (char)dir;
        }
        g_RepeatTimer -= dt;
        if (g_RepeatTimer <= 0)
        {
            g_RepeatTimer = 0.08f;
            return (char)dir;
        }
        return 0;
    }

    bool Down(const XINPUT_GAMEPAD& g, int input)
    {
        if (input == IN_LT) return g.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        if (input == IN_RT) return g.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        static const WORD masks[IN_LT] =
        {
            XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
            XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
            XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
            XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
            XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT
        };
        return (g.wButtons & masks[input]) != 0;
    }

    void SetStatus(const char* text, DWORD color)
    {
        strncpy(g_Status, text, sizeof(g_Status) - 1);
        g_Status[sizeof(g_Status) - 1] = 0;
        g_StatusColor = color;
    }

    bool Dirty()
    {
        return memcmp(g_Edit.target, g_Saved.target, sizeof(g_Edit.target)) != 0 ||
               g_Edit.swapSticks != g_Saved.swapSticks || g_Edit.invertLY != g_Saved.invertLY ||
               g_Edit.invertRY != g_Saved.invertRY;
    }

    // ----------------------------------------------------------------------------------
    // Actions
    // ----------------------------------------------------------------------------------
    void Reload()
    {
        g_GameCount = Games_Load(g_Games, MAX_GAMES);
        g_PluginLoaded = Sys_PluginLoaded();
        if (g_ListSel >= g_GameCount)
            g_ListSel = g_GameCount - 1;
    }

    void OpenEditor(int game)
    {
        g_Game = game;
        char path[MAX_PATH];
        Games_ProfilePath(g_Games[game].titleId, path, sizeof(path));
        if (Sys_ReadFile(path, g_FileBuf, sizeof(g_FileBuf)) >= 0)
            Remap_Parse(g_FileBuf, &g_Edit, NULL, 0);
        else
            Remap_Default(&g_Edit);
        g_Saved = g_Edit;
        g_EditSel = 0;
        g_Status[0] = 0;
        g_Screen = SCR_EDIT;
    }

    void Save()
    {
        GameEntry& game = g_Games[g_Game];
        char path[MAX_PATH];
        Games_ProfilePath(game.titleId, path, sizeof(path));
        if (Remap_IsDefault(&g_Edit))
        {
            // A default layout needs no profile; removing it keeps the plugin out of the game.
            Sys_DeleteFile(path);
            game.hasProfile = false;
            g_Saved = g_Edit;
            SetStatus("Default layout - profile removed", COL_LIT);
            return;
        }
        Sys_CreateDir(CT_DIR_HDD);
        Sys_CreateDir(CT_DIR_HDD "\\Remap");
        int n = Remap_Write(&g_Edit, game.name, g_FileBuf, sizeof(g_FileBuf));
        if (Sys_WriteFile(path, g_FileBuf, n))
        {
            game.hasProfile = true;
            g_Saved = g_Edit;
            SetStatus("Saved - used the next time the game starts", COL_LIT);
        }
        else
            SetStatus("Could not write the profile to the hard drive", COL_RED);
    }

    void DeleteProfile()
    {
        GameEntry& game = g_Games[g_ListSel];
        char path[MAX_PATH];
        Games_ProfilePath(game.titleId, path, sizeof(path));
        Sys_DeleteFile(path);
        game.hasProfile = false;
    }

    void ChangeRow(int dir)
    {
        if (g_EditSel < IN_COUNT)
        {
            // Cycle through every input, then NONE.
            signed char& t = g_Edit.target[g_EditSel];
            int idx = t == MAP_NONE ? IN_COUNT : t;
            idx = (idx + dir + IN_COUNT + 1) % (IN_COUNT + 1);
            t = (signed char)(idx == IN_COUNT ? MAP_NONE : idx);
        }
        else if (g_EditSel == OPT_SWAP)   g_Edit.swapSticks = !g_Edit.swapSticks;
        else if (g_EditSel == OPT_INV_LY) g_Edit.invertLY = !g_Edit.invertLY;
        else if (g_EditSel == OPT_INV_RY) g_Edit.invertRY = !g_Edit.invertRY;
        g_Status[0] = 0;
    }

    // ----------------------------------------------------------------------------------
    // Drawing helpers
    // ----------------------------------------------------------------------------------
    void TextClipped(float x, float y, float scale, DWORD color, int maxChars, const char* s)
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

    void DrawModal(const char* title, const char* line, const char* keys)
    {
        R::Rect(0, 0, SCREEN_W, SCREEN_H, COL_SHADE);
        R::RoundRect(337, 257, 606, 206, 16, COL_ACCENT);
        R::RoundRect(340, 260, 600, 200, 14, COL_PANEL);
        R::Text(640, 285, 3, COL_TEXT, ALIGN_CENTER, title);
        R::Text(640, 345, 1, COL_DIM, ALIGN_CENTER, line);
        R::Text(640, 400, 2, COL_TEXT, ALIGN_CENTER, keys);
    }

    void DrawList()
    {
        R::Text(64, 40, 3, COL_TEXT, ALIGN_LEFT, "BUTTON REMAPPER");
        if (g_PluginLoaded)
            R::Text(64, 80, 2, COL_LIT, ALIGN_LEFT, "Plugin running - profiles load automatically when a game starts");
        else
        {
            R::Text(64, 80, 2, COL_YELLOW, ALIGN_LEFT, "Plugin not running - profiles won't be applied yet");
            R::Text(64, 102, 1, COL_DIM, ALIGN_LEFT,
                    "Add Hdd:\\ControllerWorkshop\\ControllerWorkshopPlugin.xex as a DashLaunch plugin and restart the console (see README).");
        }

        R::RoundRect(64, 124, 1152, LIST_ROWS * 44 + 16, 12, COL_PANEL);
        if (g_ListSel < g_ListTop) g_ListTop = g_ListSel;
        if (g_ListSel >= g_ListTop + LIST_ROWS) g_ListTop = g_ListSel - LIST_ROWS + 1;
        for (int r = 0; r < LIST_ROWS && g_ListTop + r < g_GameCount; r++)
        {
            int i = g_ListTop + r;
            const GameEntry& g = g_Games[i];
            float y = 132 + r * 44.0f;
            bool sel = i == g_ListSel;
            if (sel)
                R::RoundRect(72, y, 1120, 40, 8, COL_ACCENT);
            DWORD c = sel ? COL_SELECTED : COL_TEXT;
            TextClipped(90, y + 12, 2, c, 46, g.name);
            if (g.hasProfile)
            {
                R::RoundRect(880, y + 8, 104, 24, 6, sel ? RGBA(255, 255, 255, 60) : RGBA(16, 124, 16, 120));
                R::Text(932, y + 12, 2, sel ? COL_SELECTED : COL_LIT, ALIGN_CENTER, "CUSTOM");
            }
            if (g.titleId)
                R::Textf(1176, y + 12, 2, sel ? c : COL_DIM, ALIGN_RIGHT, "%08X", g.titleId);
        }
        // Scrollbar
        if (g_GameCount > LIST_ROWS)
        {
            float h = LIST_ROWS * 44.0f, bar = h * LIST_ROWS / g_GameCount;
            float pos = (h - bar) * g_ListTop / (g_GameCount - LIST_ROWS);
            R::Rect(1200, 132, 4, h, COL_PANEL_EDGE);
            R::Rect(1200, 132 + pos, 4, bar, COL_DIM);
        }

        R::Text(64, 572, 2, COL_TEXT, ALIGN_LEFT, "A: edit    Y: delete profile    B: back");
        R::Text(64, 602, 1, COL_DIM, ALIGN_LEFT,
                "Games appear here once they have been started with the plugin running, or when installed to the hard drive.");
        R::Text(64, 618, 1, COL_DIM, ALIGN_LEFT,
                "\"All games\" applies to every game that has no profile of its own.");
    }

    const char* TargetName(signed char t)
    {
        return t == MAP_NONE ? "NONE" : g_InputNames[t];
    }

    void DrawLiveTest(const XINPUT_STATE& cur, bool active)
    {
        float x = 680, y = 116, w = 536;
        R::RoundRect(x, y, w, 448, 12, active ? COL_TRY_PANEL : COL_PANEL);
        R::Text(x + w / 2, y + 14, 2, COL_TEXT, ALIGN_CENTER, active ? "TRYING IT OUT" : "WHAT THE GAME SEES");
        R::Text(x + w / 2, y + 38, 1, COL_DIM, ALIGN_CENTER,
                active ? "Press anything. Hold BACK for 1 second to stop." : "Press Y to try this layout with every button.");

        XINPUT_GAMEPAD out = cur.Gamepad;
        Remap_Apply(&g_Edit, &out);

        // Output buttons, 4 x 4
        for (int i = 0; i < IN_COUNT; i++)
        {
            float tx = x + 24 + (i % 4) * 124.0f, ty = y + 62 + (i / 4) * 52.0f;
            bool on = Down(out, i);
            R::RoundRect(tx, ty, 116, 44, 8, on ? COL_LIT : COL_PANEL_EDGE);
            R::Text(tx + 58, ty + 14, 2, on ? COL_ON_LIT : COL_DIM, ALIGN_CENTER, g_InputNames[i]);
        }
        if (out.bLeftTrigger || out.bRightTrigger)
            R::Textf(x + w / 2, y + 274, 1, COL_DIM, ALIGN_CENTER, "LT %d   RT %d", out.bLeftTrigger, out.bRightTrigger);

        // Output sticks
        for (int k = 0; k < 2; k++)
        {
            float cx = x + 150 + k * 236.0f, cy = y + 360, r = 56;
            SHORT sx = k == 0 ? out.sThumbLX : out.sThumbRX, sy = k == 0 ? out.sThumbLY : out.sThumbRY;
            R::Circle(cx, cy, r + 4, COL_PANEL_EDGE);
            R::Circle(cx, cy, r, COL_WELL);
            R::Line(cx - r, cy, cx + r, cy, 1, COL_FAINT);
            R::Line(cx, cy - r, cx, cy + r, 1, COL_FAINT);
            R::Circle(cx + sx / 32767.0f * r, cy - sy / 32767.0f * r, 7, COL_LIT);
            R::Text(cx, cy + r + 10, 1, COL_DIM, ALIGN_CENTER, k == 0 ? "LEFT STICK" : "RIGHT STICK");
        }
    }

    void DrawEditor(const XINPUT_STATE& cur)
    {
        const GameEntry& game = g_Games[g_Game];
        TextClipped(64, 40, 3, COL_TEXT, 46, game.name);
        if (game.titleId)
            R::Textf(64, 74, 1, COL_DIM, ALIGN_LEFT, "Title ID %08X - profile used every time this game starts", game.titleId);
        else
            R::Text(64, 74, 1, COL_DIM, ALIGN_LEFT, "Used by every game that has no profile of its own");
        R::Text(64, 92, 1, COL_DIM, ALIGN_LEFT, "BUTTON YOU PRESS");
        R::Text(330, 92, 1, COL_DIM, ALIGN_LEFT, "WHAT THE GAME GETS");

        XINPUT_GAMEPAD out = cur.Gamepad;
        Remap_Apply(&g_Edit, &out);

        R::RoundRect(64, 108, 596, EDIT_ROWS * 24 + 12, 12, COL_PANEL);
        for (int row = 0; row < EDIT_ROWS; row++)
        {
            float y = 114 + row * 24.0f;
            bool sel = row == g_EditSel && g_Screen != SCR_TRY;
            if (sel)
                R::RoundRect(70, y, 584, 24, 6, COL_ACCENT);
            DWORD base = sel ? COL_SELECTED : COL_TEXT;
            DWORD litRow = sel ? RGBA(190, 255, 170, 255) : COL_LIT;   // readable on the green bar
            if (row < IN_COUNT)
            {
                bool changed = g_Edit.target[row] != row;
                R::Text(90, y + 4, 2, Down(cur.Gamepad, row) ? litRow : base, ALIGN_LEFT, g_InputNames[row]);
                R::Text(250, y + 4, 2, COL_DIM, ALIGN_LEFT, "->");
                DWORD tc = changed ? (sel ? RGBA(255, 220, 150, 255) : COL_ORANGE) : base;
                if (g_Edit.target[row] != MAP_NONE && Down(out, g_Edit.target[row]) && Down(cur.Gamepad, row))
                    tc = litRow;
                R::Text(330, y + 4, 2, tc, ALIGN_LEFT, TargetName(g_Edit.target[row]));
                if (sel)
                    R::Text(630, y + 4, 2, base, ALIGN_RIGHT, "< >");
            }
            else
            {
                static const char* labels[3] = { "Swap left and right sticks", "Invert left stick up/down",
                                                  "Invert right stick up/down" };
                bool on = row == OPT_SWAP ? g_Edit.swapSticks : row == OPT_INV_LY ? g_Edit.invertLY : g_Edit.invertRY;
                R::Text(90, y + 4, 2, base, ALIGN_LEFT, labels[row - IN_COUNT]);
                R::Text(630, y + 4, 2, on ? (sel ? base : COL_ORANGE) : base, ALIGN_RIGHT, on ? "ON" : "OFF");
            }
        }

        DrawLiveTest(cur, g_Screen == SCR_TRY);

        if (g_Screen == SCR_TRY)
        {
            if (g_BackHold > 0.05f)
                R::Rect(680, 570, 536 * (g_BackHold > 1 ? 1 : g_BackHold), 6, COL_TEXT);
        }
        else
        {
            R::Text(64, 584, 2, COL_TEXT, ALIGN_LEFT, "D-pad: choose / change   X: reset   Y: try it   START: save   B: back");
        }
        if (g_Status[0])
            R::Text(64, 614, 2, g_StatusColor, ALIGN_LEFT, g_Status);
        else if (Dirty())
            R::Text(64, 614, 2, COL_YELLOW, ALIGN_LEFT, "Unsaved changes - press START to save");
    }
}

void RemapUI_Open()
{
    g_Screen = SCR_LIST;
    g_ListSel = 0;
    g_ListTop = 0;
    g_RepeatDir = 0;
    Reload();
}

bool RemapUI_Update(const XINPUT_STATE& cur, const XINPUT_STATE& prev, float dt)
{
    g_Cur = &cur;
    g_Prev = &prev;

    switch (g_Screen)
    {
    case SCR_LIST:
    {
        char nav = Nav(dt);
        if (nav == 'U') g_ListSel = (g_ListSel + g_GameCount - 1) % g_GameCount;
        if (nav == 'D') g_ListSel = (g_ListSel + 1) % g_GameCount;
        if (nav == 'L') g_ListSel = g_ListSel > LIST_ROWS ? g_ListSel - LIST_ROWS : 0;
        if (nav == 'R') g_ListSel = g_ListSel + LIST_ROWS < g_GameCount ? g_ListSel + LIST_ROWS : g_GameCount - 1;
        if (Pressed(XINPUT_GAMEPAD_A))
            OpenEditor(g_ListSel);
        else if (Pressed(XINPUT_GAMEPAD_Y) && g_Games[g_ListSel].hasProfile)
            g_Screen = SCR_CONFIRM_DELETE;
        else if (Pressed(XINPUT_GAMEPAD_B))
            return false;
        break;
    }
    case SCR_EDIT:
    {
        char nav = Nav(dt);
        if (nav == 'U') g_EditSel = (g_EditSel + EDIT_ROWS - 1) % EDIT_ROWS;
        if (nav == 'D') g_EditSel = (g_EditSel + 1) % EDIT_ROWS;
        if (nav == 'L') ChangeRow(-1);
        if (nav == 'R') ChangeRow(1);
        if (Pressed(XINPUT_GAMEPAD_A) && g_EditSel >= IN_COUNT)
            ChangeRow(1);
        if (Pressed(XINPUT_GAMEPAD_X))
        {
            Remap_Default(&g_Edit);
            SetStatus("Reset to the normal layout (not saved yet)", COL_YELLOW);
        }
        if (Pressed(XINPUT_GAMEPAD_Y))
        {
            g_Screen = SCR_TRY;
            g_BackHold = 0;
        }
        if (Pressed(XINPUT_GAMEPAD_START))
            Save();
        if (Pressed(XINPUT_GAMEPAD_B))
            g_Screen = Dirty() ? SCR_CONFIRM_DISCARD : SCR_LIST;
        break;
    }
    case SCR_TRY:
        g_BackHold = (cur.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? g_BackHold + dt : 0.0f;
        if (g_BackHold >= 1.0f)
        {
            g_Screen = SCR_EDIT;
            g_RepeatDir = 0;
        }
        break;
    case SCR_CONFIRM_DISCARD:
        if (Pressed(XINPUT_GAMEPAD_A)) g_Screen = SCR_LIST;
        if (Pressed(XINPUT_GAMEPAD_B)) g_Screen = SCR_EDIT;
        break;
    case SCR_CONFIRM_DELETE:
        if (Pressed(XINPUT_GAMEPAD_A)) { DeleteProfile(); g_Screen = SCR_LIST; }
        if (Pressed(XINPUT_GAMEPAD_B)) g_Screen = SCR_LIST;
        break;
    }
    return true;
}

void RemapUI_Draw(const XINPUT_STATE& cur)
{
    if (g_Screen == SCR_LIST || g_Screen == SCR_CONFIRM_DELETE)
    {
        DrawList();
        if (g_Screen == SCR_CONFIRM_DELETE)
            DrawModal("DELETE PROFILE?", g_Games[g_ListSel].name, "A: delete    B: cancel");
    }
    else
    {
        DrawEditor(cur);
        if (g_Screen == SCR_CONFIRM_DISCARD)
            DrawModal("DISCARD CHANGES?", "Your changes to this profile have not been saved.", "A: discard    B: keep editing");
    }
}
