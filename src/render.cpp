// render.cpp - see render.h
#include "render.h"
#include "font8x8.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

namespace
{
    struct Vertex
    {
        float x, y;
        DWORD color;
    };

    // Vertex position arrives in pixels; c0 = (2/W, -2/H, -1, 1) maps it to clip space.
    const char g_VS[] =
        "vs.2.0\n"
        "dcl_position v0\n"
        "dcl_color v1\n"
        "mad r0.xy, v0.xy, c0.xy, c0.zw\n"
        "mov r0.zw, c1\n"
        "mov oPos, r0\n"
        "mov oD0, v1\n";

    const char g_PS[] =
        "ps.2.0\n"
        "dcl v0\n"
        "mov oC0, v0\n";

    const int MAX_VERTS = 3 * 4096;

    IDirect3D9*                  g_pD3D;
    IDirect3DDevice9*            g_pDev;
    IDirect3DVertexShader9*      g_pVS;
    IDirect3DPixelShader9*       g_pPS;
    IDirect3DVertexDeclaration9* g_pDecl;
    Vertex                       g_Verts[MAX_VERTS];
    int                          g_Count;

    void Flush()
    {
        if (g_Count == 0)
            return;
        g_pDev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, g_Count / 3, g_Verts, sizeof(Vertex));
        g_Count = 0;
    }

    inline void Push(float x, float y, DWORD c)
    {
        Vertex& v = g_Verts[g_Count++];
        v.x = x;
        v.y = y;
        v.color = c;
    }

    inline void Reserve(int verts)
    {
        if (g_Count + verts > MAX_VERTS)
            Flush();
    }

    int Segments(float r)
    {
        int n = (int)(r * 0.7f);
        if (n < 16) n = 16;
        if (n > 72) n = 72;
        return n;
    }

    bool MakeShader(const char* src, bool vertex)
    {
        ID3DXBuffer* code = NULL;
        ID3DXBuffer* errors = NULL;
        if (FAILED(D3DXAssembleShader(src, (UINT)strlen(src), NULL, NULL, 0, &code, &errors)))
        {
            OutputDebugStringA(errors ? (const char*)errors->GetBufferPointer() : "shader assemble failed\n");
            return false;
        }
        HRESULT hr = vertex ? g_pDev->CreateVertexShader((DWORD*)code->GetBufferPointer(), &g_pVS)
                            : g_pDev->CreatePixelShader((DWORD*)code->GetBufferPointer(), &g_pPS);
        code->Release();
        return SUCCEEDED(hr);
    }
}

namespace R
{
    bool Init()
    {
        g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
        if (!g_pD3D)
            return false;

        // 2x MSAA with no depth buffer fits in EDRAM at 720p without predicated tiling.
        D3DPRESENT_PARAMETERS pp;
        ZeroMemory(&pp, sizeof(pp));
        pp.BackBufferWidth = SCREEN_W;
        pp.BackBufferHeight = SCREEN_H;
        pp.BackBufferFormat = D3DFMT_A8R8G8B8;
        pp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
        pp.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
        pp.BackBufferCount = 1;
        pp.EnableAutoDepthStencil = FALSE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

        if (FAILED(g_pD3D->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_pDev)))
            return false;
        if (!MakeShader(g_VS, true) || !MakeShader(g_PS, false))
            return false;

        static const D3DVERTEXELEMENT9 elements[] =
        {
            { 0, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 8, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
            D3DDECL_END()
        };
        return SUCCEEDED(g_pDev->CreateVertexDeclaration(elements, &g_pDecl));
    }

    void BeginFrame(DWORD clearColor)
    {
        g_pDev->Clear(0, NULL, D3DCLEAR_TARGET, clearColor, 1.0f, 0);

        g_pDev->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        g_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        g_pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        g_pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

        g_pDev->SetVertexShader(g_pVS);
        g_pDev->SetPixelShader(g_pPS);
        g_pDev->SetVertexDeclaration(g_pDecl);

        const float c[8] = { 2.0f / SCREEN_W, -2.0f / SCREEN_H, -1.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 1.0f };
        g_pDev->SetVertexShaderConstantF(0, c, 2);
        g_Count = 0;
    }

    void EndFrame()
    {
        Flush();
        g_pDev->Present(NULL, NULL, NULL, NULL);
    }

    void Tri(float x0, float y0, float x1, float y1, float x2, float y2, DWORD color)
    {
        Reserve(3);
        Push(x0, y0, color);
        Push(x1, y1, color);
        Push(x2, y2, color);
    }

    void Rect(float x, float y, float w, float h, DWORD color)
    {
        if (w <= 0 || h <= 0)
            return;
        Reserve(6);
        Push(x, y, color);         Push(x + w, y, color);     Push(x + w, y + h, color);
        Push(x, y, color);         Push(x + w, y + h, color); Push(x, y + h, color);
    }

    void RectOutline(float x, float y, float w, float h, float t, DWORD color)
    {
        Rect(x, y, w, t, color);
        Rect(x, y + h - t, w, t, color);
        Rect(x, y + t, t, h - 2 * t, color);
        Rect(x + w - t, y + t, t, h - 2 * t, color);
    }

