// render.h - minimal immediate-mode 2D renderer: colored triangles in 1280x720 pixel space,
// plus text from the built-in 8x8 font. No textures or media files are needed.
#pragma once
#include <xtl.h>

#define SCREEN_W 1280
#define SCREEN_H 720

// Colors are 0xAARRGGBB.
#define RGBA(r, g, b, a) ((DWORD)(((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))

enum TextAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

namespace R
{
    bool Init();
    void BeginFrame(DWORD clearColor);
    void EndFrame();

    void Tri(float x0, float y0, float x1, float y1, float x2, float y2, DWORD color);
    void Rect(float x, float y, float w, float h, DWORD color);
    void RectOutline(float x, float y, float w, float h, float thick, DWORD color);
    void RoundRect(float x, float y, float w, float h, float radius, DWORD color);
    void Line(float x0, float y0, float x1, float y1, float thick, DWORD color);
    void Circle(float cx, float cy, float r, DWORD color);
    void Ring(float cx, float cy, float r, float thick, DWORD color);
    // Ellipse rotated by 'angle' radians around its center.
    void Ellipse(float cx, float cy, float rx, float ry, float angle, DWORD color);
    // Pie slice from angle a0 to a1 (radians, 0 = right, clockwise on screen).
    void Arc(float cx, float cy, float r, float a0, float a1, DWORD color);

    // Text with the 8x8 font. 'scale' is the pixel size of one font pixel (2 = 16px glyphs).
    void Text(float x, float y, float scale, DWORD color, TextAlign align, const char* str);
    void Textf(float x, float y, float scale, DWORD color, TextAlign align, const char* fmt, ...);
    float TextWidth(float scale, const char* str);

    DWORD Lerp(DWORD a, DWORD b, float t);
}
