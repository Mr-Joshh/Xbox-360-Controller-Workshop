// remap_ui.h - the button remapper screens (game list, profile editor, try-it mode).
#pragma once
#include <xtl.h>

void RemapUI_Open();

// Returns false when the user leaves the remapper.
bool RemapUI_Update(const XINPUT_STATE& cur, const XINPUT_STATE& prev, float dt);

void RemapUI_Draw(const XINPUT_STATE& cur);