    void RoundRect(float x, float y, float w, float h, float r, DWORD color)
    {
        if (r * 2 > w) r = w / 2;
        if (r * 2 > h) r = h / 2;
        Rect(x + r, y, w - 2 * r, h, color);
        Rect(x, y + r, r, h - 2 * r, color);
        Rect(x + w - r, y + r, r, h - 2 * r, color);
        const float PI = 3.14159265f;
        Arc(x + r,     y + r,     r, PI,        1.5f * PI, color);
        Arc(x + w - r, y + r,     r, 1.5f * PI, 2.0f * PI, color);
        Arc(x + w - r, y + h - r, r, 0.0f,      0.5f * PI, color);
        Arc(x + r,     y + h - r, r, 0.5f * PI, PI,        color);
    }

    void Line(float x0, float y0, float x1, float y1, float t, DWORD color)
    {
        float dx = x1 - x0, dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f)
            return;
        float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
        Reserve(6);
        Push(x0 + nx, y0 + ny, color); Push(x1 + nx, y1 + ny, color); Push(x1 - nx, y1 - ny, color);
        Push(x0 + nx, y0 + ny, color); Push(x1 - nx, y1 - ny, color); Push(x0 - nx, y0 - ny, color);
    }

    void Arc(float cx, float cy, float r, float a0, float a1, DWORD color)
    {
        int n = (int)(Segments(r) * (a1 - a0) / 6.2831853f) + 2;
        float step = (a1 - a0) / n;
        float px = cx + cosf(a0) * r, py = cy + sinf(a0) * r;
        for (int i = 1; i <= n; i++)
        {
            float a = a0 + step * i;
            float qx = cx + cosf(a) * r, qy = cy + sinf(a) * r;
            Tri(cx, cy, px, py, qx, qy, color);
            px = qx;
            py = qy;
        }
    }

    void Circle(float cx, float cy, float r, DWORD color)
    {
        Ellipse(cx, cy, r, r, 0.0f, color);
    }

    void Ellipse(float cx, float cy, float rx, float ry, float angle, DWORD color)
    {
        int n = Segments(rx > ry ? rx : ry);
        float ca = cosf(angle), sa = sinf(angle);
        float px = cx + rx * ca, py = cy + rx * sa;
        for (int i = 1; i <= n; i++)
        {
            float t = 6.2831853f * i / n;
            float ex = cosf(t) * rx, ey = sinf(t) * ry;
            float qx = cx + ex * ca - ey * sa, qy = cy + ex * sa + ey * ca;
            Tri(cx, cy, px, py, qx, qy, color);
            px = qx;
            py = qy;
        }
    }

    void Ring(float cx, float cy, float r, float t, DWORD color)
    {
        int n = Segments(r);
        float ri = r - t;
        for (int i = 0; i < n; i++)
        {
            float a0 = 6.2831853f * i / n, a1 = 6.2831853f * (i + 1) / n;
            float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
            Tri(cx + c0 * r, cy + s0 * r, cx + c1 * r, cy + s1 * r, cx + c1 * ri, cy + s1 * ri, color);
            Tri(cx + c0 * r, cy + s0 * r, cx + c1 * ri, cy + s1 * ri, cx + c0 * ri, cy + s0 * ri, color);
        }
    }

    float TextWidth(float scale, const char* str)
    {
        return (float)strlen(str) * 8.0f * scale;
    }

    void Text(float x, float y, float scale, DWORD color, TextAlign align, const char* str)
    {
        if (align == ALIGN_CENTER)
            x -= TextWidth(scale, str) * 0.5f;
        else if (align == ALIGN_RIGHT)
            x -= TextWidth(scale, str);

        for (const unsigned char* p = (const unsigned char*)str; *p; p++, x += 8 * scale)
        {
            unsigned c = *p;
            if (c < 0x20 || c > 0x7F)
                c = '?';
            const unsigned char* glyph = g_Font8x8[c - 0x20];
            for (int row = 0; row < 8; row++)
            {
                unsigned bits = glyph[row];
                // Merge runs of lit pixels into one quad.
                for (int col = 0; col < 8;)
                {
                    if (!(bits & (1u << col)))
                    {
                        col++;
                        continue;
                    }
                    int start = col;
                    while (col < 8 && (bits & (1u << col)))
                        col++;
                    Rect(x + start * scale, y + row * scale, (col - start) * scale, scale, color);
                }
            }
        }
    }

    void Textf(float x, float y, float scale, DWORD color, TextAlign align, const char* fmt, ...)
    {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        _vsnprintf(buf, sizeof(buf) - 1, fmt, args);
        va_end(args);
        buf[sizeof(buf) - 1] = 0;
        Text(x, y, scale, color, align, buf);
    }

    DWORD Lerp(DWORD a, DWORD b, float t)
    {
        if (t <= 0) return a;
        if (t >= 1) return b;
        DWORD out = 0;
        for (int shift = 0; shift < 32; shift += 8)
        {
            float ca = (float)((a >> shift) & 0xFF), cb = (float)((b >> shift) & 0xFF);
            out |= ((DWORD)(ca + (cb - ca) * t) & 0xFF) << shift;
        }
        return out;
    }
}
