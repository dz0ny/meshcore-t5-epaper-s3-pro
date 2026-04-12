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

} // namespace sd_log
