#include "XboxController.h"

XboxController::~XboxController() {
    Disconnect();
}

bool XboxController::Connect() {
    m_client = vigem_alloc();
    if (!m_client) {
        printf("Failed to allocate ViGEm client.\n");
        return false;
    }

    VIGEM_ERROR err = vigem_connect(m_client);
    if (!VIGEM_SUCCESS(err)) {
        printf("Failed to connect to ViGEmBus (error 0x%X).\n", err);
        printf("Make sure ViGEmBus driver is installed:\n");
        printf("  https://github.com/nefarius/ViGEmBus/releases\n");
        vigem_free(m_client);
        m_client = nullptr;
        return false;
    }

    m_target = vigem_target_x360_alloc();
    if (!m_target) {
        printf("Failed to allocate Xbox 360 target.\n");
        Disconnect();
        return false;
    }

    err = vigem_target_add(m_client, m_target);
    if (!VIGEM_SUCCESS(err)) {
        printf("Failed to add virtual Xbox controller (error 0x%X).\n", err);
        vigem_target_free(m_target);
        m_target = nullptr;
        Disconnect();
        return false;
    }

    return true;
}

void XboxController::Disconnect() {
    if (m_target) {
        vigem_target_remove(m_client, m_target);
        vigem_target_free(m_target);
        m_target = nullptr;
    }
    if (m_client) {
        vigem_disconnect(m_client);
        vigem_free(m_client);
        m_client = nullptr;
    }
}

void XboxController::Update(const XUSB_REPORT& report) {
    if (m_target) {
        vigem_target_x360_update(m_client, m_target, report);
    }
}
