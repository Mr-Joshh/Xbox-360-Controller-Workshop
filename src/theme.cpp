// theme.cpp - see theme.h
#include "theme.h"
#include "system.h"
#include "remap.h"
#include <string.h>

static const Theme g_Dark =
{
    RGBA(16, 18, 22, 255),      // bg
    RGBA(28, 31, 37, 255),      // panel
    RGBA(48, 53, 62, 255),      // panelEdge
    RGBA(52, 56, 64, 255),      // body
    RGBA(92, 99, 112, 255),     // bodyEdge
    RGBA(30, 32, 37, 255),      // part
    RGBA(225, 228, 235, 255),   // text
    RGBA(130, 136, 148, 255),   // dim
    RGBA(70, 75, 86, 255),      // faint
    RGBA(16, 124, 16, 255),     // accent (Xbox green)
    RGBA(110, 220, 90, 255),    // lit
    RGBA(20, 20, 20, 255),      // onLit
    RGBA(70, 190, 230, 255),    // cyan
    RGBA(240, 150, 40, 255),    // orange
    RGBA(230, 70, 60, 255),     // red
    RGBA(235, 200, 60, 255),    // yellow
    RGBA(20, 22, 27, 255),      // well
    RGBA(0, 0, 0, 170),         // shade
    RGBA(54, 58, 66, 255),      // padFace
    RGBA(36, 39, 45, 255),      // padUnder
    RGBA(66, 71, 81, 255),      // padHalo
    RGBA(15, 15, 15, 255),      // padRim
    RGBA(30, 32, 37, 255),      // padWell
    RGBA(72, 77, 87, 255),      // capOff
    RGBA(98, 104, 116, 255),    // capRing
    RGBA(16, 60, 16, 255),      // tryPanel
    RGBA(30, 32, 37, 255),      // dimTarget
    0.7f,                       // dimAmount
};

// Light palette, modeled on the white Xbox 360 controller.
static const Theme g_Light =
{
    RGBA(226, 229, 234, 255),   // bg
    RGBA(247, 248, 250, 255),   // panel
    RGBA(196, 201, 210, 255),   // panelEdge
    RGBA(205, 208, 213, 255),   // body
    RGBA(40, 42, 46, 255),      // bodyEdge
    RGBA(222, 225, 230, 255),   // part
    RGBA(24, 27, 33, 255),      // text
    RGBA(92, 98, 110, 255),     // dim
    RGBA(178, 183, 192, 255),   // faint
    RGBA(16, 124, 16, 255),     // accent
    RGBA(46, 170, 56, 255),     // lit
    RGBA(255, 255, 255, 255),   // onLit
    RGBA(0, 120, 180, 255),     // cyan
    RGBA(205, 105, 0, 255),     // orange
    RGBA(200, 40, 30, 255),     // red
    RGBA(170, 125, 0, 255),     // yellow
    RGBA(234, 236, 240, 255),   // well
    RGBA(40, 44, 52, 120),      // shade
    RGBA(204, 207, 212, 255),   // padFace
    RGBA(140, 144, 152, 255),   // padUnder
    RGBA(220, 223, 228, 255),   // padHalo
    RGBA(25, 25, 25, 255),      // padRim
    RGBA(52, 54, 58, 255),      // padWell
    RGBA(78, 81, 88, 255),      // capOff
    RGBA(105, 109, 117, 255),   // capRing
    RGBA(205, 236, 205, 255),   // tryPanel
    RGBA(255, 255, 255, 255),   // dimTarget
    0.45f,                      // dimAmount
};

const Theme* g_Theme = &g_Dark;

#define SETTINGS_FILE CT_DIR_HDD "\\settings.ini"

void Theme_Set(bool light)
{
    g_Theme = light ? &g_Light : &g_Dark;
}

bool Theme_IsLight()
{
    return g_Theme == &g_Light;
}

void Theme_Load()
{
    char buf[512];
    if (Sys_ReadFile(SETTINGS_FILE, buf, sizeof(buf)) > 0)
        Theme_Set(strstr(buf, "theme=light") != NULL);
}

void Theme_Save()
{
    Sys_CreateDir(CT_DIR_HDD);
    const char* text = Theme_IsLight() ? "theme=light\r\n" : "theme=dark\r\n";
    Sys_WriteFile(SETTINGS_FILE, text, (int)strlen(text));
}
