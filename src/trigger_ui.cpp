// trigger_ui.cpp - see trigger_ui.h
#include "trigger_ui.h"
#include "trigger.h"
#include "remap.h"
#include "gamelist_ui.h"
#include "system.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

namespace
{
    enum Screen { SCR_LIST, SCR_EDIT, SCR_MEASURE, SCR_CONFIRM_DISCARD, SCR_CONFIRM_DELETE };

    const int   EDIT_ROWS = 4;        // LT start, LT full, RT start, RT full

    // Measurement: each trigger is pulled all the way and let go. The highest value gives the
    // full press point, the value it rests at afterwards gives the start point.
    enum MeasurePhase { MP_PULL, MP_HOLD, MP_SETTLE, MP_SAMPLE };
    const int   MEASURE_PULLED = 128;     // a pull has to get at least this far to count
    const float MEASURE_SETTLE = 0.3f;    // seconds for the trigger to spring back
    const float MEASURE_SAMPLE = 0.7f;    // seconds of sampling the resting value
    const int   MEASURE_FULL_MARGIN = 3;  // full press point this far below the highest value
    const int   MEASURE_START_MARGIN = 4; // start point this far above the resting value
    const char* const g_TrigNames[2] = { "LT", "RT" };

    Screen          g_Screen;
    GameList        g_List;
    bool            g_PluginLoaded;

    int             g_Game;               // index into g_List.games being edited
    TriggerProfile  g_Edit, g_Saved;
    int             g_EditSel;
    char            g_Status[96];
    DWORD           g_StatusColor;
    char            g_FileBuf[4 * 1024];
    NavRepeat       g_Repeat;
    int             g_RepeatCount;        // repeats of the held direction, to speed up changes
    BYTE            g_Highest[2];         // highest value seen in the editor

