#pragma once

#include <stdint.h>

namespace mesh::task {

// Start the mesh networking task on the specified core.
// Must be called after board::init() since it uses the SPI bus and LoRa radio.
void start(int core);

// Send a text message to the currently selected contact.
// Thread-safe — can be called from UI core.
bool send_message(const char* recipient_prefix, const char* text);

// Send a broadcast to the public channel.
bool send_public(const char* text);

// Get the node name.
const char* node_name();

// Radio parameters (read current values)
float get_freq();
float get_bw();
uint8_t get_sf();
uint8_t get_cr();
int8_t get_tx_power();
uint32_t get_packets_recv();
uint32_t get_packets_sent();

// Push all known contacts to the bridge queue (for UI to display).
void push_all_contacts();

} // namespace mesh::task
