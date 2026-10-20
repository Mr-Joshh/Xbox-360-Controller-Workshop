// deadzone_ui.h - the stick dead zone screens (game list, dead zone editor, drift measurement).
#pragma once
#include <xtl.h>

void DeadzoneUI_Open();

// Returns false when the user leaves the dead zone screens.
bool DeadzoneUI_Update(const XINPUT_STATE& cur, const XINPUT_STATE& prev, float dt);

void DeadzoneUI_Draw(const XINPUT_STATE& cur);
