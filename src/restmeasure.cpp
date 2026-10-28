// restmeasure.cpp - see restmeasure.h
#include "restmeasure.h"
#include "render.h"
#include "theme.h"
#include <math.h>
#include <string.h>

const char* const g_RestStepNames[REST_STEPS] = { "UP", "RIGHT", "DOWN", "LEFT" };

namespace
{
    const float PUSHED = 0.6f;    // stick travel toward the asked direction that counts as pushed
    const float LET_GO = 0.55f;   // above this the stick is still being held (dead zones stop at 50%)
    const float SETTLE = 0.3f;    // seconds for the stick to spring back
    const float SAMPLE = 0.7f;    // seconds of sampling per direction
    const float g_StepDir[REST_STEPS][2] = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } };

    float Magnitude(SHORT x, SHORT y)
    {
        float fx = x / 32767.0f, fy = y / 32767.0f;
        return sqrtf(fx * fx + fy * fy);
    }
}

void RestMeasure_Start(RestMeasure& m)
{
    memset(&m, 0, sizeof(m));
    m.phase = RP_PUSH;
}

bool RestMeasure_Update(RestMeasure& m, const XINPUT_GAMEPAD& g, float dt)
{
    if (m.step >= REST_STEPS)
        return true;
    float mag[2] = { Magnitude(g.sThumbLX, g.sThumbLY), Magnitude(g.sThumbRX, g.sThumbRY) };
    bool letGo = mag[0] < LET_GO && mag[1] < LET_GO;
    switch (m.phase)
    {
    case RP_PUSH:
    {
        const float* d = g_StepDir[m.step];
        float l = (g.sThumbLX * d[0] + g.sThumbLY * d[1]) / 32767.0f;
        float r = (g.sThumbRX * d[0] + g.sThumbRY * d[1]) / 32767.0f;
        if (l > PUSHED && r > PUSHED)
            m.phase = RP_RELEASE;
        break;
    }
    case RP_RELEASE:
        if (letGo)
        {
            m.phase = RP_SETTLE;
            m.timer = 0;
        }
        break;
    case RP_SETTLE:
    case RP_SAMPLE:
        // Touched again: wait for the next release. Only a full sample counts.
        if (!letGo)
        {
            m.phase = RP_RELEASE;
            break;
        }
        m.timer += dt;
        if (m.phase == RP_SETTLE)
        {
            if (m.timer >= SETTLE)
            {
                m.phase = RP_SAMPLE;
                m.timer = 0;
                m.stepMax[0] = m.stepMax[1] = 0;
            }
            break;
        }
        for (int k = 0; k < 2; k++)
            if (mag[k] > m.stepMax[k])
                m.stepMax[k] = mag[k];
        if (g.bLeftTrigger > m.trigMax[0])  m.trigMax[0] = g.bLeftTrigger;
        if (g.bRightTrigger > m.trigMax[1]) m.trigMax[1] = g.bRightTrigger;
        if (m.timer >= SAMPLE)
        {
            for (int k = 0; k < 2; k++)
            {
                m.dirMax[m.step][k] = m.stepMax[k];
                if (m.stepMax[k] > m.worst[k])
                    m.worst[k] = m.stepMax[k];
            }
            m.step++;
            m.phase = RP_PUSH;
        }
        break;
    }
    return m.step >= REST_STEPS;
}

void RestMeasure_Draw(const RestMeasure& m, const char* title)
{
    R::Rect(0, 0, SCREEN_W, SCREEN_H, COL_SHADE);
    R::RoundRect(317, 227, 646, 266, 16, COL_ACCENT);
    R::RoundRect(320, 230, 640, 260, 14, COL_PANEL);
    R::Text(640, 250, 3, COL_TEXT, ALIGN_CENTER, title);
    int step = m.step < REST_STEPS ? m.step : REST_STEPS - 1;
    R::Textf(640, 284, 1, COL_DIM, ALIGN_CENTER, "Step %d of %d", step + 1, REST_STEPS);
    if (m.phase == RP_PUSH)
    {
        R::Textf(640, 314, 2, COL_YELLOW, ALIGN_CENTER, "Push both sticks %s all the way", g_RestStepNames[step]);
        R::Text(640, 344, 2, COL_TEXT, ALIGN_CENTER, "then let go");
    }
    else if (m.phase == RP_RELEASE)
        R::Text(640, 314, 2, COL_YELLOW, ALIGN_CENTER, "Let go of both sticks");
    else
    {
        float t = (m.phase == RP_SAMPLE ? SETTLE : 0) + m.timer;
        t /= SETTLE + SAMPLE;
        R::Text(640, 314, 2, COL_TEXT, ALIGN_CENTER, "Measuring - hands off");
        R::Rect(400, 344, 480, 20, COL_WELL);
        R::Rect(400, 344, 480 * (t > 1 ? 1 : t), 20, COL_LIT);
        R::RectOutline(400, 344, 480, 20, 2, COL_PANEL_EDGE);
    }

    // Directions: done, current, still to do.
    for (int i = 0; i < REST_STEPS; i++)
    {
        DWORD c = i < m.step ? COL_LIT : (i == m.step ? COL_TEXT : COL_FAINT);
        R::Text(460 + i * 120.0f, 396, 2, c, ALIGN_CENTER, g_RestStepNames[i]);
    }
    R::Text(640, 448, 1, COL_DIM, ALIGN_CENTER, "B: cancel");
}
