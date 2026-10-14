// theme.h - colors shared by every screen, with a dark and a light palette.
#pragma once
#include "render.h"

struct Theme
{
    DWORD bg, panel, panelEdge, body, bodyEdge, part;
    DWORD text, dim, faint, accent, lit, onLit;
    DWORD cyan, orange, red, yellow;
    DWORD well;                         // plot and bar backgrounds
    DWORD shade;                        // full-screen dimming behind dialogs
    DWORD padFace, padUnder, padHalo;   // controller picture
    DWORD padRim;                       // dark rim around sticks, buttons and the Guide
    DWORD padWell, capOff, capRing;     // stick wells, stick caps, D-pad
    DWORD tryPanel;                     // remapper "trying it out" panel
    DWORD dimTarget;                    // unpressed face buttons fade toward this
    float dimAmount;
};

extern const Theme* g_Theme;

void Theme_Set(bool light);
bool Theme_IsLight();

// Reads / writes Hdd:\ControllerWorkshop\settings.ini (theme=dark|light).
void Theme_Load();
void Theme_Save();

#define COL_BG          (g_Theme->bg)
#define COL_PANEL       (g_Theme->panel)
#define COL_PANEL_EDGE  (g_Theme->panelEdge)
#define COL_BODY        (g_Theme->body)
#define COL_BODY_EDGE   (g_Theme->bodyEdge)
#define COL_PART        (g_Theme->part)
#define COL_TEXT        (g_Theme->text)
#define COL_DIM         (g_Theme->dim)
#define COL_FAINT       (g_Theme->faint)
#define COL_ACCENT      (g_Theme->accent)
#define COL_LIT         (g_Theme->lit)
#define COL_ON_LIT      (g_Theme->onLit)
#define COL_CYAN        (g_Theme->cyan)
#define COL_ORANGE      (g_Theme->orange)
#define COL_RED         (g_Theme->red)
#define COL_YELLOW      (g_Theme->yellow)
#define COL_WELL        (g_Theme->well)
#define COL_SHADE       (g_Theme->shade)
#define COL_PAD_FACE    (g_Theme->padFace)
#define COL_PAD_UNDER   (g_Theme->padUnder)
#define COL_PAD_HALO    (g_Theme->padHalo)
#define COL_PAD_RIM     (g_Theme->padRim)
#define COL_PAD_WELL    (g_Theme->padWell)
#define COL_CAP_OFF     (g_Theme->capOff)
#define COL_CAP_RING    (g_Theme->capRing)
#define COL_TRY_PANEL   (g_Theme->tryPanel)
#define COL_SELECTED    RGBA(255, 255, 255, 255)   // text on the green selection bar
