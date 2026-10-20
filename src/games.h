// games.h - the list of games the remapper and the dead zone screen offer.
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
// have a profile in 'folder' (CT_REMAP_FOLDER or CT_DEADZONE_FOLDER), sorted by name.
// hasProfile refers to that folder. Returns the count.
int Games_Load(GameEntry* out, int max, const char* folder);

// Hdd:\ControllerWorkshop\<folder>\<TITLEID>.ini, or ALL.ini for titleId 0.
void Games_ProfilePath(const char* folder, DWORD titleId, char* out, int size);
