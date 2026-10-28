// trigger_ui.h - the trigger configuration screens (game list, trigger range editor, measurement).
#pragma once
#include <xtl.h>

void TriggerUI_Open();

// Returns false when the user leaves the trigger screens.
bool TriggerUI_Update(const XINPUT_STATE& cur, const XINPUT_STATE& prev, float dt);

void TriggerUI_Draw(const XINPUT_STATE& cur);
