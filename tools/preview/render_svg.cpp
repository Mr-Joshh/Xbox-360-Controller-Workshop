// SVG backend for render.h, used only by the PC layout preview.
#include "../../src/render.h"
#include "../../src/font8x8.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <string>

std::string g_Svg;

static void Fill(DWORD c, char* out)
{
    sprintf(out, "fill=\"#%06x\" fill-opacity=\"%.3f\"", c & 0xFFFFFF, ((c >> 24) & 0xFF) / 255.0);
}

static void Add(const char* s)
{
    g_Svg += s;
    g_Svg += "\n";
}

namespace R
{
    bool Init() { return true; }

    void BeginFrame(DWORD clear)
    {
        char f[64], b[256];
        Fill(clear, f);
        g_Svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1280\" height=\"720\">";
        sprintf(b, "<rect width=\"1280\" height=\"720\" %s/>", f);
        Add(b);
    }

    void EndFrame() { g_Svg += "</svg>\n"; }

    void Tri(float x0, float y0, float x1, float y1, float x2, float y2, DWORD c)
    {
        char f[64], b[256];
        Fill(c, f);
        sprintf(b, "<polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\" %s/>", x0, y0, x1, y1, x2, y2, f);
        Add(b);
    }

    void Rect(float x, float y, float w, float h, DWORD c)
    {
        if (w <= 0 || h <= 0) return;
        char f[64], b[256];
        Fill(c, f);
        sprintf(b, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" %s/>", x, y, w, h, f);
        Add(b);
    }

    void RectOutline(float x, float y, float w, float h, float t, DWORD c)
    {
        Rect(x, y, w, t, c);
        Rect(x, y + h - t, w, t, c);
        Rect(x, y + t, t, h - 2 * t, c);
        Rect(x + w - t, y + t, t, h - 2 * t, c);
    }

    void RoundRect(float x, float y, float w, float h, float r, DWORD c)
    {
        if (r * 2 > w) r = w / 2;
        if (r * 2 > h) r = h / 2;
        char f[64], b[256];
        Fill(c, f);
        sprintf(b, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"%.1f\" %s/>", x, y, w, h, r, f);
        Add(b);
    }

    void Line(float x0, float y0, float x1, float y1, float t, DWORD c)
    {
        char b[256];
        sprintf(b, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#%06x\" stroke-opacity=\"%.3f\" stroke-width=\"%.1f\"/>",
                x0, y0, x1, y1, c & 0xFFFFFF, ((c >> 24) & 0xFF) / 255.0, t);
        Add(b);
    }

    void Circle(float cx, float cy, float r, DWORD c) { Ellipse(cx, cy, r, r, 0, c); }

    void Ring(float cx, float cy, float r, float t, DWORD c)
    {
        char b[256];
        sprintf(b, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" fill=\"none\" stroke=\"#%06x\" stroke-opacity=\"%.3f\" stroke-width=\"%.1f\"/>",
                cx, cy, r - t / 2, c & 0xFFFFFF, ((c >> 24) & 0xFF) / 255.0, t);
        Add(b);
    }

    void Ellipse(float cx, float cy, float rx, float ry, float a, DWORD c)
    {
        char f[64], b[256];
        Fill(c, f);
        sprintf(b, "<ellipse cx=\"%.1f\" cy=\"%.1f\" rx=\"%.1f\" ry=\"%.1f\" transform=\"rotate(%.2f %.1f %.1f)\" %s/>",
                cx, cy, rx, ry, a * 57.29578f, cx, cy, f);
        Add(b);
    }

    void Arc(float cx, float cy, float r, float a0, float a1, DWORD c)
    {
        const int n = 24;
        for (int i = 0; i < n; i++)
        {
            float t0 = a0 + (a1 - a0) * i / n, t1 = a0 + (a1 - a0) * (i + 1) / n;
            Tri(cx, cy, cx + cosf(t0) * r, cy + sinf(t0) * r, cx + cosf(t1) * r, cy + sinf(t1) * r, c);
        }
    }

    float TextWidth(float scale, const char* s) { return strlen(s) * 8.0f * scale; }

    void Text(float x, float y, float scale, DWORD c, TextAlign align, const char* s)
    {
        if (align == ALIGN_CENTER) x -= TextWidth(scale, s) / 2;
        else if (align == ALIGN_RIGHT) x -= TextWidth(scale, s);
        for (const unsigned char* p = (const unsigned char*)s; *p; p++, x += 8 * scale)
        {
            unsigned ch = *p < 0x20 || *p > 0x7F ? '?' : *p;
            for (int row = 0; row < 8; row++)
                for (int col = 0; col < 8; col++)
                    if (g_Font8x8[ch - 0x20][row] & (1 << col))
                        Rect(x + col * scale, y + row * scale, scale, scale, c);
        }
    }

    void Textf(float x, float y, float scale, DWORD c, TextAlign align, const char* fmt, ...)
    {
        char buf[256];
        va_list a;
        va_start(a, fmt);
        _vsnprintf(buf, 255, fmt, a);
        va_end(a);
        buf[255] = 0;
        Text(x, y, scale, c, align, buf);
    }

    DWORD Lerp(DWORD a, DWORD b, float t)
    {
        if (t <= 0) return a;
        if (t >= 1) return b;
        DWORD out = 0;
        for (int s = 0; s < 32; s += 8)
        {
            float ca = (float)((a >> s) & 0xFF), cb = (float)((b >> s) & 0xFF);
            out |= ((DWORD)(ca + (cb - ca) * t) & 0xFF) << s;
        }
        return out;
    }
}
