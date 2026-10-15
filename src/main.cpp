// main.cpp - Controller Workshop for Xbox 360.
// Shows a live controller diagram, detailed stick plots (outer reach, trail),
// trigger bars, a button checklist, a hands-off drift test and a rumble test.
#include <xtl.h>
#include <math.h>
#include <stdio.h>
#include "render.h"
#include "shape.h"
#include "battery.h"
#include "theme.h"
#include "system.h"
#include "remap_ui.h"

const float PI = 3.14159265f;

//--------------------------------------------------------------------------------------
// Buttons
//--------------------------------------------------------------------------------------
enum
{
    BTN_A, BTN_B, BTN_X, BTN_Y, BTN_LB, BTN_RB, BTN_LS, BTN_RS, BTN_BACK, BTN_START,
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_LT, BTN_RT, BTN_COUNT
};

struct ButtonInfo
{
    const char* label;
    WORD        mask;   // 0 for the triggers
};

static const ButtonInfo g_Buttons[BTN_COUNT] =
{
    { "A",     XINPUT_GAMEPAD_A },
    { "B",     XINPUT_GAMEPAD_B },
    { "X",     XINPUT_GAMEPAD_X },
    { "Y",     XINPUT_GAMEPAD_Y },
    { "LB",    XINPUT_GAMEPAD_LEFT_SHOULDER },
    { "RB",    XINPUT_GAMEPAD_RIGHT_SHOULDER },
    { "LS",    XINPUT_GAMEPAD_LEFT_THUMB },
    { "RS",    XINPUT_GAMEPAD_RIGHT_THUMB },
    { "BACK",  XINPUT_GAMEPAD_BACK },
    { "START", XINPUT_GAMEPAD_START },
    { "UP",    XINPUT_GAMEPAD_DPAD_UP },
    { "DOWN",  XINPUT_GAMEPAD_DPAD_DOWN },
    { "LEFT",  XINPUT_GAMEPAD_DPAD_LEFT },
    { "RIGHT", XINPUT_GAMEPAD_DPAD_RIGHT },
    { "LT",    0 },
    { "RT",    0 },
};

//--------------------------------------------------------------------------------------
// Per-controller state and statistics
//--------------------------------------------------------------------------------------
const int SECTORS = 64;
const int TRAIL = 40;

struct StickStats
{
    float reach[SECTORS];       // furthest normalized magnitude seen in each direction
    float trailX[TRAIL], trailY[TRAIL];
    int   trailHead;
    bool  restMeasured;
    float restMax;              // largest magnitude seen during the hands-off test
    float restX, restY;         // average position during the hands-off test
};

struct PadState
{
    bool          connected;
    XINPUT_STATE  state;
    XINPUT_STATE  prev;
    XINPUT_CAPABILITIES caps;
    DWORD         lastChange;       // tick of the last input change (for Auto port select)

    DWORD         pressCount[BTN_COUNT];
    bool          tested[BTN_COUNT];
    BYTE          trigMax[2];
    bool          trigRestMeasured;
    BYTE          trigRest[2];
    StickStats    stick[2];

    bool          batteryKnown;     // wireless pads only
    int           battery;          // BATTERY_EMPTY..BATTERY_FULL
    DWORD         batteryTick;
};

static PadState g_Pads[XUSER_MAX_COUNT];

//--------------------------------------------------------------------------------------
// App state
//--------------------------------------------------------------------------------------
enum Mode { MODE_TEST, MODE_MENU, MODE_RUMBLE, MODE_DRIFT, MODE_DRIFT_RESULT, MODE_REMAP };

static Mode  g_Mode = MODE_TEST;
static int   g_PortChoice = -1;          // -1 = auto
static int   g_Active = 0;               // port being shown
static float g_BackHold = 0.0f;          // seconds BACK has been held (opens the menu)
static bool  g_BackConsumed = false;     // ignore BACK until released after opening menu
static int   g_MenuSel = 0;
static float g_RepeatTimer = 0.0f;
static int   g_RepeatDir = 0;
static float g_Time = 0.0f;
static char  g_Toast[96];
static float g_ToastTime = 0.0f;

// Rumble test
static float g_RumbleL = 0.0f, g_RumbleR = 0.0f;

// Drift test
static float g_DriftTimer = 0.0f;
static int   g_DriftSamples = 0;
static float g_DriftSum[2][2];
static float g_DriftMax[2];
static BYTE  g_DriftTrig[2];
const float  DRIFT_WAIT = 2.0f;          // seconds to let go
const float  DRIFT_SAMPLE = 2.0f;        // seconds of sampling

//--------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------
static float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float Norm(SHORT v) { return Clamp(v / 32767.0f, -1.0f, 1.0f); }

static bool ButtonDown(const XINPUT_STATE& s, int b)
{
    if (b == BTN_LT) return s.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    if (b == BTN_RT) return s.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    return (s.Gamepad.wButtons & g_Buttons[b].mask) != 0;
}

static void StickXY(const XINPUT_STATE& s, int which, float& x, float& y)
{
    x = Norm(which == 0 ? s.Gamepad.sThumbLX : s.Gamepad.sThumbRX);
    y = Norm(which == 0 ? s.Gamepad.sThumbLY : s.Gamepad.sThumbRY);
}

static void SetRumble(int port, float left, float right)
{
    XINPUT_VIBRATION v;
    v.wLeftMotorSpeed = (WORD)(Clamp(left, 0, 1) * 65535.0f);
    v.wRightMotorSpeed = (WORD)(Clamp(right, 0, 1) * 65535.0f);
    XInputSetState(port, &v);
}

static void StopAllRumble()
{
    for (int i = 0; i < XUSER_MAX_COUNT; i++)
        if (g_Pads[i].connected)
            SetRumble(i, 0, 0);
}

static void Toast(const char* msg)
{
    strncpy(g_Toast, msg, sizeof(g_Toast) - 1);
    g_ToastTime = 3.0f;
}

static void ResetStats(PadState& p)
{
    ZeroMemory(p.pressCount, sizeof(p.pressCount));
    ZeroMemory(p.tested, sizeof(p.tested));
    ZeroMemory(p.trigMax, sizeof(p.trigMax));
    ZeroMemory(p.stick, sizeof(p.stick));
    p.trigRestMeasured = false;
}

