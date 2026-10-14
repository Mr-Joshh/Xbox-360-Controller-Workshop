// remap.cpp - see remap.h
#include "remap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char* const g_InputNames[IN_COUNT] =
{
    "A", "B", "X", "Y", "LB", "RB", "LS", "RS", "BACK", "START",
    "UP", "DOWN", "LEFT", "RIGHT", "LT", "RT"
};

static const WORD g_Masks[IN_COUNT] =
{
    XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
    XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
    XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
    XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT,
    0, 0
};

void Remap_Default(RemapProfile* p)
{
    for (int i = 0; i < IN_COUNT; i++)
        p->target[i] = (signed char)i;
    p->swapSticks = p->invertLY = p->invertRY = false;
}

bool Remap_IsDefault(const RemapProfile* p)
{
    for (int i = 0; i < IN_COUNT; i++)
        if (p->target[i] != i)
            return false;
    return !p->swapSticks && !p->invertLY && !p->invertRY;
}

static int FindInput(const char* s)
{
    for (int i = 0; i < IN_COUNT; i++)
        if (_stricmp(s, g_InputNames[i]) == 0)
            return i;
    return -2;
}

static void Trim(char* s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
        s[--n] = 0;
    int start = 0;
    while (s[start] == ' ' || s[start] == '\t')
        start++;
    if (start)
        memmove(s, s + start, n - start + 1);
}

void Remap_Parse(const char* text, RemapProfile* p, char* name, int nameSize)
{
    Remap_Default(p);
    if (name && nameSize)
        name[0] = 0;

    while (*text)
    {
        char line[160];
        int n = 0;
        while (*text && *text != '\n')
        {
            if (n < (int)sizeof(line) - 1)
                line[n++] = *text;
            text++;
        }
        if (*text == '\n')
            text++;
        line[n] = 0;

        char* eq = strchr(line, '=');
        if (line[0] == ';' || line[0] == '#' || !eq)
            continue;
        *eq = 0;
        char* key = line;
        char* value = eq + 1;
        Trim(key);
        Trim(value);

        if (_stricmp(key, "name") == 0)
        {
            if (name && nameSize)
            {
                strncpy(name, value, nameSize - 1);
                name[nameSize - 1] = 0;
            }
        }
        else if (_stricmp(key, "swap_sticks") == 0)
            p->swapSticks = atoi(value) != 0;
        else if (_stricmp(key, "invert_ly") == 0)
            p->invertLY = atoi(value) != 0;
        else if (_stricmp(key, "invert_ry") == 0)
            p->invertRY = atoi(value) != 0;
        else
        {
            int src = FindInput(key);
            if (src < 0)
                continue;
            int dst = _stricmp(value, "NONE") == 0 ? MAP_NONE : FindInput(value);
            if (dst != -2)
                p->target[src] = (signed char)dst;
        }
    }
}

int Remap_Write(const RemapProfile* p, const char* name, char* out, int outSize)
{
    int n = _snprintf(out, outSize, "; Controller Workshop remap profile\r\nname=%s\r\n", name ? name : "");
    for (int i = 0; i < IN_COUNT && n < outSize; i++)
        n += _snprintf(out + n, outSize - n, "%s=%s\r\n", g_InputNames[i],
                       p->target[i] == MAP_NONE ? "NONE" : g_InputNames[p->target[i]]);
    if (n < outSize)
        n += _snprintf(out + n, outSize - n, "swap_sticks=%d\r\ninvert_ly=%d\r\ninvert_ry=%d\r\n",
                       p->swapSticks ? 1 : 0, p->invertLY ? 1 : 0, p->invertRY ? 1 : 0);
    if (n > outSize - 1)
        n = outSize - 1;
    out[n] = 0;
    return n;
}

static SHORT Invert(SHORT v)
{
    return v == -32768 ? 32767 : (SHORT)-v;
}

void Remap_Apply(const RemapProfile* p, XINPUT_GAMEPAD* g)
{
    const XINPUT_GAMEPAD in = *g;

    // Value of every physical input on a 0..255 scale (buttons are 0 or 255).
    BYTE value[IN_COUNT];
    for (int i = 0; i < IN_LT; i++)
        value[i] = (in.wButtons & g_Masks[i]) ? 255 : 0;
    value[IN_LT] = in.bLeftTrigger;
    value[IN_RT] = in.bRightTrigger;

    // Keep bits we don't remap (e.g. the Guide button) as they are.
    WORD known = 0;
    for (int i = 0; i < IN_LT; i++)
        known |= g_Masks[i];
    g->wButtons = in.wButtons & ~known;
    g->bLeftTrigger = 0;
    g->bRightTrigger = 0;

    for (int src = 0; src < IN_COUNT; src++)
    {
        int dst = p->target[src];
        if (dst == MAP_NONE || value[src] == 0)
            continue;
        if (dst == IN_LT)
        {
            if (value[src] > g->bLeftTrigger) g->bLeftTrigger = value[src];
        }
        else if (dst == IN_RT)
        {
            if (value[src] > g->bRightTrigger) g->bRightTrigger = value[src];
        }
        else
        {
            // A trigger driving a button presses it past the standard threshold.
            bool down = src < IN_LT || value[src] > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
            if (down)
                g->wButtons |= g_Masks[dst];
        }
    }

    if (p->swapSticks)
    {
        g->sThumbLX = in.sThumbRX;
        g->sThumbLY = in.sThumbRY;
        g->sThumbRX = in.sThumbLX;
        g->sThumbRY = in.sThumbLY;
    }
    if (p->invertLY)
        g->sThumbLY = Invert(g->sThumbLY);
    if (p->invertRY)
        g->sThumbRY = Invert(g->sThumbRY);
}
