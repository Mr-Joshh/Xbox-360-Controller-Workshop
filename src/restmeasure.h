// restmeasure.h - guided resting-position measurement, used by the drift test and the dead zone
// editor. Both sticks are pushed UP, RIGHT, DOWN and LEFT in turn and let go; after each release
// the resting position is sampled. A worn stick settles in a different spot depending on where it
// came back from, so each direction is kept and the worst one is the stick's drift.
#pragma once
#include <xtl.h>

const int REST_STEPS = 4;
extern const char* const g_RestStepNames[REST_STEPS];   // "UP", "RIGHT", "DOWN", "LEFT"

enum RestPhase { RP_PUSH, RP_RELEASE, RP_SETTLE, RP_SAMPLE };

struct RestMeasure
{
    int       step;                     // direction being measured, REST_STEPS when done
    RestPhase phase;
    float     timer;
    float     stepMax[2];               // furthest from center during this step's sample
    float     dirMax[REST_STEPS][2];    // per direction and stick, fraction of full travel
    float     worst[2];                 // worst direction per stick
    BYTE      trigMax[2];               // highest trigger value seen while sampling (LT, RT)
};

void RestMeasure_Start(RestMeasure& m);

// Feeds one frame. Returns true once all four directions have been measured.
bool RestMeasure_Update(RestMeasure& m, const XINPUT_GAMEPAD& g, float dt);

// The measuring box (dimmed screen, instructions, progress, the four directions).
void RestMeasure_Draw(const RestMeasure& m, const char* title);
