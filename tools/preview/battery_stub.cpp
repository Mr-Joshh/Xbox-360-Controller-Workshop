// Preview stand-in for src/battery.cpp.
#include "../../src/battery.h"

bool GetBatteryLevel(int port, int* level)
{
    *level = port == 0 ? BATTERY_MEDIUM : BATTERY_LOW;
    return true;
}
