#pragma once

#include <helpers/BaseSerialInterface.h>

// Stub serial interface — t-paper doesn't use companion serial protocol.
// Messages are routed through the bridge queues to the LVGL UI instead.
class NullSerialInterface : public BaseSerialInterface {
public:
    void enable() override {}
    void disable() override {}
    bool isEnabled() const override { return false; }
    bool isConnected() const override { return false; }
    bool isWriteBusy() const override { return false; }
    size_t writeFrame(const uint8_t src[], size_t len) override { return len; }
    size_t checkRecvFrame(uint8_t dest[]) override { return 0; }
};
