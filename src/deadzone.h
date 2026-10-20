// deadzone.h - stick dead zone profiles, shared by the Controller Workshop app and the plugin.
//
// A profile is a small INI file, one per game:  Hdd:\ControllerWorkshop\Deadzone\<TITLEID>.ini
// ALL.ini in the same folder applies to every game that has no dead zone profile of its own.
// Dead zone and remap profiles are separate, so one drifting stick can be fixed for every game
// while each game keeps its own button layout.
//
//   name=Halo 3            (shown in the app; ignored by the plugin)
//   left_stick=30          (percent of full travel, 0 = off)
//   right_stick=12
#pragma once
#include <xtl.h>

#define CT_DEADZONE_FOLDER "Deadzone"
#define DZ_MAX_PERCENT     50

struct DeadzoneProfile
{
    int left;       // percent, 0..DZ_MAX_PERCENT
    int right;
};

void Deadzone_Default(DeadzoneProfile* p);
bool Deadzone_IsDefault(const DeadzoneProfile* p);

// Parses INI text into p (starting from the default). Copies the name= value if 'name' is given.
void Deadzone_Parse(const char* text, DeadzoneProfile* p, char* name, int nameSize);

// Writes the profile as INI text. Returns the length.
int Deadzone_Write(const DeadzoneProfile* p, const char* name, char* out, int outSize);

// Applies the dead zones to the physical sticks (before any remapping). A stick inside its dead
// zone reads as centered. Outside it, the remaining travel is stretched so that it starts where
// a game's own standard dead zone ends, so movement picks up smoothly instead of jumping.
void Deadzone_Apply(const DeadzoneProfile* p, XINPUT_GAMEPAD* g);
