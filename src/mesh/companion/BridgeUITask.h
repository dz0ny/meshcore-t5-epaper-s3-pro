#pragma once

#include "AbstractUITask.h"
#include "../mesh_bridge.h"

// UITask implementation that routes companion radio events to our bridge queues.
// This replaces the display-based UITask from companion_radio.
class BridgeUITask : public AbstractUITask {
public:
    BridgeUITask(mesh::MainBoard* board, BaseSerialInterface* serial)
        : AbstractUITask(board, serial) {}

    void msgRead(int msgcount) override {
        // UI acknowledged messages — nothing to do
    }

    void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override {
        mesh::bridge::MessageIn mi = {};
        if (from_name) strncpy(mi.sender_name, from_name, sizeof(mi.sender_name) - 1);
        if (text) strncpy(mi.text, text, sizeof(mi.text) - 1);
        mi.is_direct = (path_len != 0xFF && path_len > 0);
        mi.is_channel = false;
        mesh::bridge::push_message(mi);
    }

    void notify(UIEventType t) override {
        // Could trigger a screen refresh flag here if needed
    }

    void loop() override {
        // Nothing — our bridge is polled from UI task
    }
};
