// PC layout preview: runs Controller Workshop's drawing code with a scripted controller state
// and writes one SVG per screen. Build with build_preview.bat.
#define main ControllerWorkshop_main
#include "../../src/main.cpp"
#undef main
#include <string>
extern std::string g_Svg;

static void Save(const char* name)
{
    char path[128];
    sprintf(path, "%s%s", Theme_IsLight() ? "light_" : "", name);
    FILE* f = fopen(path, "w");
    fputs(g_Svg.c_str(), f);
    fclose(f);
}

static void DrawAll(Mode mode)
{
    PadState& p = g_Pads[g_Active];
    R::BeginFrame(COL_BG);
    DrawHeader();
    DrawStickPanel(64, "LEFT STICK", p, 0);
    DrawStickPanel(896, "RIGHT STICK", p, 1);
    DrawController(p, g_Active, p.connected);
    DrawChecklist(p);
    DrawFooter();
    if (mode == MODE_MENU)   DrawMenu();
    if (mode == MODE_RUMBLE) DrawRumble(p);
    if (mode == MODE_DRIFT)  DrawDrift();
    if (mode == MODE_DRIFT_RESULT) DrawDriftResult(p);
    R::EndFrame();
}

int main(int argc, char** argv)
{
    Theme_Set(argc > 1 && strcmp(argv[1], "light") == 0);
    InitControllerShape();
    PadState& p = g_Pads[0];
    p.connected = true;
    p.caps.SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
    p.caps.Flags = XINPUT_CAPS_WIRELESS;
    p.caps.Vibration.wLeftMotorSpeed = p.caps.Vibration.wRightMotorSpeed = 0xFFFF;
    g_Pads[1].connected = true;
    p.batteryKnown = GetBatteryLevel(0, &p.battery);

    // Spin the left stick around the edge (weak toward the top-left) and part of the right.
    for (int i = 0; i < 400; i++)
    {
        float a = i * 0.05f;
        float r = (cosf(a) < 0 && sinf(a) > 0) ? 0.9f : 1.0f;
        p.prev = p.state;
        p.state.Gamepad.sThumbLX = (SHORT)(cosf(a) * r * 32000);
        p.state.Gamepad.sThumbLY = (SHORT)(sinf(a) * r * 32000);
        p.state.Gamepad.sThumbRX = (SHORT)(cosf(a * 0.3f) * 30000);
        p.state.Gamepad.sThumbRY = (SHORT)(sinf(a * 0.3f) * 30000);
        p.state.Gamepad.wButtons = (WORD)((i % 7 == 0) ? (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X |
            XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_START) : 0);
        p.state.Gamepad.bLeftTrigger = (BYTE)(i < 200 ? i : 0);
        UpdateStats(p);
    }
    p.stick[1].restMeasured = true; p.stick[1].restMax = 0.31f;
    p.stick[0].restMeasured = true; p.stick[0].restMax = 0.04f;

    // Live frame: sticks slightly off center, A + RB + D-pad left + RS held.
    p.prev = p.state;
    p.state.Gamepad.sThumbLX = 14000; p.state.Gamepad.sThumbLY = 9000;
    p.state.Gamepad.sThumbRX = -2500; p.state.Gamepad.sThumbRY = 1800;
    p.state.Gamepad.wButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_RIGHT_SHOULDER |
                               XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_RIGHT_THUMB;
    p.state.Gamepad.bLeftTrigger = 120; p.state.Gamepad.bRightTrigger = 255;
    p.trigMax[1] = 255;
    g_BackHold = 0.6f;


    DrawAll(MODE_TEST);   Save("preview_test.svg");
    g_ToastTime = 0; g_BackHold = 0;
    g_MenuSel = MI_DRIFT;
    DrawAll(MODE_MENU);   Save("preview_menu.svg");
    g_RumbleL = 0.47f; g_RumbleR = 1.0f;
    DrawAll(MODE_RUMBLE); Save("preview_rumble.svg");
    g_DriftTimer = 0.7f;
    DrawAll(MODE_DRIFT);  Save("preview_drift.svg");
    p.stick[0].restX = 0.012f; p.stick[0].restY = -0.035f;
    p.stick[1].restX = -0.22f; p.stick[1].restY = 0.21f;
    p.trigRestMeasured = true; p.trigRest[1] = 41;
    DrawAll(MODE_DRIFT_RESULT); Save("preview_drift_result.svg");

    // Remapper: list, editor with a few changes, try-it mode.
    XINPUT_STATE none = {}, cur = {};
    RemapUI_Open();
    R::BeginFrame(COL_BG); RemapUI_Draw(none); R::EndFrame(); Save("preview_remap_list.svg");
    cur.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_DOWN; RemapUI_Update(cur, none, 0.016f);   // -> Assassin's Creed II
    RemapUI_Update(none, cur, 0.016f);
    for (int i = 0; i < 5; i++) { RemapUI_Update(cur, none, 0.016f); RemapUI_Update(none, cur, 0.016f); }  // -> Halo 3
    cur.Gamepad.wButtons = XINPUT_GAMEPAD_A; RemapUI_Update(cur, none, 0.016f); RemapUI_Update(none, cur, 0.016f);
    cur.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_RIGHT; RemapUI_Update(cur, none, 0.016f); RemapUI_Update(none, cur, 0.016f);
    XINPUT_STATE held = {};
    held.Gamepad.wButtons = XINPUT_GAMEPAD_A;
    held.Gamepad.bLeftTrigger = 200;
    held.Gamepad.sThumbRY = 25000;
    R::BeginFrame(COL_BG); RemapUI_Draw(held); R::EndFrame(); Save("preview_remap_edit.svg");
    cur.Gamepad.wButtons = XINPUT_GAMEPAD_Y; RemapUI_Update(cur, none, 0.016f);
    R::BeginFrame(COL_BG); RemapUI_Draw(held); R::EndFrame(); Save("preview_remap_try.svg");
    return 0;
}
