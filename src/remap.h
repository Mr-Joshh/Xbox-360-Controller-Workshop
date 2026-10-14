// remap.h - button remap profiles, shared by the Controller Workshop app and the remap plugin.
//
// A profile is a small INI file, one per game:  Hdd:\ControllerWorkshop\Remap\<TITLEID>.ini
// ALL.ini in the same folder applies to every game that has no profile of its own.
//
//   name=Halo 3            (shown in the app; ignored by the plugin)
//   A=B                    (physical A acts as B)
//   LT=RT
//   X=NONE                 (disabled)
//   swap_sticks=1
//   invert_ly=0
//   invert_ry=1
#pragma once
#include <xtl.h>

enum
{
    IN_A, IN_B, IN_X, IN_Y, IN_LB, IN_RB, IN_LS, IN_RS, IN_BACK, IN_START,
    IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_LT, IN_RT, IN_COUNT
};
#define MAP_NONE (-1)

struct RemapProfile
{
    signed char target[IN_COUNT];   // output for each physical input, or MAP_NONE
    bool        swapSticks;
    bool        invertLY;
    bool        invertRY;
};

extern const char* const g_InputNames[IN_COUNT];   // "A", "B", ... "LT", "RT"

#define CT_DIR_DEVICE  "\\Device\\Harddisk0\\Partition1\\ControllerWorkshop"
#define CT_DIR_HDD     "Hdd:\\ControllerWorkshop"
#define CT_REMAP_ALL   "ALL"
#define CT_PLUGIN_NAME "ControllerWorkshopPlugin.xex"

void Remap_Default(RemapProfile* p);
bool Remap_IsDefault(const RemapProfile* p);

// Parses INI text into p (starting from the default). Copies the name= value if 'name' is given.
void Remap_Parse(const char* text, RemapProfile* p, char* name, int nameSize);

// Writes the profile as INI text. Returns the length.
int Remap_Write(const RemapProfile* p, const char* name, char* out, int outSize);

// Rewrites a controller state according to the profile.
void Remap_Apply(const RemapProfile* p, XINPUT_GAMEPAD* g);