    int             g_MeasureStep;        // 0 = LT, 1 = RT
    MeasurePhase    g_MeasurePhase;
    float           g_MeasureTimer;
    BYTE            g_MeasurePeak[2];
    BYTE            g_MeasureRest[2];
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
        return memcmp(&g_Edit, &g_Saved, sizeof(g_Edit)) != 0;
    }

    BYTE TriggerValue(const XINPUT_GAMEPAD& g, int t)
    {
        return t == 0 ? g.bLeftTrigger : g.bRightTrigger;
    }

    // ----------------------------------------------------------------------------------
    // Actions
    // ----------------------------------------------------------------------------------
    void OpenEditor(int game)
    {
        g_Game = game;
        char path[MAX_PATH];
        Games_ProfilePath(CT_TRIGGER_FOLDER, g_List.games[game].titleId, path, sizeof(path));
        if (Sys_ReadFile(path, g_FileBuf, sizeof(g_FileBuf)) >= 0)
            Trigger_Parse(g_FileBuf, &g_Edit, NULL, 0);
        else
            Trigger_Default(&g_Edit);
        g_Saved = g_Edit;
        g_EditSel = 0;
        g_Measured = false;
        g_Highest[0] = g_Highest[1] = 0;
        g_Status[0] = 0;
        g_Screen = SCR_EDIT;
    }

    void Save()
    {
        GameEntry& game = g_List.games[g_Game];
        char path[MAX_PATH];
        Games_ProfilePath(CT_TRIGGER_FOLDER, game.titleId, path, sizeof(path));
        if (Trigger_IsDefault(&g_Edit))
        {
            // Full range needs no profile; removing it keeps the plugin out of the game.
            Sys_DeleteFile(path);
            game.hasProfile = false;
            g_Saved = g_Edit;
            SetStatus("Triggers back to normal - profile removed", COL_LIT);
            return;
        }
        Sys_CreateDir(CT_DIR_HDD);
        Sys_CreateDir(CT_DIR_HDD "\\" CT_TRIGGER_FOLDER);
        int n = Trigger_Write(&g_Edit, game.name, g_FileBuf, sizeof(g_FileBuf));
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
        Games_ProfilePath(CT_TRIGGER_FOLDER, game.titleId, path, sizeof(path));
        Sys_DeleteFile(path);
        game.hasProfile = false;
    }

    void ChangeRow(int dir)
    {
        // Holding a direction speeds up after about a second, so 255 steps don't take long.
        int step = g_RepeatCount > 10 ? 5 : 1;
        int t = g_EditSel / 2;
        if (g_EditSel % 2 == 0)
        {
            int& v = g_Edit.start[t];
            v += dir * step;
            if (v < 0) v = 0;
            if (v > TRIG_MAX_START) v = TRIG_MAX_START;
        }
        else
        {
            int& v = g_Edit.full[t];
            v += dir * step;
            if (v < TRIG_MIN_FULL) v = TRIG_MIN_FULL;
            if (v > 255) v = 255;
        }
        g_Status[0] = 0;
    }

    void StartMeasure()
    {
        g_MeasureStep = 0;
        g_MeasurePhase = MP_PULL;
        g_MeasureTimer = 0;
        ZeroMemory(g_MeasurePeak, sizeof(g_MeasurePeak));
        ZeroMemory(g_MeasureRest, sizeof(g_MeasureRest));
        g_Screen = SCR_MEASURE;
    }

    void FinishMeasure()
    {
        for (int t = 0; t < 2; t++)
        {
            int peak = g_MeasurePeak[t], rest = g_MeasureRest[t];
            int full = peak >= 255 - MEASURE_FULL_MARGIN ? 255 : peak - MEASURE_FULL_MARGIN;
            g_Edit.full[t] = full < TRIG_MIN_FULL ? TRIG_MIN_FULL : full;
            int start = rest > 0 ? rest + MEASURE_START_MARGIN : 0;
            g_Edit.start[t] = start > TRIG_MAX_START ? TRIG_MAX_START : start;
        }
        g_Measured = true;
        char text[96];
        _snprintf(text, sizeof(text), "Set to LT %d-%d, RT %d-%d (not saved yet)",
                  g_Edit.start[0], g_Edit.full[0], g_Edit.start[1], g_Edit.full[1]);
        SetStatus(text, COL_YELLOW);
        g_Screen = SCR_EDIT;
        g_Repeat.dir = 0;
    }

    void UpdateMeasure(const XINPUT_GAMEPAD& g, float dt)
    {
        int t = g_MeasureStep;
        BYTE v = TriggerValue(g, t);
        BYTE& peak = g_MeasurePeak[t];
        // Let go = back under half of the highest value (a stuck trigger may not reach 0).
        bool letGo = peak >= MEASURE_PULLED && v < peak / 2;
        switch (g_MeasurePhase)
        {
        case MP_PULL:
        case MP_HOLD:
            if (v > peak)
                peak = v;
            if (peak >= MEASURE_PULLED)
                g_MeasurePhase = MP_HOLD;
            if (letGo)
            {
                g_MeasurePhase = MP_SETTLE;
                g_MeasureTimer = 0;
            }
            break;
        case MP_SETTLE:
        case MP_SAMPLE:
            // Pulled again: keep looking for the highest value, then wait for the next release.
            if (!letGo)
            {
                g_MeasurePhase = MP_HOLD;
                if (v > peak)
                    peak = v;
                break;
            }
            g_MeasureTimer += dt;
            if (g_MeasurePhase == MP_SETTLE)
            {
                if (g_MeasureTimer >= MEASURE_SETTLE)
                {
                    g_MeasurePhase = MP_SAMPLE;
                    g_MeasureTimer = 0;
                    g_MeasureRest[t] = 0;
                }
                break;
            }
            if (v > g_MeasureRest[t])
                g_MeasureRest[t] = v;
            if (g_MeasureTimer >= MEASURE_SAMPLE)
            {
                if (++g_MeasureStep == 2)
                    FinishMeasure();
                else
                    g_MeasurePhase = MP_PULL;
            }
            break;
        }
    }

    // ----------------------------------------------------------------------------------
    // Drawing
    // ----------------------------------------------------------------------------------
    DWORD WithAlpha(DWORD color, DWORD alpha)
    {
        return (color & 0x00FFFFFF) | (alpha << 24);
    }

    // A 0-255 bar. With 'marks' it also shows the ignored start range and the full press point.
    void DrawTriggerBar(float x, float y, float w, float h, int value, DWORD color, int t, bool marks)
    {
        R::Rect(x, y, w, h, COL_WELL);
        if (marks && g_Edit.start[t] > 0)
            R::Rect(x, y, w * g_Edit.start[t] / 255.0f, h, WithAlpha(COL_ORANGE, 90));
        R::Rect(x, y, w * value / 255.0f, h, color);
        R::RectOutline(x, y, w, h, 2, COL_PANEL_EDGE);
        if (marks)
        {
            if (g_Edit.start[t] > 0)
                R::Rect(x + w * g_Edit.start[t] / 255.0f - 1, y - 4, 2, h + 8, COL_ORANGE);
            if (g_Edit.full[t] < 255)
                R::Rect(x + w * g_Edit.full[t] / 255.0f - 1, y - 4, 3, h + 8, COL_CYAN);
        }
    }

    void DrawLive(const XINPUT_STATE& cur)
    {
        float x = 680, y = 116, w = 536;
        R::RoundRect(x, y, w, 448, 12, COL_PANEL);
        R::Text(x + w / 2, y + 14, 2, COL_TEXT, ALIGN_CENTER, "WHAT THE GAME SEES");

        float bx = x + 24, bw = w - 48;
        for (int t = 0; t < 2; t++)
        {
            float ty = y + 56 + t * 168.0f;
            BYTE raw = TriggerValue(cur.Gamepad, t);
            BYTE out = Trigger_Map(&g_Edit, t, raw);
            R::Text(bx, ty, 2, COL_TEXT, ALIGN_LEFT, t == 0 ? "LEFT TRIGGER" : "RIGHT TRIGGER");
            R::Textf(bx + bw, ty + 4, 1, COL_DIM, ALIGN_RIGHT, "highest seen %d", g_Highest[t]);

            R::Text(bx, ty + 30, 1, COL_DIM, ALIGN_LEFT, "your trigger");
            R::Textf(bx + bw, ty + 30, 1, COL_TEXT, ALIGN_RIGHT, "%d", raw);
            DrawTriggerBar(bx, ty + 44, bw, 22, raw, COL_ORANGE, t, true);

            R::Text(bx, ty + 82, 1, COL_DIM, ALIGN_LEFT, "what the game gets");
            R::Textf(bx + bw, ty + 82, 1, COL_TEXT, ALIGN_RIGHT, "%d", out);
            DrawTriggerBar(bx, ty + 96, bw, 22, out, out > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? COL_LIT : COL_ORANGE, t, false);
        }

        // Legend
        float ly = y + 392;
        R::Rect(x + 34, ly - 2, 12, 12, WithAlpha(COL_ORANGE, 160));
        R::Text(x + 54, ly, 1, COL_DIM, ALIGN_LEFT, "ignored (below the start point)");
        R::Rect(x + 39, ly + 18, 3, 14, COL_CYAN);
        R::Text(x + 54, ly + 22, 1, COL_DIM, ALIGN_LEFT, "full press point: from here the game gets 255");
    }

    void DrawEditor(const XINPUT_STATE& cur)
    {
        const GameEntry& game = g_List.games[g_Game];
        UI_TextClipped(64, 40, 3, COL_TEXT, 46, game.name);
        if (game.titleId)
            R::Textf(64, 74, 1, COL_DIM, ALIGN_LEFT, "Title ID %08X - trigger ranges used every time this game starts", game.titleId);
        else
            R::Text(64, 74, 1, COL_DIM, ALIGN_LEFT, "Used by every game that has no trigger profile of its own");

        R::RoundRect(64, 108, 596, 456, 12, COL_PANEL);
        static const char* names[EDIT_ROWS] = { "LT start point", "LT full press", "RT start point", "RT full press" };
        for (int row = 0; row < EDIT_ROWS; row++)
        {
            float y = 116 + row * 40.0f;
            bool sel = row == g_EditSel;
            if (sel)
                R::RoundRect(72, y, 580, 36, 8, COL_ACCENT);
            DWORD base = sel ? COL_SELECTED : COL_TEXT;
            int t = row / 2;
            bool start = row % 2 == 0;
            int v = start ? g_Edit.start[t] : g_Edit.full[t];
            bool changed = start ? v != 0 : v != 255;
            R::Text(90, y + 10, 2, base, ALIGN_LEFT, names[row]);
            char val[24];
            _snprintf(val, sizeof(val), sel ? "< %d >" : "%d", v);
            R::Text(630, y + 10, 2, changed && !sel ? (start ? COL_ORANGE : COL_CYAN) : base, ALIGN_RIGHT, val);
        }

        float tx = 90, ty = 290;
        static const char* help[] =
        {
            "Below the start point the game sees the trigger as released.",
            "From the full press point up, it sees the trigger fully pressed.",
            "In between, the travel is stretched to cover 0-255.",
            "",
            "A worn trigger that stops short of 255 never fully presses in a",
            "game: set its full press point just below its highest value, or",
            "press Y to measure both triggers.",
        };
        for (int i = 0; i < (int)(sizeof(help) / sizeof(help[0])); i++)
            R::Text(tx, ty + i * 16.0f, 1, COL_DIM, ALIGN_LEFT, help[i]);

        if (g_Measured)
        {
            float my = ty + 132;
            R::Text(tx, my, 2, COL_TEXT, ALIGN_LEFT, "LAST MEASUREMENT");
            for (int t = 0; t < 2; t++)
            {
                bool shortRange = g_MeasurePeak[t] < 255;
                R::Textf(tx, my + 28 + t * 22.0f, 2, shortRange ? COL_ORANGE : COL_LIT, ALIGN_LEFT,
                         "%s highest %3d  rest %3d", g_TrigNames[t], g_MeasurePeak[t], g_MeasureRest[t]);
            }
        }

        DrawLive(cur);

        R::Text(64, 584, 2, COL_TEXT, ALIGN_LEFT, "D-pad: choose / change  Y: measure  X: reset  START: save  B: back");
        if (g_Status[0])
            R::Text(64, 614, 2, g_StatusColor, ALIGN_LEFT, g_Status);
        else if (Dirty())
            R::Text(64, 614, 2, COL_YELLOW, ALIGN_LEFT, "Unsaved changes - press START to save");
    }

    void DrawMeasure(const XINPUT_STATE& cur)
    {
        R::Rect(0, 0, SCREEN_W, SCREEN_H, COL_SHADE);
        R::RoundRect(317, 227, 646, 266, 16, COL_ACCENT);
        R::RoundRect(320, 230, 640, 260, 14, COL_PANEL);
        R::Text(640, 250, 3, COL_TEXT, ALIGN_CENTER, "MEASURING TRIGGERS");
        int t = g_MeasureStep < 2 ? g_MeasureStep : 1;
        R::Textf(640, 284, 1, COL_DIM, ALIGN_CENTER, "Step %d of 2", t + 1);
        if (g_MeasurePhase == MP_PULL || g_MeasurePhase == MP_HOLD)
        {
            R::Textf(640, 314, 2, COL_YELLOW, ALIGN_CENTER, "Pull %s all the way, then let go", g_TrigNames[t]);
            BYTE v = TriggerValue(cur.Gamepad, t);
            R::Rect(400, 344, 480, 20, COL_WELL);
            R::Rect(400, 344, 480 * v / 255.0f, 20, COL_ORANGE);
            R::Rect(400 + 480 * g_MeasurePeak[t] / 255.0f - 1, 340, 3, 28, COL_CYAN);
            R::RectOutline(400, 344, 480, 20, 2, COL_PANEL_EDGE);
            R::Textf(640, 372, 1, COL_DIM, ALIGN_CENTER, "highest %d", g_MeasurePeak[t]);
        }
        else
        {
            float p = (g_MeasurePhase == MP_SAMPLE ? MEASURE_SETTLE : 0) + g_MeasureTimer;
            p /= MEASURE_SETTLE + MEASURE_SAMPLE;
            R::Text(640, 314, 2, COL_TEXT, ALIGN_CENTER, "Measuring - hands off");
            R::Rect(400, 344, 480, 20, COL_WELL);
            R::Rect(400, 344, 480 * (p > 1 ? 1 : p), 20, COL_LIT);
            R::RectOutline(400, 344, 480, 20, 2, COL_PANEL_EDGE);
        }

        for (int i = 0; i < 2; i++)
        {
            DWORD c = i < g_MeasureStep ? COL_LIT : (i == g_MeasureStep ? COL_TEXT : COL_FAINT);
            R::Text(580 + i * 120.0f, 404, 2, c, ALIGN_CENTER, g_TrigNames[i]);
        }
        R::Text(640, 448, 1, COL_DIM, ALIGN_CENTER, "B: cancel");
    }
}

