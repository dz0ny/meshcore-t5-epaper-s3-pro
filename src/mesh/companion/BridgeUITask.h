#pragma once

#include "AbstractUITask.h"
#include "../mesh_bridge.h"
#include "../../model.h"
#include "../../sd_log.h"

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
        mi.is_direct = (path_len == 0xFF);
        mi.is_channel = false;  // both direct and channel come through here
        mi.timestamp = 0;
        mesh::bridge::push_message(mi);

        // Store in model for persistent display
        if (model::message_count < MAX_STORED_MESSAGES) {
            auto& msg = model::messages[model::message_count];
            if (from_name) strncpy(msg.sender, from_name, sizeof(msg.sender) - 1);
            if (text) strncpy(msg.text, text, sizeof(msg.text) - 1);
            msg.hour = model::clock.hour;
            msg.minute = model::clock.minute;
            msg.is_self = false;
            model::message_count++;
        }

        // Mark for SD flush on next silence window
        sd_log::mark_dirty();

        // Track for lock screen
        model::note_incoming_message(from_name, text);

        Serial.printf("BRIDGE: msg from '%s': %s\n", from_name ? from_name : "?", text ? text : "?");
    }

    void notify(UIEventType t) override {
        if (t == UIEventType::newContactMessage) {
            mesh::bridge::discovery_changed = true;
        }
    }

    void loop() override {
        // Nothing — our bridge is polled from UI task
    }
};
