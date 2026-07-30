#pragma once

#include <Windows.h>
#include <ViGEm/Client.h>

#include <cstdio>

class XboxController {
public:
    ~XboxController();

    bool Connect();
    void Disconnect();
    bool IsConnected() const { return m_target != nullptr; }

    void Update(const XUSB_REPORT& report);

private:
    PVIGEM_CLIENT m_client = nullptr;
    PVIGEM_TARGET m_target = nullptr;
};
