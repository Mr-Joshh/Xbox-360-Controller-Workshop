// trigger.cpp - see trigger.h
#include "trigger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void Trigger_Default(TriggerProfile* p)
{
    p->start[0] = p->start[1] = 0;
    p->full[0] = p->full[1] = 255;
}

bool Trigger_IsDefault(const TriggerProfile* p)
{
    return p->start[0] == 0 && p->start[1] == 0 && p->full[0] == 255 && p->full[1] == 255;
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

static int Clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void Trigger_Parse(const char* text, TriggerProfile* p, char* name, int nameSize)
{
    Trigger_Default(p);
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
        else if (_stricmp(key, "lt_start") == 0) p->start[0] = Clamp(atoi(value), 0, TRIG_MAX_START);
        else if (_stricmp(key, "rt_start") == 0) p->start[1] = Clamp(atoi(value), 0, TRIG_MAX_START);
        else if (_stricmp(key, "lt_full") == 0)  p->full[0] = Clamp(atoi(value), TRIG_MIN_FULL, 255);
        else if (_stricmp(key, "rt_full") == 0)  p->full[1] = Clamp(atoi(value), TRIG_MIN_FULL, 255);
    }
}

int Trigger_Write(const TriggerProfile* p, const char* name, char* out, int outSize)
{
    int n = _snprintf(out, outSize, "; Controller Workshop trigger profile (raw values 0-255)\r\n"
                      "name=%s\r\nlt_start=%d\r\nlt_full=%d\r\nrt_start=%d\r\nrt_full=%d\r\n",
                      name ? name : "", p->start[0], p->full[0], p->start[1], p->full[1]);
    if (n < 0 || n > outSize - 1)
        n = outSize - 1;
    out[n] = 0;
    return n;
}

BYTE Trigger_Map(const TriggerProfile* p, int trigger, BYTE value)
{
    int start = p->start[trigger], full = p->full[trigger];
    if (value <= start)
        return 0;
    if (value >= full)
        return 255;
    // TRIG_MAX_START < TRIG_MIN_FULL, so full - start is never zero.
    return (BYTE)(((value - start) * 255 + (full - start) / 2) / (full - start));
}

void Trigger_Apply(const TriggerProfile* p, XINPUT_GAMEPAD* g)
{
    g->bLeftTrigger = Trigger_Map(p, 0, g->bLeftTrigger);
    g->bRightTrigger = Trigger_Map(p, 1, g->bRightTrigger);
}
