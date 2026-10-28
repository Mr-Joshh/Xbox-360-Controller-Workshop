// trigger.h - trigger range profiles, shared by the Controller Workshop app and the plugin.
//
// A profile is a small INI file, one per game:  Hdd:\ControllerWorkshop\Trigger\<TITLEID>.ini
// ALL.ini in the same folder applies to every game that has no trigger profile of its own.
// Like dead zones, these fix a worn controller, so they are kept apart from the button remaps.
//
//   name=Halo 3            (shown in the app; ignored by the plugin)
//   lt_start=0             (raw value 0-255 the trigger must pass before the game sees it)
//   lt_full=230            (raw value that already counts as fully pressed)
//   rt_start=12
//   rt_full=255
#pragma once
#include <xtl.h>

#define CT_TRIGGER_FOLDER  "Trigger"
#define TRIG_MAX_START     100
#define TRIG_MIN_FULL      128

struct TriggerProfile
{
    int start[2];   // LT, RT: 0..TRIG_MAX_START
    int full[2];    // LT, RT: TRIG_MIN_FULL..255
};

void Trigger_Default(TriggerProfile* p);
bool Trigger_IsDefault(const TriggerProfile* p);

// Parses INI text into p (starting from the default). Copies the name= value if 'name' is given.
void Trigger_Parse(const char* text, TriggerProfile* p, char* name, int nameSize);

// Writes the profile as INI text. Returns the length.
int Trigger_Write(const TriggerProfile* p, const char* name, char* out, int outSize);

// One trigger value through the profile: at or below start reads 0, at or above full reads 255,
// and the travel in between is stretched to cover 0-255.
BYTE Trigger_Map(const TriggerProfile* p, int trigger, BYTE value);

// Applies the profile to the physical triggers (before any remapping).
void Trigger_Apply(const TriggerProfile* p, XINPUT_GAMEPAD* g);
