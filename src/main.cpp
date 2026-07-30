// rog_kunai_xbox — ROG Kunai Gamepad 3 to Xbox 360 controller remapper
//
// Dependencies:
//   - ViGEmBus driver:  https://github.com/nefarius/ViGEmBus/releases (install the .msi)
//   - ViGEmClient SDK:  https://github.com/nefarius/ViGEmClient/releases
//     Extract to: deps/ViGEmClient/ so that these paths exist:
//       deps/ViGEmClient/include/ViGEm/Client.h
//       deps/ViGEmClient/lib/release/x64/ViGEmClient.lib
//
// Run modes:
//   rog_kunai_xbox.exe --list        List all HID gamepads (find your device)
//   rog_kunai_xbox.exe --dump N      Dump raw HID reports from device N (discover mapping)
//   rog_kunai_xbox.exe               Run with default ASUS VID auto-detect
//   rog_kunai_xbox.exe --device N    Run with device index N from --list

#include <Windows.h>
#include <SetupAPI.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <cfgmgr32.h>

#include <ViGEm/Client.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <csignal>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

// ─── HID device info ────────────────────────────────────────────────────────

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

static std::vector<HidDeviceInfo> EnumerateHidGamepads() {
    std::vector<HidDeviceInfo> devices;
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfoSet = SetupDiGetClassDevsW(
        &hidGuid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfoSet == INVALID_HANDLE_VALUE) return devices;

    SP_DEVICE_INTERFACE_DATA ifaceData = {};
    ifaceData.cbSize = sizeof(ifaceData);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devInfoSet, nullptr, &hidGuid, i, &ifaceData); ++i) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfoSet, &ifaceData, nullptr, 0, &requiredSize, nullptr);

        auto detailBuf = std::vector<BYTE>(requiredSize);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(devInfoSet, &ifaceData, detail, requiredSize, nullptr, nullptr))
            continue;

        HANDLE hDev = CreateFileW(
            detail->DevicePath, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDev == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attrs = {};
        attrs.Size = sizeof(attrs);
        if (!HidD_GetAttributes(hDev, &attrs)) {
            CloseHandle(hDev);
            continue;
        }

        PHIDP_PREPARSED_DATA preparsed = nullptr;
        HIDP_CAPS caps = {};
        if (HidD_GetPreparsedData(hDev, &preparsed)) {
            HidP_GetCaps(preparsed, &caps);
            HidD_FreePreparsedData(preparsed);
        }

        // Filter: Usage Page 0x01 (Generic Desktop), Usage 0x04 (Joystick) or 0x05 (Gamepad)
        if (caps.UsagePage == 0x01 && (caps.Usage == 0x04 || caps.Usage == 0x05)) {
            HidDeviceInfo info = {};
            info.path = detail->DevicePath;
            info.vendorId = attrs.VendorID;
            info.productId = attrs.ProductID;
            info.usagePage = caps.UsagePage;
            info.usage = caps.Usage;
            info.inputReportLength = caps.InputReportByteLength;

            wchar_t buf[256] = {};
            if (HidD_GetManufacturerString(hDev, buf, sizeof(buf)))
                info.manufacturer = buf;
            if (HidD_GetProductString(hDev, buf, sizeof(buf)))
                info.product = buf;

            devices.push_back(info);
        }
        CloseHandle(hDev);
    }
    SetupDiDestroyDeviceInfoList(devInfoSet);
    return devices;
}

// ─── ROG Kunai 3 HID report mapping ────────────────────────────────────────
//
// The ROG Kunai Gamepad 3 uses a standard HID gamepad report.
// Common ASUS gamepad VID: 0x0B05
// The report layout below matches the typical DirectInput HID report:
//
//   Byte 0:    Report ID
//   Byte 1:    Left stick X  (0x00 = left, 0x80 = center, 0xFF = right)
//   Byte 2:    Left stick Y  (0x00 = up,   0x80 = center, 0xFF = down)
//   Byte 3:    Right stick X
//   Byte 4:    Right stick Y
//   Byte 5:    D-pad + face buttons (lower nibble = hat, upper nibble = buttons)
//   Byte 6:    More buttons
//   Byte 7:    Left trigger  (0x00–0xFF)
//   Byte 8:    Right trigger (0x00–0xFF)
//
// If your device uses a different layout, run with --dump to see raw bytes
// and adjust the offsets in KunaiReportLayout below.

