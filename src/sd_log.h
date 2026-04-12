#pragma once

#include <stddef.h>
#include "model.h"

// SD card message persistence.
// Messages buffer in model::messages[] (RAM). When radio silence is detected,
// the mesh task calls flush() which grabs the SPI bus (LoRa CS high) and
// writes dirty messages to /messages.bin on the SD card.
// On boot, load() reads them back into model::messages[].
//
// All functions must be called with mesh_mutex held (SPI bus exclusive).

namespace sd_log {

struct PingHistoryEntry {
    bool success;
    bool timed_out;
    bool relay;
    bool used_flood;
    bool retried_flood;
    uint32_t duration_ms;
    uint8_t hop_count;
    int8_t snr_there_q4;
    int8_t snr_back_q4;
    uint32_t timestamp;
};

// Load messages from SD into model::messages[]. Call once at startup.
void load();

// Mark that new messages need flushing.
void mark_dirty();

// Flush dirty messages to SD if the card is available.
// Call with mesh_mutex held — will deselect LoRa CS, use SD, then restore.
void flush();

// Returns true if there are unflushed messages.
bool is_dirty();

// Store the latest telemetry response for a contact.
void store_telemetry(const uint8_t* pub_key_prefix, const uint8_t* data, uint8_t len);

// Load the latest telemetry response for a contact into out.
bool get_telemetry(const uint8_t* pub_key_prefix, uint8_t* out, uint8_t* len, size_t out_size,
                   uint32_t* timestamp = nullptr);

// Remove persisted telemetry snapshots.
void clear_telemetry();

// Store the latest ping history for a contact.
void store_ping_history(const uint8_t* pub_key_prefix, const PingHistoryEntry* entries, uint8_t count);

// Load the latest ping history for a contact into out.
bool get_ping_history(const uint8_t* pub_key_prefix, PingHistoryEntry* out, uint8_t* count, size_t max_entries);

// Remove persisted ping history snapshots.
void clear_ping_history();

} // namespace sd_log
