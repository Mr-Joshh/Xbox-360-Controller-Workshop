// gamelist_ui.h - pieces shared by the remapper and dead zone screens: the game list, dialogs,
// clipped text and D-pad key repeat.
#pragma once
#include <xtl.h>
#include "games.h"

const int GAMELIST_MAX  = 256;
const int GAMELIST_ROWS = 9;

struct GameList
{
    GameEntry games[GAMELIST_MAX];
    int       count;
    int       sel, top;
};

// Reloads the games (hasProfile = has a profile in 'folder') and keeps the selection in range.
void GameList_Load(GameList& list, const char* folder);

// Moves the selection: 'U'/'D' one row, 'L'/'R' one page.
void GameList_Move(GameList& list, char nav);

// Title, plugin status, the list (games with a profile are marked CUSTOM) and the key hints.
// 'allGamesNote' explains what the "All games" entry does.
void GameList_Draw(GameList& list, const char* title, bool pluginLoaded, const char* allGamesNote);

// Centered dialog over a dimmed screen.
void UI_DrawModal(const char* title, const char* line, const char* keys);

// Text cut to maxChars with "..." at the end.
void UI_TextClipped(float x, float y, float scale, DWORD color, int maxChars, const char* s);

// D-pad (and optionally left stick) with key repeat. Returns 'U', 'D', 'L', 'R' or 0.
struct NavRepeat
{
    int   dir;
    float timer;
};
char UI_Nav(NavRepeat& repeat, const XINPUT_GAMEPAD& g, float dt, bool useStick);
