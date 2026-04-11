#pragma once

#include <stdint.h>
#include <stdbool.h>

// Reactive data model — single source of truth for all UI screens.
// Updated by background tasks (mesh, gps, board), read by UI.
// No mutexes needed: writers are atomic-friendly POD types,
// and UI only reads (worst case: one stale frame).

namespace model {

struct GPS {
    double lat;
    double lng;
    uint32_t satellites;
    double altitude_m;
    bool has_fix;
    bool module_ok;
    // "No Module", "Searching...", "Fix OK"
    const char* status_text;
};

struct Battery {
    uint16_t percent;       // 0-100
    uint16_t voltage_mv;
    int16_t  current_ma;
    float    temperature_c;
    uint16_t remain_mah;
    uint16_t full_mah;
    uint16_t design_mah;
    uint16_t health_pct;
    bool     charging;
    // Charger (BQ25896)
    bool     charger_ok;
    const char* charge_status;
    const char* bus_status;
    const char* ntc_status;
    float    vbus_v;
    float    vsys_v;
    float    vbat_v;
    float    charge_current_ma;
};

struct Mesh {
    int      peer_count;
    uint32_t rx_packets;
    uint32_t tx_packets;
    float    last_rssi;
    float    last_snr;
    bool     radio_ok;
    // Radio config (read-only from prefs)
    const char* node_name;
    float    freq_mhz;
    float    bw_khz;
    uint8_t  sf;
    uint8_t  cr;
    int8_t   tx_power_dbm;
};

struct Clock {
    uint8_t hour, minute, second;   // local time (UTC + tz_offset)
    uint8_t year, month, day;
    int8_t  tz_offset_hours;        // auto-calculated from GPS longitude
};

// Global state — written by updaters, read by UI
extern GPS     gps;
extern Battery battery;
extern Mesh    mesh;
extern Clock   clock;

// Sleep config
struct Sleep {
    uint8_t timeout_idx;       // index into timeout presets
    uint32_t timeout_ms;       // 0 = disabled
    uint32_t last_activity_ms; // last touch/interaction timestamp
    int unread_messages;
    char last_message[80];
    char last_sender[32];
};

extern Sleep sleep_cfg;

// Message history (persists across screen switches)
struct StoredMessage {
    char sender[32];
    char text[160];
    uint8_t hour, minute;
    bool is_self;
};

#define MAX_STORED_MESSAGES 50
extern StoredMessage* messages;
extern int message_count;

void init_messages();  // call once at startup to allocate PSRAM
void touch_activity();  // call on any user interaction
bool should_sleep();    // check if timeout expired
void delete_message(int idx);  // remove message at index, shift remaining

// Call from background tasks to refresh the model
void update_gps();
void update_battery();
void update_mesh();
void update_clock();

} // namespace model
