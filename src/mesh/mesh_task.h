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

// Discovery: get recently heard nodes (not yet contacts)
struct DiscoveredNode {
    char name[32];
    uint8_t pubkey_prefix[7];
    uint8_t path_len;
    uint32_t recv_timestamp;
};
int get_discovered(DiscoveredNode* dest, int max_num);

// Add a discovered node as a contact (by pubkey prefix match)
bool add_contact_by_prefix(const uint8_t* pubkey_prefix);

// Set radio params (saves to prefs, requires reboot to apply)
void set_node_name(const char* name);
void set_freq(float freq_mhz);
void set_bw(float bw_khz);
void set_sf(uint8_t sf);
void set_cr(uint8_t cr);
void set_tx_power(int8_t dbm);

// GPS location sharing over mesh adverts
void set_gps_enabled(bool enabled);
bool get_gps_enabled();
void set_advert_location(bool share);
bool get_advert_location();

// Node advertised location (from prefs)
double get_node_lat();
double get_node_lon();

// Enter light sleep — waits for radio idle, then sleeps both cores.
// Wakes on LoRa packet (DIO1) or timer.
void enter_sleep(uint32_t wake_secs);

} // namespace mesh::task
