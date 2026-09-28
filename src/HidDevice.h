#pragma once

#include <Windows.h>
#include <SetupAPI.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

constexpr USHORT ASUS_VENDOR_ID = 0x0B05;

struct HidDeviceInfo {
    std::wstring path;
    std::wstring manufacturer;
    std::wstring product;
    USHORT vendorId;
    USHORT productId;
    USHORT usagePage;
    USHORT usage;
    ULONG  inputReportLength;
};

std::vector<HidDeviceInfo> EnumerateHidGamepads();

// Every HID device, no usage-page filtering. Diagnostic for when a pad
// doesn't report itself as Joystick/Gamepad.
std::vector<HidDeviceInfo> EnumerateAllHidDevices();

HANDLE OpenHidDevice(const std::wstring& path);