struct KunaiReportLayout {
    int leftStickXByte  = 1;
    int leftStickYByte  = 2;
    int rightStickXByte = 3;
    int rightStickYByte = 4;
    int dpadByte        = 5;   // lower nibble: 0=N,1=NE,2=E,...,7=NW, 8/0xF=neutral
    int buttonsByte1    = 5;   // upper nibble of byte 5
    int buttonsByte2    = 6;
    int leftTriggerByte = 7;
    int rightTriggerByte= 8;

    // Bit masks within buttonsByte1 (upper nibble of byte 5)
    BYTE maskA      = 0x10;
    BYTE maskB      = 0x20;
    BYTE maskX      = 0x40;
    BYTE maskY      = 0x80;

    // Bit masks within buttonsByte2
    BYTE maskLB     = 0x01;
    BYTE maskRB     = 0x02;
    BYTE maskBack   = 0x04;
    BYTE maskStart  = 0x08;
    BYTE maskLS     = 0x10;   // left stick press
    BYTE maskRS     = 0x20;   // right stick press
    BYTE maskHome   = 0x40;
};

// ─── Mapping HID report → XUSB_REPORT ──────────────────────────────────────

static SHORT ScaleAxisToShort(BYTE raw) {
    // HID: 0x00..0xFF center at 0x80 → Xbox: -32768..32767
    int centered = static_cast<int>(raw) - 128;
    int scaled = centered * 32767 / 128;
    return static_cast<SHORT>(std::clamp(scaled, -32768, 32767));
}

static SHORT ScaleAxisToShortInvertY(BYTE raw) {
    // Same but inverted (HID Y-axis is typically inverted vs Xbox)
    int centered = static_cast<int>(raw) - 128;
    int scaled = -(centered * 32767 / 128);
    return static_cast<SHORT>(std::clamp(scaled, -32768, 32767));
}

static XUSB_REPORT MapKunaiToXbox(const BYTE* report, ULONG reportLen, const KunaiReportLayout& layout) {
    XUSB_REPORT xr = {};

    if (reportLen < 9) return xr;

    // Thumbsticks
    xr.sThumbLX = ScaleAxisToShort(report[layout.leftStickXByte]);
    xr.sThumbLY = ScaleAxisToShortInvertY(report[layout.leftStickYByte]);
    xr.sThumbRX = ScaleAxisToShort(report[layout.rightStickXByte]);
    xr.sThumbRY = ScaleAxisToShortInvertY(report[layout.rightStickYByte]);

    // Triggers
    xr.bLeftTrigger  = report[layout.leftTriggerByte];
    xr.bRightTrigger = report[layout.rightTriggerByte];

    // D-pad (hat switch in lower nibble)
    BYTE hat = report[layout.dpadByte] & 0x0F;
    switch (hat) {
        case 0: xr.wButtons |= XUSB_GAMEPAD_DPAD_UP; break;
        case 1: xr.wButtons |= XUSB_GAMEPAD_DPAD_UP | XUSB_GAMEPAD_DPAD_RIGHT; break;
        case 2: xr.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT; break;
        case 3: xr.wButtons |= XUSB_GAMEPAD_DPAD_DOWN | XUSB_GAMEPAD_DPAD_RIGHT; break;
        case 4: xr.wButtons |= XUSB_GAMEPAD_DPAD_DOWN; break;
        case 5: xr.wButtons |= XUSB_GAMEPAD_DPAD_DOWN | XUSB_GAMEPAD_DPAD_LEFT; break;
        case 6: xr.wButtons |= XUSB_GAMEPAD_DPAD_LEFT; break;
        case 7: xr.wButtons |= XUSB_GAMEPAD_DPAD_UP | XUSB_GAMEPAD_DPAD_LEFT; break;
        default: break; // 8 or 0xF = neutral
    }

    // Face buttons (upper nibble of buttons byte 1)
    BYTE b1 = report[layout.buttonsByte1];
    if (b1 & layout.maskA) xr.wButtons |= XUSB_GAMEPAD_A;
    if (b1 & layout.maskB) xr.wButtons |= XUSB_GAMEPAD_B;
    if (b1 & layout.maskX) xr.wButtons |= XUSB_GAMEPAD_X;
    if (b1 & layout.maskY) xr.wButtons |= XUSB_GAMEPAD_Y;

    // Shoulder + meta buttons (byte 2)
    BYTE b2 = report[layout.buttonsByte2];
    if (b2 & layout.maskLB)    xr.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
    if (b2 & layout.maskRB)    xr.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
    if (b2 & layout.maskBack)  xr.wButtons |= XUSB_GAMEPAD_BACK;
    if (b2 & layout.maskStart) xr.wButtons |= XUSB_GAMEPAD_START;
    if (b2 & layout.maskLS)    xr.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;
    if (b2 & layout.maskRS)    xr.wButtons |= XUSB_GAMEPAD_RIGHT_THUMB;
    if (b2 & layout.maskHome)  xr.wButtons |= XUSB_GAMEPAD_GUIDE;

    return xr;
}

