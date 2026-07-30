#include "ButtonMapping.h"

static SHORT ScaleAxis(BYTE raw) {
    int centered = static_cast<int>(raw) - 128;
    int scaled = centered * 32767 / 128;
    return static_cast<SHORT>(std::clamp(scaled, -32768, 32767));
}

static SHORT ScaleAxisInvertY(BYTE raw) {
    int centered = static_cast<int>(raw) - 128;
    int scaled = -(centered * 32767 / 128);
    return static_cast<SHORT>(std::clamp(scaled, -32768, 32767));
}

static USHORT MapDpad(BYTE hatValue) {
    switch (hatValue) {
        case 0: return XUSB_GAMEPAD_DPAD_UP;
        case 1: return XUSB_GAMEPAD_DPAD_UP    | XUSB_GAMEPAD_DPAD_RIGHT;
        case 2: return XUSB_GAMEPAD_DPAD_RIGHT;
        case 3: return XUSB_GAMEPAD_DPAD_DOWN  | XUSB_GAMEPAD_DPAD_RIGHT;
        case 4: return XUSB_GAMEPAD_DPAD_DOWN;
        case 5: return XUSB_GAMEPAD_DPAD_DOWN  | XUSB_GAMEPAD_DPAD_LEFT;
        case 6: return XUSB_GAMEPAD_DPAD_LEFT;
        case 7: return XUSB_GAMEPAD_DPAD_UP    | XUSB_GAMEPAD_DPAD_LEFT;
        default: return 0;
    }
}

static USHORT MapFaceButtons(BYTE raw, const KunaiReportLayout& layout) {
    USHORT buttons = 0;
    if (raw & layout.maskA) buttons |= XUSB_GAMEPAD_A;
    if (raw & layout.maskB) buttons |= XUSB_GAMEPAD_B;
    if (raw & layout.maskX) buttons |= XUSB_GAMEPAD_X;
    if (raw & layout.maskY) buttons |= XUSB_GAMEPAD_Y;
    return buttons;
}

static USHORT MapMetaButtons(BYTE raw, const KunaiReportLayout& layout) {
    USHORT buttons = 0;
    if (raw & layout.maskLB)    buttons |= XUSB_GAMEPAD_LEFT_SHOULDER;
    if (raw & layout.maskRB)    buttons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
    if (raw & layout.maskBack)  buttons |= XUSB_GAMEPAD_BACK;
    if (raw & layout.maskStart) buttons |= XUSB_GAMEPAD_START;
    if (raw & layout.maskLS)    buttons |= XUSB_GAMEPAD_LEFT_THUMB;
    if (raw & layout.maskRS)    buttons |= XUSB_GAMEPAD_RIGHT_THUMB;
    if (raw & layout.maskHome)  buttons |= XUSB_GAMEPAD_GUIDE;
    return buttons;
}

XUSB_REPORT MapKunaiToXbox(const BYTE* report, ULONG reportLen,
                           const KunaiReportLayout& layout) {
    XUSB_REPORT xr = {};
    if (reportLen < 9) return xr;

    xr.sThumbLX = ScaleAxis(report[layout.leftStickXByte]);
    xr.sThumbLY = ScaleAxisInvertY(report[layout.leftStickYByte]);
    xr.sThumbRX = ScaleAxis(report[layout.rightStickXByte]);
    xr.sThumbRY = ScaleAxisInvertY(report[layout.rightStickYByte]);

    xr.bLeftTrigger  = report[layout.leftTriggerByte];
    xr.bRightTrigger = report[layout.rightTriggerByte];

    xr.wButtons |= MapDpad(report[layout.dpadByte] & 0x0F);
    xr.wButtons |= MapFaceButtons(report[layout.buttonsByte1], layout);
    xr.wButtons |= MapMetaButtons(report[layout.buttonsByte2], layout);

    return xr;
}
