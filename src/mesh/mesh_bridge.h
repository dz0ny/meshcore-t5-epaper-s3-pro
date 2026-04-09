#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdint.h>
#include <string.h>

// Thread-safe bridge between MeshCore (core 0) and UI (core 1).
// MeshCore callbacks push data into queues; UI reads from them.

namespace mesh::bridge {

// ---------- Data types ----------

struct ContactUpdate {
    char name[32];
    uint8_t pub_key[32];
    uint8_t type;       // ADV_TYPE_*
    int32_t gps_lat;    // 6 decimal places * 1e6
    int32_t gps_lon;
    uint8_t path_len;   // 0xFF = unknown
    bool is_new;
};

struct MessageIn {
    char sender_name[32];
    char text[160];
    uint32_t timestamp;
    bool is_direct;     // direct vs flood routing
    bool is_channel;    // group channel message
};

struct MeshStatus {
    int peer_count;
    uint32_t last_rx_time;
    float last_rssi;
    float last_snr;
    bool radio_ok;
};

// ---------- Queue handles ----------

extern QueueHandle_t contact_queue;   // ContactUpdate items
extern QueueHandle_t message_queue;   // MessageIn items

// ---------- Status (protected by mutex) ----------

extern SemaphoreHandle_t status_mutex;
extern MeshStatus status;

// ---------- API ----------

void init();

// Called from mesh task (core 0)
void push_contact(const ContactUpdate& c);
void push_message(const MessageIn& m);
void update_status(const MeshStatus& s);

// Called from UI task (core 1)
bool pop_contact(ContactUpdate& c);
bool pop_message(MessageIn& m);
MeshStatus get_status();

} // namespace mesh::bridge