// ─── Globals for clean shutdown ─────────────────────────────────────────────

static volatile bool g_running = true;

static void SignalHandler(int) {
    g_running = false;
}

// ─── Commands ───────────────────────────────────────────────────────────────

static void CmdListDevices() {
    auto devices = EnumerateHidGamepads();
    if (devices.empty()) {
        printf("No HID gamepads found.\n");
        printf("Make sure your ROG Kunai Gamepad 3 is connected.\n");
        return;
    }
    printf("Found %zu HID gamepad(s):\n\n", devices.size());
    for (size_t i = 0; i < devices.size(); ++i) {
        auto& d = devices[i];
        printf("  [%zu] VID:%04X PID:%04X  Usage:%02X/%02X  ReportLen:%lu\n",
               i, d.vendorId, d.productId, d.usagePage, d.usage, d.inputReportLength);
        printf("      Manufacturer: %ls\n", d.manufacturer.empty() ? L"(unknown)" : d.manufacturer.c_str());
        printf("      Product:      %ls\n", d.product.empty() ? L"(unknown)" : d.product.c_str());
        if (d.vendorId == 0x0B05) printf("      ** ASUS device detected **\n");
        printf("\n");
    }
    printf("Use --device <index> to select a specific device.\n");
    printf("Use --dump <index> to see raw HID reports for mapping.\n");
}

static void CmdDumpReports(int deviceIndex) {
    auto devices = EnumerateHidGamepads();
    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(devices.size())) {
        printf("Invalid device index %d. Use --list to see devices.\n", deviceIndex);
        return;
    }
    auto& dev = devices[deviceIndex];
    printf("Dumping raw HID reports from: %ls (VID:%04X PID:%04X)\n",
           dev.product.c_str(), dev.vendorId, dev.productId);
    printf("Report length: %lu bytes\n", dev.inputReportLength);
    printf("Press Ctrl+C to stop.\n\n");

    HANDLE hDev = CreateFileW(
        dev.path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Failed to open device (error %lu). Try running as administrator.\n", GetLastError());
        return;
    }

    auto buf = std::vector<BYTE>(dev.inputReportLength);
    DWORD bytesRead = 0;

    signal(SIGINT, SignalHandler);
    while (g_running) {
        if (ReadFile(hDev, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, nullptr)) {
            printf("  [%3lu] ", bytesRead);
            for (DWORD j = 0; j < bytesRead && j < 16; ++j)
                printf("%02X ", buf[j]);
            printf("\r");
        }
    }
    printf("\n\nDone.\n");
    CloseHandle(hDev);
}

