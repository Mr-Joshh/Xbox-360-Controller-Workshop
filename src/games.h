// games.h - the list of games the remapper offers.
#pragma once
#include <xtl.h>

struct GameEntry
{
    DWORD titleId;      // 0 = "All games" (ALL.ini)
    char  name[64];
    bool  hasProfile;
};

// Builds the list: "All games" first, then every game the plugin has seen (titles.txt),
// games installed on the hard drive (Content\0000000000000000), and games that already
// have a profile, sorted by name. Returns the count.
int Games_Load(GameEntry* out, int max);

// Hdd:\ControllerWorkshop\Remap\<TITLEID>.ini, or ALL.ini for titleId 0.
void Games_ProfilePath(DWORD titleId, char* out, int size);
