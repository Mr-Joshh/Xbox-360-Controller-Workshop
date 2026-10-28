// deadzone_ui.cpp - see deadzone_ui.h
#include "deadzone_ui.h"
#include "deadzone.h"
#include "remap.h"
#include "gamelist_ui.h"
#include "system.h"
#include "theme.h"
#include "restmeasure.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{
    enum Screen { SCR_LIST, SCR_EDIT, SCR_MEASURE, SCR_CONFIRM_DISCARD, SCR_CONFIRM_DELETE };

    const int   EDIT_ROWS = 2;
    const int   MEASURE_MARGIN = 2;       // percent added above the measured drift (see restmeasure.h)

    Screen          g_Screen;
    GameList        g_List;
    bool            g_PluginLoaded;

    int             g_Game;               // index into g_List.games being edited
    DeadzoneProfile g_Edit, g_Saved;
    int             g_EditSel;
    char            g_Status[96];
    DWORD           g_StatusColor;
    char            g_FileBuf[4 * 1024];
    NavRepeat       g_Repeat;

    RestMeasure     g_Measure;
    bool            g_Measured;

    const XINPUT_STATE* g_Cur;
    const XINPUT_STATE* g_Prev;

    bool Pressed(WORD mask)
    {
        return (g_Cur->Gamepad.wButtons & mask) && !(g_Prev->Gamepad.wButtons & mask);
    }

    void SetStatus(const char* text, DWORD color)
    {
        strncpy(g_Status, text, sizeof(g_Status) - 1);
        g_Status[sizeof(g_Status) - 1] = 0;
        g_StatusColor = color;
    }

    bool Dirty()
    {
        return g_Edit.left != g_Saved.left || g_Edit.right != g_Saved.right;
    }

    float Magnitude(SHORT x, SHORT y)
    {
        float fx = x / 32767.0f, fy = y / 32767.0f;
        return sqrtf(fx * fx + fy * fy);
    }

    // Standard XInput dead zones, the ones most games use, as a fraction of full travel.
    float GameDeadzone(int stick)
    {
        return (stick == 0 ? XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE : XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / 32767.0f;
    }

    int& Percent(DeadzoneProfile& p, int stick)
    {
        return stick == 0 ? p.left : p.right;
    }

    // ----------------------------------------------------------------------------------
    // Actions
    // ----------------------------------------------------------------------------------
    void OpenEditor(int game)
    {
        g_Game = game;
        char path[MAX_PATH];
        Games_ProfilePath(CT_DEADZONE_FOLDER, g_List.games[game].titleId, path, sizeof(path));
        if (Sys_ReadFile(path, g_FileBuf, sizeof(g_FileBuf)) >= 0)
            Deadzone_Parse(g_FileBuf, &g_Edit, NULL, 0);
        else
            Deadzone_Default(&g_Edit);
        g_Saved = g_Edit;
        g_EditSel = 0;
        g_Measured = false;
        g_Status[0] = 0;
        g_Screen = SCR_EDIT;
    }

    void Save()
    {
        GameEntry& game = g_List.games[g_Game];
        char path[MAX_PATH];
        Games_ProfilePath(CT_DEADZONE_FOLDER, game.titleId, path, sizeof(path));
        if (Deadzone_IsDefault(&g_Edit))
        {
            // No dead zone needs no profile; removing it keeps the plugin out of the game.
            Sys_DeleteFile(path);
            game.hasProfile = false;
            g_Saved = g_Edit;
            SetStatus("Dead zones off - profile removed", COL_LIT);
            return;
        }
        Sys_CreateDir(CT_DIR_HDD);
        Sys_CreateDir(CT_DIR_HDD "\\" CT_DEADZONE_FOLDER);
        int n = Deadzone_Write(&g_Edit, game.name, g_FileBuf, sizeof(g_FileBuf));
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
        GameEntry& game = g_List.games[g_List.sel];
        char path[MAX_PATH];
        Games_ProfilePath(CT_DEADZONE_FOLDER, game.titleId, path, sizeof(path));
        Sys_DeleteFile(path);
        game.hasProfile = false;
    }

    void ChangeRow(int dir)
    {
        int& v = Percent(g_Edit, g_EditSel);
        v += dir;
        if (v < 0) v = 0;
        if (v > DZ_MAX_PERCENT) v = DZ_MAX_PERCENT;
        g_Status[0] = 0;
    }

    void StartMeasure()
    {
        RestMeasure_Start(g_Measure);
        g_Screen = SCR_MEASURE;
    }

    // Sets each dead zone just above the furthest either stick rested from center.
    void FinishMeasure()
    {
        bool tooBig = false;
        for (int k = 0; k < 2; k++)
        {
            int pct = (int)ceilf(g_Measure.worst[k] * 100) + MEASURE_MARGIN;
            if (pct > DZ_MAX_PERCENT)
            {
                pct = DZ_MAX_PERCENT;
                tooBig = true;
            }
            Percent(g_Edit, k) = pct;
        }
        g_Measured = true;
        char text[96];
        if (tooBig)
            _snprintf(text, sizeof(text), "Drift too large - set to the %d%% maximum (not saved yet)", DZ_MAX_PERCENT);
        else
            _snprintf(text, sizeof(text), "Set to left %d%%, right %d%% (not saved yet)", g_Edit.left, g_Edit.right);
        SetStatus(text, COL_YELLOW);
        g_Screen = SCR_EDIT;
        g_Repeat.dir = 0;
    }

    // ----------------------------------------------------------------------------------
    // Drawing
    // ----------------------------------------------------------------------------------
    DWORD WithAlpha(DWORD color, DWORD alpha)
    {
        return (color & 0x00FFFFFF) | (alpha << 24);
    }

    void DrawStickPlot(float cx, float cy, float r, int stick, const XINPUT_GAMEPAD& raw, const XINPUT_GAMEPAD& out)
    {
        SHORT rx = stick == 0 ? raw.sThumbLX : raw.sThumbRX, ry = stick == 0 ? raw.sThumbLY : raw.sThumbRY;
        SHORT ox = stick == 0 ? out.sThumbLX : out.sThumbRX, oy = stick == 0 ? out.sThumbLY : out.sThumbRY;
        int pct = Percent(g_Edit, stick);

        R::Circle(cx, cy, r + 4, COL_PANEL_EDGE);
        R::Circle(cx, cy, r, COL_WELL);
        if (pct > 0)
            R::Circle(cx, cy, r * pct / 100.0f, WithAlpha(COL_ORANGE, 90));
        R::Ring(cx, cy, r * GameDeadzone(stick), 1, COL_FAINT);
        R::Line(cx - r, cy, cx + r, cy, 1, COL_FAINT);
        R::Line(cx, cy - r, cx, cy + r, 1, COL_FAINT);
        // Hollow ring = where the stick really is; solid dot = what the game gets.
        R::Ring(cx + rx / 32767.0f * r, cy - ry / 32767.0f * r, 8, 2, COL_DIM);
        R::Circle(cx + ox / 32767.0f * r, cy - oy / 32767.0f * r, 6, COL_LIT);

        R::Text(cx, cy + r + 14, 2, COL_TEXT, ALIGN_CENTER, stick == 0 ? "LEFT STICK" : "RIGHT STICK");
        float rawMag = Magnitude(rx, ry), outMag = Magnitude(ox, oy);
        R::Textf(cx, cy + r + 40, 1, COL_DIM, ALIGN_CENTER, "stick %3.0f%%   game %3.0f%%",
                 (rawMag > 1 ? 1 : rawMag) * 100, (outMag > 1 ? 1 : outMag) * 100);
    }

    void DrawLive(const XINPUT_STATE& cur)
    {
        float x = 680, y = 116, w = 536;
        R::RoundRect(x, y, w, 448, 12, COL_PANEL);
        R::Text(x + w / 2, y + 14, 2, COL_TEXT, ALIGN_CENTER, "WHAT THE GAME SEES");

        XINPUT_GAMEPAD out = cur.Gamepad;
        Deadzone_Apply(&g_Edit, &out);
        for (int k = 0; k < 2; k++)
            DrawStickPlot(x + 136 + k * 264.0f, y + 196, 104, k, cur.Gamepad, out);

        // Legend
        float ly = y + 384;
        R::Ring(x + 40, ly + 4, 6, 2, COL_DIM);
        R::Text(x + 54, ly, 1, COL_DIM, ALIGN_LEFT, "your stick");
        R::Circle(x + 160, ly + 4, 5, COL_LIT);
        R::Text(x + 172, ly, 1, COL_DIM, ALIGN_LEFT, "what the game gets");
        R::Circle(x + 340, ly + 4, 6, WithAlpha(COL_ORANGE, 160));
        R::Text(x + 352, ly, 1, COL_DIM, ALIGN_LEFT, "dead zone");
        R::Ring(x + 40, ly + 26, 6, 1, COL_DIM);
        R::Text(x + 54, ly + 22, 1, COL_DIM, ALIGN_LEFT, "thin ring: the dead zone most games already use");
    }

    void DrawEditor(const XINPUT_STATE& cur)
    {
        const GameEntry& game = g_List.games[g_Game];
        UI_TextClipped(64, 40, 3, COL_TEXT, 46, game.name);
        if (game.titleId)
            R::Textf(64, 74, 1, COL_DIM, ALIGN_LEFT, "Title ID %08X - dead zones used every time this game starts", game.titleId);
        else
            R::Text(64, 74, 1, COL_DIM, ALIGN_LEFT, "Used by every game that has no dead zone profile of its own");

        R::RoundRect(64, 108, 596, 456, 12, COL_PANEL);
        static const char* names[EDIT_ROWS] = { "Left stick", "Right stick" };
        for (int row = 0; row < EDIT_ROWS; row++)
        {
            float y = 116 + row * 40.0f;
            bool sel = row == g_EditSel;
            if (sel)
                R::RoundRect(72, y, 580, 36, 8, COL_ACCENT);
            DWORD base = sel ? COL_SELECTED : COL_TEXT;
            int pct = Percent(g_Edit, row);
            R::Text(90, y + 10, 2, base, ALIGN_LEFT, names[row]);
            char val[24];
            if (pct == 0)
                _snprintf(val, sizeof(val), sel ? "< OFF >" : "OFF");
            else
                _snprintf(val, sizeof(val), sel ? "< %d%% >" : "%d%%", pct);
            R::Text(630, y + 10, 2, pct ? (sel ? base : COL_ORANGE) : base, ALIGN_RIGHT, val);
        }

        // How it works, and the last measurement.
        float tx = 90, ty = 222;
        static const char* help[] =
        {
            "Inside the dead zone the game sees the stick as centered.",
            "Past it, the stick's travel is stretched so movement",
            "starts smoothly where the game's own dead zone ends.",
            "",
            "Pick a size just above your stick's drift, or press Y",
            "to measure it.",
        };
        for (int i = 0; i < (int)(sizeof(help) / sizeof(help[0])); i++)
            R::Text(tx, ty + i * 16.0f, 1, COL_DIM, ALIGN_LEFT, help[i]);

        if (g_Measured)
        {
            float my = ty + 122;
            R::Text(tx, my, 2, COL_TEXT, ALIGN_LEFT, "LAST MEASUREMENT");
            for (int k = 0; k < 2; k++)
            {
                float drift = g_Measure.worst[k] * 100;
                bool games = g_Measure.worst[k] > GameDeadzone(k);
                R::Textf(tx, my + 28 + k * 22.0f, 2, games ? COL_ORANGE : COL_LIT, ALIGN_LEFT, "%s %5.1f%%",
                         k == 0 ? "Left  drift" : "Right drift", drift);
                R::Text(tx + 300, my + 32 + k * 22.0f, 1, COL_DIM, ALIGN_LEFT,
                        games ? "shows up in most games" : "most games ignore this");
            }
        }

        DrawLive(cur);

        R::Text(64, 584, 2, COL_TEXT, ALIGN_LEFT, "D-pad: choose / change  Y: measure drift  X: off  START: save  B: back");
        if (g_Status[0])
            R::Text(64, 614, 2, g_StatusColor, ALIGN_LEFT, g_Status);
        else if (Dirty())
            R::Text(64, 614, 2, COL_YELLOW, ALIGN_LEFT, "Unsaved changes - press START to save");
    }
}