static int CmdRun(int deviceIndex) {
    auto devices = EnumerateHidGamepads();

    // Auto-detect ASUS device if no index given
    if (deviceIndex < 0) {
        for (size_t i = 0; i < devices.size(); ++i) {
            if (devices[i].vendorId == 0x0B05) {
                deviceIndex = static_cast<int>(i);
                printf("Auto-detected ASUS device at index %d: %ls\n",
                       deviceIndex, devices[i].product.c_str());
                break;
            }
        }
        if (deviceIndex < 0) {
            printf("No ASUS gamepad found. Use --list to see connected devices,\n");
            printf("then --device <index> to select one manually.\n");
            return 1;
        }
    }
    if (deviceIndex >= static_cast<int>(devices.size())) {
        printf("Invalid device index %d.\n", deviceIndex);
        return 1;
    }

    auto& dev = devices[deviceIndex];
    printf("Using: %ls (VID:%04X PID:%04X, report length: %lu)\n",
           dev.product.c_str(), dev.vendorId, dev.productId, dev.inputReportLength);

    // ── Open HID device ──
    HANDLE hDev = CreateFileW(
        dev.path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Failed to open HID device (error %lu). Try running as administrator.\n", GetLastError());
        return 1;
    }

    // ── Initialize ViGEmBus ──
    printf("Connecting to ViGEmBus...\n");
    PVIGEM_CLIENT vigemClient = vigem_alloc();
    if (!vigemClient) {
        printf("Failed to allocate ViGEm client.\n");
        CloseHandle(hDev);
        return 1;
    }

    VIGEM_ERROR vErr = vigem_connect(vigemClient);
    if (!VIGEM_SUCCESS(vErr)) {
        printf("Failed to connect to ViGEmBus (error 0x%X).\n", vErr);
        printf("Make sure ViGEmBus driver is installed:\n");
        printf("  https://github.com/nefarius/ViGEmBus/releases\n");
        vigem_free(vigemClient);
        CloseHandle(hDev);
        return 1;
    }

    // ── Create virtual Xbox 360 controller ──
    PVIGEM_TARGET xbox = vigem_target_x360_alloc();
    if (!xbox) {
        printf("Failed to allocate Xbox 360 target.\n");
        vigem_disconnect(vigemClient);
        vigem_free(vigemClient);
        CloseHandle(hDev);
        return 1;
    }

    vErr = vigem_target_add(vigemClient, xbox);
    if (!VIGEM_SUCCESS(vErr)) {
        printf("Failed to add virtual Xbox controller (error 0x%X).\n", vErr);
        vigem_target_free(xbox);
        vigem_disconnect(vigemClient);
        vigem_free(vigemClient);
        CloseHandle(hDev);
        return 1;
    }

    printf("Virtual Xbox 360 controller created!\n");
    printf("Your ROG Kunai Gamepad 3 is now emulating an Xbox controller.\n");
    printf("Press Ctrl+C to stop.\n\n");

    // ── Main loop ──
    KunaiReportLayout layout;
    auto buf = std::vector<BYTE>(dev.inputReportLength);
    DWORD bytesRead = 0;
    XUSB_REPORT lastReport = {};
    ULONG pollCount = 0;

    signal(SIGINT, SignalHandler);
    while (g_running) {
        if (!ReadFile(hDev, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_DEVICE_NOT_CONNECTED) {
                printf("\nGamepad disconnected.\n");
                break;
            }
            continue;
        }

        XUSB_REPORT xr = MapKunaiToXbox(buf.data(), bytesRead, layout);

        // Only update if state changed (reduces bus traffic)
        if (memcmp(&xr, &lastReport, sizeof(xr)) != 0) {
            vigem_target_x360_update(vigemClient, xbox, xr);
            lastReport = xr;
        }

        // Periodic status (every ~500 reads)
        if (++pollCount % 500 == 0) {
            printf("  LX:%6d LY:%6d RX:%6d RY:%6d LT:%3d RT:%3d Btn:0x%04X\r",
                   xr.sThumbLX, xr.sThumbLY, xr.sThumbRX, xr.sThumbRY,
                   xr.bLeftTrigger, xr.bRightTrigger, xr.wButtons);
        }
    }

    // ── Cleanup ──
    printf("\nShutting down...\n");
    vigem_target_remove(vigemClient, xbox);
    vigem_target_free(xbox);
    vigem_disconnect(vigemClient);
    vigem_free(vigemClient);
    CloseHandle(hDev);
    printf("Done.\n");
    return 0;
}

// ─── Entry point ────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    printf("=== ROG Kunai Gamepad 3 → Xbox 360 Controller Remapper ===\n\n");

    if (argc >= 2 && strcmp(argv[1], "--list") == 0) {
        CmdListDevices();
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "--dump") == 0) {
        CmdDumpReports(atoi(argv[2]));
        return 0;
    }

    int deviceIndex = -1; // -1 = auto-detect ASUS
    if (argc >= 3 && strcmp(argv[1], "--device") == 0) {
        deviceIndex = atoi(argv[2]);
    }

    return CmdRun(deviceIndex);
}
