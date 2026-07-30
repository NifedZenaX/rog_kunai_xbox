#pragma once

#include <Windows.h>
#include <ViGEm/Client.h>

#include <algorithm>

// ROG Kunai Gamepad 3 HID report byte layout.
//
// Default offsets match the typical ASUS DirectInput HID report:
//   Byte 0:  Report ID
//   Byte 1:  Left stick X   (0x00 = left,  0x80 = center, 0xFF = right)
//   Byte 2:  Left stick Y   (0x00 = up,    0x80 = center, 0xFF = down)
//   Byte 3:  Right stick X
//   Byte 4:  Right stick Y
//   Byte 5:  D-pad (lower nibble) + face buttons (upper nibble)
//   Byte 6:  Shoulder / meta buttons
//   Byte 7:  Left trigger   (0x00–0xFF)
//   Byte 8:  Right trigger  (0x00–0xFF)
//
// Run with --dump to see raw bytes and adjust these if needed.

struct KunaiReportLayout {
    int leftStickXByte   = 1;
    int leftStickYByte   = 2;
    int rightStickXByte  = 3;
    int rightStickYByte  = 4;
    int dpadByte         = 5;
    int buttonsByte1     = 5;
    int buttonsByte2     = 6;
    int leftTriggerByte  = 7;
    int rightTriggerByte = 8;

    BYTE maskA     = 0x10;
    BYTE maskB     = 0x20;
    BYTE maskX     = 0x40;
    BYTE maskY     = 0x80;

    BYTE maskLB    = 0x01;
    BYTE maskRB    = 0x02;
    BYTE maskBack  = 0x04;
    BYTE maskStart = 0x08;
    BYTE maskLS    = 0x10;
    BYTE maskRS    = 0x20;
    BYTE maskHome  = 0x40;
};

XUSB_REPORT MapKunaiToXbox(const BYTE* report, ULONG reportLen,
                           const KunaiReportLayout& layout);
