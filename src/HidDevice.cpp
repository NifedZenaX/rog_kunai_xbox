#include "HidDevice.h"

std::vector<HidDeviceInfo> EnumerateHidGamepads() {
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

        bool isGamepad = caps.UsagePage == 0x01
            && (caps.Usage == 0x04 || caps.Usage == 0x05);

        if (isGamepad) {
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

HANDLE OpenHidDevice(const std::wstring& path) {
    return CreateFileW(
        path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
}
