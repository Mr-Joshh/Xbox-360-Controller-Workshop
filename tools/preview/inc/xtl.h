// PC stand-in for the XDK's xtl.h, just enough for the layout preview.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#define XUSER_MAX_COUNT 4
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30
#define XINPUT_FLAG_GAMEPAD 1
#define XINPUT_CAPS_WIRELESS 2
#define XINPUT_DEVSUBTYPE_GAMEPAD            0x01
#define XINPUT_DEVSUBTYPE_WHEEL              0x02
#define XINPUT_DEVSUBTYPE_ARCADE_STICK       0x03
#define XINPUT_DEVSUBTYPE_FLIGHT_STICK       0x04
#define XINPUT_DEVSUBTYPE_DANCEPAD           0x05
#define XINPUT_DEVSUBTYPE_GUITAR             0x06
#define XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE   0x07
#define XINPUT_DEVSUBTYPE_DRUM_KIT           0x08
#define XINPUT_DEVSUBTYPE_GUITAR_BASS        0x0B
#define XINPUT_DEVSUBTYPE_ARCADE_PAD         0x13
#define XLAUNCH_KEYWORD_DEFAULT_APP ""

typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XINPUT_GAMEPAD;
typedef struct { DWORD dwPacketNumber; XINPUT_GAMEPAD Gamepad; } XINPUT_STATE;
typedef struct { WORD wLeftMotorSpeed, wRightMotorSpeed; } XINPUT_VIBRATION;
typedef struct { BYTE Type, SubType; WORD Flags; XINPUT_GAMEPAD Gamepad; XINPUT_VIBRATION Vibration; } XINPUT_CAPABILITIES;

inline DWORD XInputGetState(DWORD, XINPUT_STATE*) { return 1; }
inline DWORD XInputGetCapabilities(DWORD, DWORD, XINPUT_CAPABILITIES*) { return 1; }
inline DWORD XInputSetState(DWORD, XINPUT_VIBRATION*) { return 0; }
inline void XLaunchNewImage(const char*, DWORD) {}