void DeadzoneUI_Open()
{
    g_Screen = SCR_LIST;
    g_List.sel = 0;
    g_List.top = 0;
    g_Repeat.dir = 0;
    GameList_Load(g_List, CT_DEADZONE_FOLDER);
    g_PluginLoaded = Sys_PluginLoaded();
}

bool DeadzoneUI_Update(const XINPUT_STATE& cur, const XINPUT_STATE& prev, float dt)
{
    g_Cur = &cur;
    g_Prev = &prev;

    switch (g_Screen)
    {
    case SCR_LIST:
        GameList_Move(g_List, UI_Nav(g_Repeat, cur.Gamepad, dt, true));
        if (Pressed(XINPUT_GAMEPAD_A))
            OpenEditor(g_List.sel);
        else if (Pressed(XINPUT_GAMEPAD_Y) && g_List.games[g_List.sel].hasProfile)
            g_Screen = SCR_CONFIRM_DELETE;
        else if (Pressed(XINPUT_GAMEPAD_B))
            return false;
        break;
    case SCR_EDIT:
    {
        // D-pad only: the sticks are left free to try the dead zones.
        char nav = UI_Nav(g_Repeat, cur.Gamepad, dt, false);
        if (nav == 'U') g_EditSel = (g_EditSel + EDIT_ROWS - 1) % EDIT_ROWS;
        if (nav == 'D') g_EditSel = (g_EditSel + 1) % EDIT_ROWS;
        if (nav == 'L') ChangeRow(-1);
        if (nav == 'R') ChangeRow(1);
        if (Pressed(XINPUT_GAMEPAD_X))
        {
            Deadzone_Default(&g_Edit);
            SetStatus("Dead zones off (not saved yet)", COL_YELLOW);
        }
        if (Pressed(XINPUT_GAMEPAD_Y))
            StartMeasure();
        if (Pressed(XINPUT_GAMEPAD_START))
            Save();
        if (Pressed(XINPUT_GAMEPAD_B))
            g_Screen = Dirty() ? SCR_CONFIRM_DISCARD : SCR_LIST;
        break;
    }
    case SCR_MEASURE:
        if (Pressed(XINPUT_GAMEPAD_B))
        {
            g_Screen = SCR_EDIT;
            break;
        }
        if (RestMeasure_Update(g_Measure, cur.Gamepad, dt))
            FinishMeasure();
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

void DeadzoneUI_Draw(const XINPUT_STATE& cur)
{
    if (g_Screen == SCR_LIST || g_Screen == SCR_CONFIRM_DELETE)
    {
        GameList_Draw(g_List, "DEAD ZONE CONFIGURATION", g_PluginLoaded,
                      "\"All games\" applies to every game that has no dead zone profile of its own.");
        if (g_Screen == SCR_CONFIRM_DELETE)
            UI_DrawModal("DELETE PROFILE?", g_List.games[g_List.sel].name, "A: delete    B: cancel");
    }
    else
    {
        // Measuring shows the editor at rest behind the box.
        XINPUT_STATE shown = cur;
        if (g_Screen == SCR_MEASURE)
            ZeroMemory(&shown.Gamepad, sizeof(shown.Gamepad));
        DrawEditor(shown);
        if (g_Screen == SCR_MEASURE)
            RestMeasure_Draw(g_Measure, "MEASURING DRIFT");
        if (g_Screen == SCR_CONFIRM_DISCARD)
            UI_DrawModal("DISCARD CHANGES?", "Your dead zone changes have not been saved.", "A: discard    B: keep editing");
    }
}
