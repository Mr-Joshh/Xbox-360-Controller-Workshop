// battery.cpp - see battery.h
#include <xtl.h>
#include "battery.h"

extern "C"
{
    DWORD NTAPI XexGetModuleHandle(PCSTR ModuleName, PHANDLE Module);
    DWORD NTAPI XexGetProcedureAddress(HANDLE Module, DWORD Ordinal, PVOID OutAddress);
}

namespace
{
    // Layout from xkelib (_xamext.h, 0x1C bytes). Padded in case a dashboard version writes more.
    struct DeviceStats
    {
        DWORD BatteryLevel;     // 0..3 (XINPUT_BATTERY_LEVEL_*; 4 is the "max" marker)
        DWORD BatteryType;
        DWORD SignalStrength;
        DWORD DeviceType;
        DWORD DevicePort;
        DWORD DeviceVersion;
        DWORD Jumped;
        DWORD Reserved[9];
    };

    typedef DWORD (NTAPI *GetDeviceStatsFn)(DWORD userIndex, DeviceStats* stats);

    const DWORD ORD_XamInputGetDeviceStats = 0x197;

    GetDeviceStatsFn Resolve()
    {
        static bool tried = false;
        static GetDeviceStatsFn fn = NULL;
        if (!tried)
        {
            tried = true;
            HANDLE xam = NULL;
            DWORD address = 0;
            if (XexGetModuleHandle("xam.xex", &xam) == 0 && xam &&
                XexGetProcedureAddress(xam, ORD_XamInputGetDeviceStats, &address) == 0)
                fn = (GetDeviceStatsFn)address;
        }
        return fn;
    }
}

bool GetBatteryLevel(int port, int* level)
{
    GetDeviceStatsFn fn = Resolve();
    if (!fn)
        return false;
    DeviceStats stats;
    ZeroMemory(&stats, sizeof(stats));
    if (fn(port, &stats) != ERROR_SUCCESS)
        return false;
    *level = stats.BatteryLevel > BATTERY_FULL ? BATTERY_FULL : (int)stats.BatteryLevel;
    return true;
}