void TriggerUI_Open()
{
    g_Screen = SCR_LIST;
    g_List.sel = 0;
    g_List.top = 0;
    g_Repeat.dir = 0;
    GameList_Load(g_List, CT_TRIGGER_FOLDER);
    g_PluginLoaded = Sys_PluginLoaded();
}

bool TriggerUI_Update(const XINPUT_STATE& cur, const XINPUT_STATE& prev, float dt)
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
        for (int t = 0; t < 2; t++)
            if (TriggerValue(cur.Gamepad, t) > g_Highest[t])
                g_Highest[t] = TriggerValue(cur.Gamepad, t);
        // D-pad only: the triggers and sticks are left free to try the settings.
        char nav = UI_Nav(g_Repeat, cur.Gamepad, dt, false);
        if (!g_Repeat.dir)
            g_RepeatCount = 0;
        else if (nav)
            g_RepeatCount++;
        if (nav == 'U') g_EditSel = (g_EditSel + EDIT_ROWS - 1) % EDIT_ROWS;
        if (nav == 'D') g_EditSel = (g_EditSel + 1) % EDIT_ROWS;
        if (nav == 'L') ChangeRow(-1);
        if (nav == 'R') ChangeRow(1);
        if (Pressed(XINPUT_GAMEPAD_X))
        {
            Trigger_Default(&g_Edit);
            SetStatus("Full range 0-255 on both triggers (not saved yet)", COL_YELLOW);
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
        UpdateMeasure(cur.Gamepad, dt);
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

void TriggerUI_Draw(const XINPUT_STATE& cur)
{
    if (g_Screen == SCR_LIST || g_Screen == SCR_CONFIRM_DELETE)
    {
        GameList_Draw(g_List, "TRIGGER CONFIGURATION", g_PluginLoaded,
                      "\"All games\" applies to every game that has no trigger profile of its own.");
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
            DrawMeasure(cur);
        if (g_Screen == SCR_CONFIRM_DISCARD)
            UI_DrawModal("DISCARD CHANGES?", "Your trigger changes have not been saved.", "A: discard    B: keep editing");
    }
}
