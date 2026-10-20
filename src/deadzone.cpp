// deadzone.cpp - see deadzone.h
#include "deadzone.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

void Deadzone_Default(DeadzoneProfile* p)
{
    p->left = p->right = 0;
}

bool Deadzone_IsDefault(const DeadzoneProfile* p)
{
    return p->left == 0 && p->right == 0;
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

static int ClampPercent(int v)
{
    return v < 0 ? 0 : (v > DZ_MAX_PERCENT ? DZ_MAX_PERCENT : v);
}

void Deadzone_Parse(const char* text, DeadzoneProfile* p, char* name, int nameSize)
{
    Deadzone_Default(p);
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
        else if (_stricmp(key, "left_stick") == 0)
            p->left = ClampPercent(atoi(value));
        else if (_stricmp(key, "right_stick") == 0)
            p->right = ClampPercent(atoi(value));
    }
}

int Deadzone_Write(const DeadzoneProfile* p, const char* name, char* out, int outSize)
{
    int n = _snprintf(out, outSize, "; Controller Workshop dead zone profile (percent, 0 = off)\r\n"
                      "name=%s\r\nleft_stick=%d\r\nright_stick=%d\r\n", name ? name : "", p->left, p->right);
    if (n < 0 || n > outSize - 1)
        n = outSize - 1;
    out[n] = 0;
    return n;
}

static SHORT ToShort(float v)
{
    return v >= 32767.0f ? 32767 : (v <= -32768.0f ? -32768 : (SHORT)v);
}

// 'gameEdge' is the standard XInput dead zone for this stick, as a fraction of full travel.
static void ApplyStick(int percent, float gameEdge, SHORT* x, SHORT* y)
{
    if (percent <= 0)
        return;
    float fx = *x / 32767.0f, fy = *y / 32767.0f;
    float mag = sqrtf(fx * fx + fy * fy);
    float dz = percent / 100.0f;
    if (mag <= dz)
    {
        *x = *y = 0;
        return;
    }
    // Map [dz, 1] onto [edge, 1]. A dead zone smaller than the game's own passes the stick through
    // unchanged; a larger one starts right at the game's edge instead of stacking the two.
    float edge = dz < gameEdge ? dz : gameEdge;
    float out = edge + (mag - dz) * (1.0f - edge) / (1.0f - dz);
    float scale = out / mag * 32767.0f;
    *x = ToShort(fx * scale);
    *y = ToShort(fy * scale);
}

void Deadzone_Apply(const DeadzoneProfile* p, XINPUT_GAMEPAD* g)
{
    ApplyStick(p->left, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32767.0f, &g->sThumbLX, &g->sThumbLY);
    ApplyStick(p->right, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32767.0f, &g->sThumbRX, &g->sThumbRY);
}
