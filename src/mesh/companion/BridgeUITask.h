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
        mi.is_direct = (path_len == 0xFF);
        mi.is_channel = false;  // both direct and channel come through here
        mi.timestamp = 0;
        mesh::bridge::push_message(mi);

        Serial.printf("BRIDGE: msg from '%s': %s\n", from_name ? from_name : "?", text ? text : "?");
    }

    void telemetryResponse(const uint8_t* pub_key_prefix, const uint8_t* data, uint8_t len) override {
        if (!pub_key_prefix || !data || len == 0) return;

        mesh::bridge::TelemetryResponse tr = {};
        memcpy(tr.pub_key_prefix, pub_key_prefix, sizeof(tr.pub_key_prefix));
        tr.len = len > sizeof(tr.data) ? sizeof(tr.data) : len;
        memcpy(tr.data, data, tr.len);
        mesh::bridge::push_telemetry(tr);
    }

    void traceResponse(uint32_t tag, uint8_t hop_count, int8_t snr_there_q4, int8_t snr_back_q4) override {
        mesh::bridge::TraceResponse tr = {};
        tr.tag = tag;
        tr.hop_count = hop_count;
        tr.snr_there_q4 = snr_there_q4;
        tr.snr_back_q4 = snr_back_q4;
        mesh::bridge::push_trace(tr);
    }

    void notify(UIEventType t) override {
        if (t == UIEventType::newContactMessage) {
            mesh::bridge::mark_discovery_changed();
        }
    }

    void loop() override {
        // Nothing — our bridge is polled from UI task
    }
};
