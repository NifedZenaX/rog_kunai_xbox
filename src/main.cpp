// rog_kunai_xbox — ROG Kunai Gamepad 3 to Xbox 360 controller remapper
//
// Dependencies:
//   - ViGEmBus driver:  https://github.com/nefarius/ViGEmBus/releases
//   - ViGEmClient SDK:  https://github.com/nefarius/ViGEmClient/releases
//     Extract to deps/ViGEmClient/ (see README.md)

#include "HidDevice.h"
#include "ButtonMapping.h"
#include "XboxController.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <vector>

static volatile bool g_running = true;

static void SignalHandler(int) { g_running = false; }

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
        printf("      Manufacturer: %ls\n",
               d.manufacturer.empty() ? L"(unknown)" : d.manufacturer.c_str());
        printf("      Product:      %ls\n",
               d.product.empty() ? L"(unknown)" : d.product.c_str());
        if (d.vendorId == ASUS_VENDOR_ID)
            printf("      ** ASUS device detected **\n");
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

    HANDLE hDev = OpenHidDevice(dev.path);
    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Failed to open device (error %lu). Try running as administrator.\n",
               GetLastError());
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

    if (deviceIndex < 0) {
        for (size_t i = 0; i < devices.size(); ++i) {
            if (devices[i].vendorId == ASUS_VENDOR_ID) {
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

    HANDLE hDev = OpenHidDevice(dev.path);
    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Failed to open HID device (error %lu). Try running as administrator.\n",
               GetLastError());
        return 1;
    }

    printf("Connecting to ViGEmBus...\n");
    XboxController xbox;
    if (!xbox.Connect()) {
        CloseHandle(hDev);
        return 1;
    }

    printf("Virtual Xbox 360 controller created!\n");
    printf("Your ROG Kunai Gamepad 3 is now emulating an Xbox controller.\n");
    printf("Press Ctrl+C to stop.\n\n");

    KunaiReportLayout layout;
    auto buf = std::vector<BYTE>(dev.inputReportLength);
    DWORD bytesRead = 0;
    XUSB_REPORT lastReport = {};
    ULONG pollCount = 0;

    signal(SIGINT, SignalHandler);
    while (g_running) {
        if (!ReadFile(hDev, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, nullptr)) {
            if (GetLastError() == ERROR_DEVICE_NOT_CONNECTED) {
                printf("\nGamepad disconnected.\n");
                break;
            }
            continue;
        }

        XUSB_REPORT xr = MapKunaiToXbox(buf.data(), bytesRead, layout);

        if (memcmp(&xr, &lastReport, sizeof(xr)) != 0) {
            xbox.Update(xr);
            lastReport = xr;
        }

        if (++pollCount % 500 == 0) {
            printf("  LX:%6d LY:%6d RX:%6d RY:%6d LT:%3d RT:%3d Btn:0x%04X\r",
                   xr.sThumbLX, xr.sThumbLY, xr.sThumbRX, xr.sThumbRY,
                   xr.bLeftTrigger, xr.bRightTrigger, xr.wButtons);
        }
    }

    printf("\nShutting down...\n");
    xbox.Disconnect();
    CloseHandle(hDev);
    printf("Done.\n");
    return 0;
}

// ─── Entry point ────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    printf("=== ROG Kunai Gamepad 3 -> Xbox 360 Controller Remapper ===\n\n");

    if (argc >= 2 && strcmp(argv[1], "--list") == 0) {
        CmdListDevices();
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "--dump") == 0) {
        CmdDumpReports(atoi(argv[2]));
        return 0;
    }

    int deviceIndex = -1;
    if (argc >= 3 && strcmp(argv[1], "--device") == 0)
        deviceIndex = atoi(argv[2]);

    return CmdRun(deviceIndex);
}
