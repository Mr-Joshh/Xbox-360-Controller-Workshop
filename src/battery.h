// battery.h - wireless controller battery level, read from xam (not in the public XDK headers).
#pragma once

// Battery level steps, the same ones the Guide's battery icon shows. Xam does not report a percentage.
enum { BATTERY_EMPTY = 0, BATTERY_LOW = 1, BATTERY_MEDIUM = 2, BATTERY_FULL = 3 };

// Returns true and the level (0..3) for a wireless controller on 'port'.
bool GetBatteryLevel(int port, int* level);
