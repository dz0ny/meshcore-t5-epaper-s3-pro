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
    double speed_kmh;
    double altitude_m;
    double hdop;
    double course_deg;
    uint32_t chars_processed;
    uint8_t hour, minute, second;
    bool has_fix;
    bool module_ok;
    // "No Module", "No Data", "Searching...", "Fix OK"
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
    uint8_t hour, minute, second;
    uint8_t year, month, day;
};

// Global state — written by updaters, read by UI
extern GPS     gps;
extern Battery battery;
extern Mesh    mesh;
extern Clock   clock;

// Call from background tasks to refresh the model
void update_gps();
void update_battery();
void update_mesh();
void update_clock();

} // namespace model