static const char* SubtypeName(BYTE sub)
{
    switch (sub)
    {
    case XINPUT_DEVSUBTYPE_GAMEPAD:      return "Gamepad";
    case XINPUT_DEVSUBTYPE_WHEEL:        return "Wheel";
    case XINPUT_DEVSUBTYPE_ARCADE_STICK: return "Arcade stick";
    case XINPUT_DEVSUBTYPE_FLIGHT_STICK: return "Flight stick";
    case XINPUT_DEVSUBTYPE_DANCEPAD:     return "Dance pad";
    case XINPUT_DEVSUBTYPE_GUITAR:
    case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE:
    case XINPUT_DEVSUBTYPE_GUITAR_BASS:  return "Guitar";
    case XINPUT_DEVSUBTYPE_DRUM_KIT:     return "Drum kit";
    case XINPUT_DEVSUBTYPE_ARCADE_PAD:   return "Arcade pad";
    default:                             return "Controller";
    }
}

// Bitmask of inputs that show someone is using this pad: buttons, triggers past the
// threshold, and bit 16 while either stick is pushed well past any drift.
static DWORD Deliberate(const XINPUT_STATE& s)
{
    DWORD m = s.Gamepad.wButtons;
    for (int b = BTN_LT; b <= BTN_RT; b++)
        if (ButtonDown(s, b)) m |= 0x20000u << (b - BTN_LT);
    for (int k = 0; k < 2; k++)
    {
        float x, y;
        StickXY(s, k, x, y);
        if (x * x + y * y > 0.5f * 0.5f) m |= 0x10000;
    }
    return m;
}

//--------------------------------------------------------------------------------------
// Input polling
//--------------------------------------------------------------------------------------
static void PollPads(DWORD now)
{
    for (int i = 0; i < XUSER_MAX_COUNT; i++)
    {
        PadState& p = g_Pads[i];
        XINPUT_STATE s;
        bool was = p.connected;
        p.connected = XInputGetState(i, &s) == ERROR_SUCCESS;
        if (!p.connected)
        {
            if (was && g_PortChoice == i)
            {
                g_PortChoice = -1;
                Toast("Controller disconnected - back to Auto");
            }
            continue;
        }
        if (!was)
        {
            XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &p.caps);
            ZeroMemory(&p.state, sizeof(p.state));
            p.lastChange = now;
            p.batteryKnown = false;
            p.batteryTick = now - 10000;
        }
        // Battery level changes slowly; ask xam every two seconds.
        if ((p.caps.Flags & XINPUT_CAPS_WIRELESS) && now - p.batteryTick >= 2000)
        {
            p.batteryTick = now;
            p.batteryKnown = GetBatteryLevel(i, &p.battery);
        }
        p.prev = p.state;
        p.state = s;
        if (Deliberate(s) != Deliberate(p.prev) || Deliberate(s) & 0x10000)
            p.lastChange = now;
    }

    if (g_PortChoice >= 0)
    {
        g_Active = g_PortChoice;
        return;
    }
    // Auto: follow the controller that was used most recently.
    int best = -1;
    for (int i = 0; i < XUSER_MAX_COUNT; i++)
        if (g_Pads[i].connected && (best < 0 || (int)(g_Pads[i].lastChange - g_Pads[best].lastChange) > 0))
            best = i;
    if (best >= 0 && (!g_Pads[g_Active].connected || g_Pads[best].lastChange != g_Pads[g_Active].lastChange))
        g_Active = best;
}

static void UpdateStats(PadState& p)
{
    for (int b = 0; b < BTN_COUNT; b++)
    {
        if (ButtonDown(p.state, b) && !ButtonDown(p.prev, b))
        {
            p.pressCount[b]++;
            p.tested[b] = true;
        }
    }
    if (p.state.Gamepad.bLeftTrigger > p.trigMax[0])  p.trigMax[0] = p.state.Gamepad.bLeftTrigger;
    if (p.state.Gamepad.bRightTrigger > p.trigMax[1]) p.trigMax[1] = p.state.Gamepad.bRightTrigger;

    for (int k = 0; k < 2; k++)
    {
        StickStats& st = p.stick[k];
        float x, y;
        StickXY(p.state, k, x, y);
        float mag = sqrtf(x * x + y * y);
        if (mag > 0.25f)
        {
            float a = atan2f(y, x);
            int sec = (int)((a + PI) / (2 * PI) * SECTORS) % SECTORS;
            // The square gate reports past full scale on the diagonals; the edge counts as 100%.
            float m = mag > 1.0f ? 1.0f : mag;
            if (m > st.reach[sec])
                st.reach[sec] = m;
        }
        st.trailHead = (st.trailHead + 1) % TRAIL;
        st.trailX[st.trailHead] = x;
        st.trailY[st.trailHead] = y;
    }
}

//--------------------------------------------------------------------------------------
// Menu
//--------------------------------------------------------------------------------------
enum MenuItem
{
    MI_PORT, MI_RUMBLE, MI_DRIFT, MI_REMAP, MI_THEME, MI_RESET, MI_EXIT, MI_COUNT
};

// Next port choice in direction 'dir' (+1/-1): Auto (-1) or a port with a controller connected.
static int NextPortChoice(int cur, int dir)
{
    int c = cur;
    for (int tries = 0; tries < 5; tries++)
    {
        c = (c + 1 + dir + 5) % 5 - 1;   // cycles -1..3
        if (c < 0 || g_Pads[c].connected)
            return c;
    }
    return -1;
}

// Returns -1/+1 for a held direction with key repeat, 0 otherwise.
static int NavRepeat(int dir, float dt)
{
    if (dir == 0)
    {
        g_RepeatDir = 0;
        return 0;
    }
    if (dir != g_RepeatDir)
    {
        g_RepeatDir = dir;
        g_RepeatTimer = 0.4f;
        return dir;
    }
    g_RepeatTimer -= dt;
    if (g_RepeatTimer <= 0)
    {
        g_RepeatTimer = 0.09f;
        return dir;
    }
    return 0;
}

static bool Pressed(const PadState& p, WORD mask)
{
    return (p.state.Gamepad.wButtons & mask) && !(p.prev.Gamepad.wButtons & mask);
}

static void StartDriftTest()
{
    g_Mode = MODE_DRIFT;
    g_DriftTimer = 0.0f;
    g_DriftSamples = 0;
    ZeroMemory(g_DriftSum, sizeof(g_DriftSum));
    ZeroMemory(g_DriftMax, sizeof(g_DriftMax));
    ZeroMemory(g_DriftTrig, sizeof(g_DriftTrig));
}

static void UpdateMenu(PadState& p, float dt)
{
    const XINPUT_GAMEPAD& g = p.state.Gamepad;
    int vert = 0, horiz = 0;
    if ((g.wButtons & XINPUT_GAMEPAD_DPAD_UP) || g.sThumbLY > 20000)    vert = -1;
    if ((g.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || g.sThumbLY < -20000) vert = 1;
    if ((g.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || g.sThumbLX < -20000) horiz = -1;
    if ((g.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || g.sThumbLX > 20000) horiz = 1;

    int step = NavRepeat(vert ? vert * 10 : horiz, dt);
    if (step == 10 || step == -10)
        g_MenuSel = (g_MenuSel + step / 10 + MI_COUNT) % MI_COUNT;
    else if (step != 0 && g_MenuSel == MI_PORT)
        g_PortChoice = NextPortChoice(g_PortChoice, step);
    else if (step != 0 && g_MenuSel == MI_THEME)
    {
        Theme_Set(!Theme_IsLight());
        Theme_Save();
    }

    if (Pressed(p, XINPUT_GAMEPAD_B) || (Pressed(p, XINPUT_GAMEPAD_BACK) && !g_BackConsumed))
        g_Mode = MODE_TEST;
    else if (Pressed(p, XINPUT_GAMEPAD_A))
    {
        switch (g_MenuSel)
        {
        case MI_PORT:   g_PortChoice = NextPortChoice(g_PortChoice, 1); break;
        case MI_RUMBLE:
            g_Mode = MODE_RUMBLE;
            break;
        case MI_DRIFT:  StartDriftTest(); break;
        case MI_THEME:
            Theme_Set(!Theme_IsLight());
            Theme_Save();
            break;
        case MI_REMAP:
            RemapUI_Open();
            g_Mode = MODE_REMAP;
            break;
        case MI_RESET:
            ResetStats(p);
            Toast("Stats reset");
            g_Mode = MODE_TEST;
            break;
        case MI_EXIT:
            StopAllRumble();
            XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
            break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Rumble test
//--------------------------------------------------------------------------------------
static void UpdateRumble(int port, PadState& p, float dt)
{
    if (Pressed(p, XINPUT_GAMEPAD_B))
    {
        SetRumble(port, 0, 0);
        g_Mode = MODE_MENU;
        return;
    }
    // LT drives the left motor, RT the right one.
    float l = p.state.Gamepad.bLeftTrigger / 255.0f;
    float r = p.state.Gamepad.bRightTrigger / 255.0f;
    g_RumbleL = l;
    g_RumbleR = r;
    SetRumble(port, l, r);
}

//--------------------------------------------------------------------------------------
// Drift test
//--------------------------------------------------------------------------------------
static void UpdateDrift(PadState& p, float dt)
{
    if (Pressed(p, XINPUT_GAMEPAD_B))
    {
        g_Mode = MODE_TEST;
        return;
    }
    g_DriftTimer += dt;
    if (g_DriftTimer < DRIFT_WAIT)
        return;

    for (int k = 0; k < 2; k++)
    {
        float x, y;
        StickXY(p.state, k, x, y);
        g_DriftSum[k][0] += x;
        g_DriftSum[k][1] += y;
        float mag = sqrtf(x * x + y * y);
        if (mag > g_DriftMax[k])
            g_DriftMax[k] = mag;
    }
    if (p.state.Gamepad.bLeftTrigger > g_DriftTrig[0])  g_DriftTrig[0] = p.state.Gamepad.bLeftTrigger;
    if (p.state.Gamepad.bRightTrigger > g_DriftTrig[1]) g_DriftTrig[1] = p.state.Gamepad.bRightTrigger;
    g_DriftSamples++;

    if (g_DriftTimer >= DRIFT_WAIT + DRIFT_SAMPLE && g_DriftSamples > 0)
    {
        for (int k = 0; k < 2; k++)
        {
            StickStats& st = p.stick[k];
            st.restMeasured = true;
            st.restMax = g_DriftMax[k];
            st.restX = g_DriftSum[k][0] / g_DriftSamples;
            st.restY = g_DriftSum[k][1] / g_DriftSamples;
        }
        p.trigRestMeasured = true;
        p.trigRest[0] = g_DriftTrig[0];
        p.trigRest[1] = g_DriftTrig[1];
        g_Mode = MODE_DRIFT_RESULT;
    }
}

static void UpdateDriftResult(PadState& p)
{
    if (Pressed(p, XINPUT_GAMEPAD_A) || Pressed(p, XINPUT_GAMEPAD_B))
        g_Mode = MODE_TEST;
}

// Resting offset of a stick, as a fraction of full deflection.
static const char* DriftVerdict(float offset, DWORD* color)
{
    if (offset < 0.10f) { *color = COL_LIT;    return "GOOD"; }
    if (offset < 0.20f) { *color = COL_YELLOW; return "SLIGHT"; }
    *color = COL_RED;
    return "DRIFT";
}

//--------------------------------------------------------------------------------------
// Drawing: controller diagram
//--------------------------------------------------------------------------------------
static DWORD Dim(DWORD c)
{
    return R::Lerp(c, g_Theme->dimTarget, g_Theme->dimAmount);
}

// The controller art was traced from a 1280x916 reference image (not included)
// and drawn at this scale, centered on x = 640 with the image's top at y = 200.
const float PAD_SCALE = 0.38f;
const float PAD_TOP = 200.0f;

static float PadX(float x) { return 640.0f + (x - 640.0f) * PAD_SCALE; }
static float PadY(float y) { return PAD_TOP + y * PAD_SCALE; }


static void DrawFaceButton(float x, float y, float r, DWORD color, const char* label, bool down)
{
    R::Circle(x, y, r + 6, COL_PAD_HALO);
    R::Circle(x, y, r + 2, COL_PAD_RIM);
    R::Circle(x, y, r, down ? color : Dim(color));
    if (down)
        R::Ring(x, y, r + 5, 2, COL_TEXT);
    R::Text(x, y - 7, 2, down ? RGBA(15, 15, 15, 255) : R::Lerp(color, COL_TEXT, 0.3f), ALIGN_CENTER, label);
}

// Back / Start: a small pill with an arrow, the label above it.
static void DrawSmallButton(float x, float y, const char* label, bool pointLeft, bool down)
{
    R::RoundRect(x - 16, y - 11, 32, 22, 11, COL_BODY_EDGE);
    R::RoundRect(x - 14, y - 9, 28, 18, 9, down ? COL_LIT : COL_PART);
    DWORD arrow = down ? COL_ON_LIT : COL_DIM;
    if (pointLeft)
        R::Tri(x - 6, y, x + 4, y - 6, x + 4, y + 6, arrow);
    else
        R::Tri(x + 6, y, x - 4, y - 6, x - 4, y + 6, arrow);
    R::Text(x, y - 24, 1, down ? COL_LIT : COL_DIM, ALIGN_CENTER, label);
}

static void DrawStickCap(float x, float y, float r, float sx, float sy, bool click, bool tested)
{
    R::Circle(x, y, r * 1.5f, COL_PAD_HALO);
    R::Circle(x, y, r + 2, COL_PAD_RIM);
    R::Circle(x, y, r, COL_PAD_WELL);
    float capR = r * 0.72f, travel = r * 0.4f;
    float cx = x + sx * travel, cy = y - sy * travel;
    R::Circle(cx, cy, capR, click ? COL_LIT : COL_CAP_OFF);
    R::Ring(cx, cy, capR, 3, click ? COL_TEXT : COL_CAP_RING);
    R::Circle(cx, cy, 3, tested ? COL_TEXT : COL_FAINT);
}

static void DrawTrigger(float x, float y, float w, float h, const char* label, BYTE value, bool down)
{
    R::RoundRect(x - 2, y - 2, w + 4, h + 4, 12, COL_BODY_EDGE);
    R::RoundRect(x, y, w, h, 10, COL_PART);
    float fill = (h - 8) * value / 255.0f;
    R::Rect(x + 4, y + h - 4 - fill, w - 8, fill, down ? COL_LIT : COL_ORANGE);
    R::Text(x + w / 2, y + 8, 2, COL_TEXT, ALIGN_CENTER, label);
    R::Textf(x + w / 2, y + h - 24, 2, COL_TEXT, ALIGN_CENTER, "%d", value);
}

static void DrawBumper(float x, float y, const char* label, bool down)
{
    R::RoundRect(x - 2, y - 2, 144, 38, 15, COL_BODY_EDGE);
    R::RoundRect(x, y, 140, 34, 14, down ? COL_LIT : COL_PART);
    R::Text(x + 70, y + 9, 2, down ? COL_ON_LIT : COL_DIM, ALIGN_CENTER, label);
}

// Left half of the outline in reference-image pixels, from the top center going left and around
// the grip to the bottom center. The right half is mirrored.
static const float g_TopAndSide[][2] =
{
    { 640, 79 }, { 576, 77 }, { 528, 72 }, { 480, 60 }, { 432, 39 }, { 400, 23 }, { 368, 16 },
    { 336, 14 }, { 304, 17 }, { 272, 23 }, { 240, 33 }, { 208, 48 }, { 176, 71 }, { 160, 86 },
    { 144, 110 }, { 128, 151 }, { 112, 197 }, { 96, 245 }, { 80, 296 }, { 64, 350 }, { 48, 408 },
    { 32, 472 }, { 16, 548 }, { 5, 615 }, { 1, 680 }, { 4, 760 }, { 16, 830 }, { 32, 878 },
    { 48, 897 }, { 80, 910 }, { 128, 906 },
};
// Outer underside edge (the black line) from the grip to the bottom center.
static const float g_UnderEdge[][2] =
{
    { 176, 892 }, { 224, 863 }, { 272, 816 }, { 320, 766 }, { 368, 726 }, { 416, 696 },
    { 464, 677 }, { 512, 667 }, { 576, 663 }, { 640, 663 },
};
// Where the light top face meets the darker underside band.
static const float g_FaceEdge[][2] =
{
    { 160, 884 }, { 192, 858 }, { 224, 827 }, { 256, 791 }, { 288, 755 }, { 320, 722 },
    { 352, 694 }, { 384, 672 }, { 416, 654 }, { 448, 643 }, { 480, 634 }, { 512, 630 },
    { 544, 627 }, { 576, 626 }, { 640, 626 },
};

#define COUNT_OF(a) (sizeof(a) / sizeof(a[0]))
const int MAX_CTRL = 2 * (COUNT_OF(g_TopAndSide) + COUNT_OF(g_FaceEdge));
const int SPLINE_STEPS = 3;

struct PadShape
{
    float          x[MAX_CTRL * SPLINE_STEPS], y[MAX_CTRL * SPLINE_STEPS];
    unsigned short idx[3 * MAX_CTRL * SPLINE_STEPS];
    int            n, tris;
};
static PadShape g_Body, g_Face;

static void BuildShape(PadShape& shape, const float (*bottom)[2], int bottomCount)
{
    // Left half, top center -> grip -> bottom center, then the mirrored right half back up.
    float cx[MAX_CTRL], cy[MAX_CTRL];
    int n = 0;
    for (int i = 0; i < (int)COUNT_OF(g_TopAndSide); i++, n++)
    {
        cx[n] = g_TopAndSide[i][0];
        cy[n] = g_TopAndSide[i][1];
    }
    for (int i = 0; i < bottomCount; i++, n++)
    {
        cx[n] = bottom[i][0];
        cy[n] = bottom[i][1];
    }
    int half = n;
    for (int i = half - 2; i > 0; i--, n++)
    {
        cx[n] = 1280.0f - cx[i];
        cy[n] = cy[i];
    }
    for (int i = 0; i < n; i++)
    {
        cx[i] = PadX(cx[i]);
        cy[i] = PadY(cy[i]);
    }
    shape.n = Shape::Spline(cx, cy, n, SPLINE_STEPS, shape.x, shape.y);
    shape.tris = Shape::Triangulate(shape.x, shape.y, shape.n, shape.idx);
}

static void InitControllerShape()
{
    BuildShape(g_Body, g_UnderEdge, COUNT_OF(g_UnderEdge));
    BuildShape(g_Face, g_FaceEdge, COUNT_OF(g_FaceEdge));
}

static void FillShape(const PadShape& s, DWORD color)
{
    for (int t = 0; t < s.tris; t++)
    {
        const unsigned short* k = &s.idx[t * 3];
        R::Tri(s.x[k[0]], s.y[k[0]], s.x[k[1]], s.y[k[1]], s.x[k[2]], s.y[k[2]], color);
    }
}

static void DrawBody()
{
    // Thick edge first; the fill covers its inner half, leaving an outline.
    for (int i = 0; i < g_Body.n; i++)
    {
        int j = (i + 1) % g_Body.n;
        R::Line(g_Body.x[i], g_Body.y[i], g_Body.x[j], g_Body.y[j], 6, COL_BODY_EDGE);
        R::Circle(g_Body.x[i], g_Body.y[i], 3.0f, COL_BODY_EDGE);
    }
    FillShape(g_Body, COL_PAD_UNDER);
    FillShape(g_Face, COL_PAD_FACE);
}

// Guide button with the four player-quadrant lights; the active port's quadrant is lit,
// like the real ring of light.
static void DrawGuide(float x, float y, float r, int port)
{
    R::Circle(x, y, r + 3, COL_PAD_RIM);
    // Quadrants 1..4 = top-left, top-right, bottom-left, bottom-right.
    static const float starts[4] = { PI, 1.5f * PI, 0.5f * PI, 0.0f };
    for (int q = 0; q < 4; q++)
    {
        float a0 = starts[q] + 0.08f, a1 = starts[q] + 0.5f * PI - 0.08f;
        R::Arc(x, y, r, a0, a1, q == port ? RGBA(90, 230, 60, 255) : RGBA(40, 70, 40, 255));
    }
    R::Circle(x, y, r - 5, RGBA(150, 154, 160, 255));
    R::Circle(x, y, r - 8, RGBA(120, 124, 130, 255));
    // The X
    float d = (r - 10) * 0.7f;
    R::Line(x - d, y - d, x + d, y + d, 4, RGBA(60, 170, 40, 255));
    R::Line(x - d, y + d, x + d, y - d, 4, RGBA(60, 170, 40, 255));
}

static void DrawController(const PadState& p, int port, bool connected)
{
    const XINPUT_STATE& s = p.state;
    float lx, ly, rx, ry;
    StickXY(s, 0, lx, ly);
    StickXY(s, 1, rx, ry);

    // Triggers and bumpers sit above the body so nothing covers them.
    DrawTrigger(PadX(300) - 60, 88, 120, 62, "LT", s.Gamepad.bLeftTrigger, ButtonDown(s, BTN_LT));
    DrawTrigger(PadX(980) - 60, 88, 120, 62, "RT", s.Gamepad.bRightTrigger, ButtonDown(s, BTN_RT));
    DrawBumper(PadX(300) - 70, 162, "LB", ButtonDown(s, BTN_LB));
    DrawBumper(PadX(980) - 70, 162, "RB", ButtonDown(s, BTN_RB));

    DrawBody();
    if (!connected)
    {
        R::Text(640, 330, 2, COL_DIM, ALIGN_CENTER, "No controller connected");
        return;
    }

    DrawGuide(PadX(640), PadY(213), 27, port);
    DrawSmallButton(PadX(512), PadY(215), "BACK", true, ButtonDown(s, BTN_BACK));
    DrawSmallButton(PadX(768), PadY(215), "START", false, ButtonDown(s, BTN_START));

    // Hold-BACK progress for the menu.
    if (g_BackHold > 0.05f && g_Mode == MODE_TEST)
        R::Rect(PadX(512) - 16, PadY(215) + 14, 32 * Clamp(g_BackHold, 0, 1), 4, COL_TEXT);

    DrawStickCap(PadX(280), PadY(195), 33, lx, ly, ButtonDown(s, BTN_LS), p.tested[BTN_LS]);
    DrawStickCap(PadX(827), PadY(415), 33, rx, ry, ButtonDown(s, BTN_RS), p.tested[BTN_RS]);

    // D-pad
    float dx = PadX(453), dy = PadY(410);
    R::Circle(dx, dy, 58, COL_PAD_HALO);
    R::Circle(dx, dy, 42, COL_PAD_RIM);
    R::Circle(dx, dy, 40, COL_PAD_WELL);
    DWORD off = COL_CAP_OFF;
    R::Rect(dx - 10, dy - 10, 20, 20, off);
    R::Rect(dx - 10, dy - 32, 20, 22, ButtonDown(s, BTN_UP) ? COL_LIT : off);
    R::Rect(dx - 10, dy + 10, 20, 22, ButtonDown(s, BTN_DOWN) ? COL_LIT : off);
    R::Rect(dx - 32, dy - 10, 22, 20, ButtonDown(s, BTN_LEFT) ? COL_LIT : off);
    R::Rect(dx + 10, dy - 10, 22, 20, ButtonDown(s, BTN_RIGHT) ? COL_LIT : off);

    // Face buttons
    DrawFaceButton(PadX(1008), PadY(118), 17, RGBA(240, 210, 30, 255), "Y", ButtonDown(s, BTN_Y));
    DrawFaceButton(PadX(915), PadY(210), 17, RGBA(40, 90, 230, 255), "X", ButtonDown(s, BTN_X));
    DrawFaceButton(PadX(1100), PadY(210), 17, RGBA(225, 40, 35, 255), "B", ButtonDown(s, BTN_B));
    DrawFaceButton(PadX(1008), PadY(302), 17, RGBA(60, 200, 40, 255), "A", ButtonDown(s, BTN_A));
}

//--------------------------------------------------------------------------------------
// Drawing: stick and trigger detail panels
//--------------------------------------------------------------------------------------
static void DrawStickPlot(float cx, float cy, float rad, const StickStats& st, float x, float y, bool click)
{
    R::Circle(cx, cy, rad + 8, COL_PANEL_EDGE);
    R::Circle(cx, cy, rad, COL_WELL);

    // Grid
    R::Line(cx - rad, cy, cx + rad, cy, 1, COL_FAINT);
    R::Line(cx, cy - rad, cx, cy + rad, 1, COL_FAINT);
    R::Ring(cx, cy, rad * 0.5f, 1, COL_FAINT);

    // Outer reach: furthest point seen in each direction.
    for (int i = 0; i < SECTORS; i++)
    {
        int j = (i + 1) % SECTORS;
        if (st.reach[i] <= 0 || st.reach[j] <= 0)
            continue;
        float a0 = (i + 0.5f) / SECTORS * 2 * PI - PI, a1 = (j + 0.5f) / SECTORS * 2 * PI - PI;
        float r0 = rad * Clamp(st.reach[i], 0, 1.12f), r1 = rad * Clamp(st.reach[j], 0, 1.12f);
        R::Line(cx + cosf(a0) * r0, cy - sinf(a0) * r0, cx + cosf(a1) * r1, cy - sinf(a1) * r1, 2, COL_CYAN);
    }
    R::Ring(cx, cy, rad, 2, COL_DIM);

    // Trail, oldest first
    for (int i = 1; i < TRAIL; i++)
    {
        int a = (st.trailHead + i) % TRAIL, b = (st.trailHead + i + 1) % TRAIL;
        DWORD c = (COL_LIT & 0x00FFFFFF) | ((DWORD)(140.0f * i / TRAIL) << 24);
        R::Line(cx + st.trailX[a] * rad, cy - st.trailY[a] * rad, cx + st.trailX[b] * rad, cy - st.trailY[b] * rad, 3, c);
    }

    float px = cx + x * rad, py = cy - y * rad;
    R::Line(cx, cy, px, py, 1, COL_DIM);
    DWORD dot = click ? COL_TEXT : COL_LIT;
    R::Circle(px, py, 10, RGBA(0, 0, 0, 160));
    R::Circle(px, py, 8, dot);
}

static void DrawStickPanel(float left, const char* title, const PadState& p, int k)
{
    float cx = left + 160;
    const StickStats& st = p.stick[k];
    float x, y;
    StickXY(p.state, k, x, y);
    SHORT rawX = k == 0 ? p.state.Gamepad.sThumbLX : p.state.Gamepad.sThumbRX;
    SHORT rawY = k == 0 ? p.state.Gamepad.sThumbLY : p.state.Gamepad.sThumbRY;

    R::RoundRect(left, 86, 320, 476, 14, COL_PANEL);
    R::Text(cx, 96, 2, COL_TEXT, ALIGN_CENTER, title);
    DrawStickPlot(cx, 226, 104, st, x, y, ButtonDown(p.state, k == 0 ? BTN_LS : BTN_RS));

    // Outer reach summary
    int visited = 0;
    float sum = 0, low = 10;
    for (int i = 0; i < SECTORS; i++)
        if (st.reach[i] > 0)
        {
            visited++;
            sum += st.reach[i];
            if (st.reach[i] < low) low = st.reach[i];
        }

    // Each line is at most 18 characters so it fits the panel (320 px, text from +18) at 16px per character.
    float tx = left + 18, ty = 362, lh = 20;
    R::Textf(tx, ty, 2, COL_TEXT, ALIGN_LEFT, "X %+6d %+6.1f%%", rawX, x * 100);
    R::Textf(tx, ty + lh, 2, COL_TEXT, ALIGN_LEFT, "Y %+6d %+6.1f%%", rawY, y * 100);

    if (visited == 0)
        R::Text(tx, ty + lh * 2, 2, COL_CYAN, ALIGN_LEFT, "Spin for reach");
    else
        R::Textf(tx, ty + lh * 2, 2, COL_CYAN, ALIGN_LEFT, "Reach %3.0f%% lo%4.0f%%", sum / visited * 100, low * 100);

    // Drift result (from the drift test in the menu)
    if (!st.restMeasured)
        R::Text(tx, ty + lh * 3, 2, COL_DIM, ALIGN_LEFT, "Drift: not run");
    else
    {
        DWORD c;
        const char* verdict = DriftVerdict(st.restMax, &c);
        R::Textf(tx, ty + lh * 3, 2, c, ALIGN_LEFT, "Drift %3.0f%% %s", st.restMax * 100, verdict);
    }

    // Trigger bar
    int t = k;
    BYTE v = t == 0 ? p.state.Gamepad.bLeftTrigger : p.state.Gamepad.bRightTrigger;
    float bx = left + 18, by = 514, bw = 284, bh = 18;
    R::Textf(bx, by - 20, 2, COL_TEXT, ALIGN_LEFT, "%s %3d   max %3d", t == 0 ? "LT" : "RT", v, p.trigMax[t]);
    R::Rect(bx, by, bw, bh, COL_WELL);
    R::Rect(bx, by, bw * v / 255.0f, bh, v > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? COL_LIT : COL_ORANGE);
    R::Rect(bx + bw * XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 255.0f, by - 3, 2, bh + 6, COL_ORANGE);
    R::Rect(bx + bw * p.trigMax[t] / 255.0f - 1, by - 3, 2, bh + 6, COL_CYAN);
    if (p.trigRestMeasured)
    {
        BYTE rest = p.trigRest[t];
        R::Textf(bx, by + 24, 2, rest > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? COL_RED : COL_LIT, ALIGN_LEFT,
                 "Rest %3d %s", rest, rest > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? "STUCK" : "GOOD");
    }
    else if (p.trigMax[t] > 0 && p.trigMax[t] < 255)
        R::Text(bx, by + 24, 2, COL_DIM, ALIGN_LEFT, "Pull all the way");
    else if (p.trigMax[t] == 255)
        R::Text(bx, by + 24, 2, COL_LIT, ALIGN_LEFT, "Full range OK");
}

static void DrawChecklist(const PadState& p)
{
    int done = 0;
    for (int b = 0; b < BTN_COUNT; b++)
        if (p.tested[b]) done++;
    R::Textf(64, 574, 2, done == BTN_COUNT ? COL_LIT : COL_TEXT, ALIGN_LEFT,
             "Button check %d/%d%s", done, BTN_COUNT, done == BTN_COUNT ? " - all buttons work" : "");

    float x = 64, y = 600, w = 68, h = 54, gap = 4;
    for (int b = 0; b < BTN_COUNT; b++, x += w + gap)
    {
        bool down = ButtonDown(p.state, b);
        DWORD edge = p.tested[b] ? COL_ACCENT : COL_PANEL_EDGE;
        R::RoundRect(x, y, w, h, 8, edge);
        R::RoundRect(x + 3, y + 3, w - 6, h - 6, 6, down ? COL_LIT : COL_PANEL);
        DWORD tc = down ? COL_ON_LIT : (p.tested[b] ? COL_TEXT : COL_DIM);
        const char* lbl = g_Buttons[b].label;
        float scale = strlen(lbl) > 4 ? 1.0f : 2.0f;
        R::Text(x + w / 2, y + 9, scale, tc, ALIGN_CENTER, lbl);
        R::Textf(x + w / 2, y + 31, 2, tc, ALIGN_CENTER, "%u", p.pressCount[b]);
    }
}

static void DrawBattery(float x, float y, const PadState& p)
{
    if (!(p.caps.Flags & XINPUT_CAPS_WIRELESS))
    {
        R::Text(x + 40, y + 6, 2, COL_DIM, ALIGN_RIGHT, "WIRED");
        return;
    }
    if (!p.batteryKnown)
    {
        R::Text(x + 40, y + 6, 2, COL_DIM, ALIGN_RIGHT, "BAT ?");
        return;
    }
    static const char* names[4] = { "Empty", "Low", "Medium", "Full" };
    static const DWORD colors[4] = { COL_RED, COL_YELLOW, COL_LIT, COL_LIT };
    DWORD c = colors[p.battery];
    // Battery outline with a nub, and three bars like the Guide's icon.
    R::RoundRect(x, y, 40, 22, 4, COL_TEXT);
    R::RoundRect(x + 2, y + 2, 36, 18, 3, COL_BG);
    R::Rect(x + 40, y + 6, 4, 10, COL_TEXT);
    for (int b = 0; b < 3; b++)
        R::Rect(x + 5 + b * 11, y + 5, 9, 12, b < p.battery ? c : COL_PANEL_EDGE);
    R::Text(x - 8, y + 7, 1, c, ALIGN_RIGHT, names[p.battery]);
}

static void DrawHeader()
{
    R::Text(64, 40, 3, COL_TEXT, ALIGN_LEFT, "CONTROLLER WORKSHOP");

    for (int i = 0; i < XUSER_MAX_COUNT; i++)
    {
        float x = 1052.0f + i * 42, y = 36;
        bool on = g_Pads[i].connected;
        if (i == g_Active && on)
            R::RoundRect(x - 3, y - 3, 38, 38, 8, COL_TEXT);
        R::RoundRect(x, y, 32, 32, 6, on ? COL_ACCENT : COL_PANEL_EDGE);
        R::Textf(x + 16, y + 8, 2, on ? COL_SELECTED : COL_DIM, ALIGN_CENTER, "%d", i + 1);
    }

    const PadState& p = g_Pads[g_Active];
    if (p.connected)
    {
        DrawBattery(986, 41, p);
        // Just wired/wireless for a normal pad; other devices (guitar, wheel...) keep their name.
        bool wireless = (p.caps.Flags & XINPUT_CAPS_WIRELESS) != 0;
        if (p.caps.SubType == XINPUT_DEVSUBTYPE_GAMEPAD)
            R::Textf(1216, 74, 1, COL_DIM, ALIGN_RIGHT, "%s%s", wireless ? "Wireless" : "Wired",
                     g_PortChoice < 0 ? "" : "  locked");
        else
            R::Textf(1216, 74, 1, COL_DIM, ALIGN_RIGHT, "%s (%s)%s", SubtypeName(p.caps.SubType),
                     wireless ? "wireless" : "wired", g_PortChoice < 0 ? "" : "  locked");
    }
}

static void DrawFooter()
{
    R::Text(1216, 574, 2, COL_DIM, ALIGN_RIGHT, "Hold BACK for menu");
    if (g_ToastTime > 0)
    {
        float w = R::TextWidth(2, g_Toast) + 40;
        DWORD a = (DWORD)(255 * Clamp(g_ToastTime, 0, 1));
        R::RoundRect(640 - w / 2, 520, w, 40, 10, RGBA(16, 124, 16, a * 230 / 255));
        R::Text(640, 532, 2, RGBA(255, 255, 255, a), ALIGN_CENTER, g_Toast);
    }
}

//--------------------------------------------------------------------------------------
// Drawing: overlays
//--------------------------------------------------------------------------------------
static void DrawModalBox(float w, float h, const char* title)
{
    R::Rect(0, 0, SCREEN_W, SCREEN_H, COL_SHADE);
    float x = 640 - w / 2, y = 360 - h / 2;
    R::RoundRect(x - 3, y - 3, w + 6, h + 6, 16, COL_ACCENT);
    R::RoundRect(x, y, w, h, 14, COL_PANEL);
    R::Text(640, y + 20, 3, COL_TEXT, ALIGN_CENTER, title);
}

static void DrawMenu()
{
    DrawModalBox(560, 380, "MENU");
    static const char* names[MI_COUNT] =
    {
        "Controller", "Rumble test", "Drift test (hands off)", "Button remapper", "Theme",
        "Reset stats", "Exit to dashboard"
    };
    float x = 380, y = 240;
    for (int i = 0; i < MI_COUNT; i++, y += 40)
    {
        bool sel = i == g_MenuSel;
        if (sel)
            R::RoundRect(x - 12, y - 10, 544, 36, 8, COL_ACCENT);
        DWORD c = sel ? COL_SELECTED : COL_TEXT;
        R::Text(x, y, 2, c, ALIGN_LEFT, names[i]);
        if (i == MI_PORT)
        {
            char val[32];
            if (g_PortChoice < 0) _snprintf(val, sizeof(val), "< Auto >");
            else _snprintf(val, sizeof(val), "< Port %d >", g_PortChoice + 1);
            R::Text(x + 520, y, 2, c, ALIGN_RIGHT, val);
        }
        if (i == MI_THEME)
            R::Text(x + 520, y, 2, c, ALIGN_RIGHT, Theme_IsLight() ? "< Light >" : "< Dark >");
    }
    R::Text(640, y + 6, 1, COL_DIM, ALIGN_CENTER, "D-pad: move / change    A: select    B: close");
}

static void DrawBar(float x, float y, float w, float h, float v, DWORD color)
{
    R::Rect(x, y, w, h, COL_WELL);
    R::Rect(x, y, w * Clamp(v, 0, 1), h, color);
    R::RectOutline(x, y, w, h, 2, COL_PANEL_EDGE);
}

static void DrawRumble(const PadState& p)
{
    DrawModalBox(700, 320, "RUMBLE TEST");
    float x = 330, y = 266;
    R::Text(x, y, 2, COL_TEXT, ALIGN_LEFT, "Left motor  (heavy, low frequency)");
    DrawBar(x, y + 24, 620, 30, g_RumbleL, COL_ORANGE);
    R::Textf(x + 610, y + 31, 2, COL_TEXT, ALIGN_RIGHT, "%3.0f%%", g_RumbleL * 100);

    y += 90;
    R::Text(x, y, 2, COL_TEXT, ALIGN_LEFT, "Right motor (light, high frequency)");
    DrawBar(x, y + 24, 620, 30, g_RumbleR, COL_CYAN);
    R::Textf(x + 610, y + 31, 2, COL_TEXT, ALIGN_RIGHT, "%3.0f%%", g_RumbleR * 100);

    y += 90;
    R::Text(x, y, 2, COL_DIM, ALIGN_LEFT, "LT / RT: drive each motor   B: back");
    if (!p.caps.Vibration.wLeftMotorSpeed && !p.caps.Vibration.wRightMotorSpeed)
        R::Text(640, y + 34, 1, COL_YELLOW, ALIGN_CENTER, "This device does not report rumble motors");
}

static void DrawDrift()
{
    DrawModalBox(640, 260, "DRIFT TEST");
    if (g_DriftTimer < DRIFT_WAIT)
    {
        R::Text(640, 300, 2, COL_TEXT, ALIGN_CENTER, "Let go of both sticks and triggers");
        R::Textf(640, 340, 3, COL_YELLOW, ALIGN_CENTER, "Starting in %d", (int)(DRIFT_WAIT - g_DriftTimer) + 1);
    }
    else
    {
        R::Text(640, 300, 2, COL_TEXT, ALIGN_CENTER, "Measuring - keep your hands off");
        DrawBar(400, 340, 480, 24, (g_DriftTimer - DRIFT_WAIT) / DRIFT_SAMPLE, COL_LIT);
    }
    R::Text(640, 410, 1, COL_DIM, ALIGN_CENTER, "B: cancel");
}

static void DrawDriftResult(const PadState& p)
{
    DrawModalBox(800, 380, "DRIFT TEST RESULTS");
    float y = 240;
    R::Text(510, y, 1, COL_DIM, ALIGN_LEFT, "average resting position");
    R::Text(880, y, 1, COL_DIM, ALIGN_RIGHT, "worst");
    R::Text(1010, y, 1, COL_DIM, ALIGN_RIGHT, "result");
    y += 20;

    static const char* stickNames[2] = { "Left stick", "Right stick" };
    for (int k = 0; k < 2; k++, y += 36)
    {
        const StickStats& st = p.stick[k];
        DWORD c;
        const char* verdict = DriftVerdict(st.restMax, &c);
        R::Text(270, y, 2, COL_TEXT, ALIGN_LEFT, stickNames[k]);
        R::Textf(510, y, 2, COL_DIM, ALIGN_LEFT, "X %+5.1f%% Y %+5.1f%%", st.restX * 100, st.restY * 100);
        R::Textf(880, y, 2, c, ALIGN_RIGHT, "%.1f%%", st.restMax * 100);
        R::Text(1010, y, 2, c, ALIGN_RIGHT, verdict);
    }

    static const char* trigNames[2] = { "Left trigger", "Right trigger" };
    for (int t = 0; t < 2; t++, y += 36)
    {
        bool stuck = p.trigRest[t] > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        DWORD c = stuck ? COL_RED : COL_LIT;
        R::Text(270, y, 2, COL_TEXT, ALIGN_LEFT, trigNames[t]);
        R::Textf(880, y, 2, c, ALIGN_RIGHT, "%d", p.trigRest[t]);
        R::Text(1010, y, 2, c, ALIGN_RIGHT, stuck ? "STUCK" : "GOOD");
    }

    y += 10;
    R::Text(640, y, 1, COL_DIM, ALIGN_CENTER, "Sticks: under 10% is normal, 10-20% is slight wear, over 20% shows up in games.");
    R::Text(640, y + 14, 1, COL_DIM, ALIGN_CENTER, "Triggers: resting above 30 of 255 counts as pressed in games.");
    R::Text(640, y + 28, 1, COL_DIM, ALIGN_CENTER, "The stick results also stay in each stick panel.");
    R::Text(640, y + 54, 2, COL_TEXT, ALIGN_CENTER, "A: close");
}

// The tester screen plus whatever box is open on top of it (also used by the PC layout preview).
static void DrawTester(const PadState& p, Mode mode)
{
    // Behind the menu and the test boxes the tester shows a resting controller: input there
    // drives the menu, not the test.
    static PadState shown;
    shown = p;
    if (mode != MODE_TEST)
        ZeroMemory(&shown.state.Gamepad, sizeof(shown.state.Gamepad));

    DrawHeader();
    DrawStickPanel(64, "LEFT STICK", shown, 0);
    DrawStickPanel(896, "RIGHT STICK", shown, 1);
    DrawController(shown, g_Active, p.connected);
    DrawChecklist(shown);
    DrawFooter();
    if (mode == MODE_MENU)   DrawMenu();
    if (mode == MODE_RUMBLE) DrawRumble(p);
    if (mode == MODE_DRIFT)  DrawDrift();
    if (mode == MODE_DRIFT_RESULT) DrawDriftResult(p);
}

//--------------------------------------------------------------------------------------
// Main
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    if (!R::Init())
        return;
    InitControllerShape();
    Sys_MountHdd();
    Sys_MigrateOldData();
    Theme_Load();

    DWORD last = GetTickCount();
    for (;;)
    {
        DWORD now = GetTickCount();
        float dt = Clamp((now - last) / 1000.0f, 0.0f, 0.1f);
        last = now;
        g_Time += dt;
        if (g_ToastTime > 0)
            g_ToastTime -= dt;

        PollPads(now);
        PadState& p = g_Pads[g_Active];

        // Holding BACK opens the menu; releasing it lets BACK be used again.
        bool back = p.connected && (p.state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
        if (!back)
            g_BackConsumed = false;

        if (!p.connected && g_Mode != MODE_TEST)
        {
            StopAllRumble();
            g_Mode = MODE_TEST;
        }

        switch (g_Mode)
        {
        case MODE_TEST:
            if (p.connected)
            {
                UpdateStats(p);
                g_BackHold = back && !g_BackConsumed ? g_BackHold + dt : 0.0f;
                if (g_BackHold >= 1.0f)
                {
                    g_BackHold = 0;
                    g_BackConsumed = true;
                    g_MenuSel = 0;
                    g_RepeatDir = 0;
                    g_Mode = MODE_MENU;
                }
            }
            break;
        case MODE_MENU:   UpdateMenu(p, dt); break;
        case MODE_RUMBLE: UpdateRumble(g_Active, p, dt); break;
        case MODE_DRIFT:  UpdateDrift(p, dt); break;
        case MODE_DRIFT_RESULT: UpdateDriftResult(p); break;
        case MODE_REMAP:
            if (!RemapUI_Update(p.state, p.prev, dt))
                g_Mode = MODE_MENU;
            break;
        }
        if (g_Mode != MODE_RUMBLE)
        {
            g_RumbleL = g_RumbleR = 0;
        }

        R::BeginFrame(COL_BG);
        if (g_Mode == MODE_REMAP)
        {
            RemapUI_Draw(p.state);
            R::EndFrame();
            continue;
        }
        DrawTester(p, g_Mode);
        R::EndFrame();
    }
}
