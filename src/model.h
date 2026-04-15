#pragma once

#include <stdint.h>
#include <stdbool.h>

// Reactive data model — single source of truth for all UI screens.
// Updated by background tasks (mesh, gps, board), read by UI.
// No mutexes needed: writers are atomic-friendly POD types,
// and UI only reads (worst case: one stale frame).

namespace model {

enum DirtyFlags : uint32_t {
    DIRTY_NONE = 0,
    DIRTY_CLOCK = 1 << 0,
    DIRTY_BATTERY = 1 << 1,
    DIRTY_GPS = 1 << 2,
    DIRTY_MESH = 1 << 3,
    DIRTY_MESSAGES = 1 << 4,
    DIRTY_SLEEP = 1 << 5,
};

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
    bool     ble_enabled;
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

static constexpr int MAX_CONTACT_ENTRIES = 100;
static constexpr int MAX_DISCOVERY_ENTRIES = 16;
static constexpr int MAX_TELEMETRY_ENTRIES = 32;
static constexpr int MAX_TRACE_ENTRIES = 16;

struct ContactEntry {
    char name[32];
    uint8_t pub_key[32];
    uint8_t type;
    uint8_t flags;
    bool has_path;
    int32_t gps_lat;
    int32_t gps_lon;
};

struct DiscoveryEntry {
    char name[32];
    uint8_t pubkey_prefix[7];
    uint8_t path_len;
    uint32_t recv_timestamp;
};

struct TelemetryEntry {
    bool valid;
    uint8_t pub_key_prefix[7];
    uint8_t data[96];
    uint8_t len;
    uint32_t timestamp;
    uint32_t seq;
};

struct TraceEntry {
    bool valid;
    uint32_t tag;
    uint8_t hop_count;
    int8_t snr_there_q4;
    int8_t snr_back_q4;
    uint32_t timestamp;
    uint32_t seq;
};

// Global state — written by updaters, read by UI
extern GPS     gps;
extern Battery battery;
extern Mesh    mesh;
extern Clock   clock;
extern ContactEntry contacts[MAX_CONTACT_ENTRIES];
extern int contact_count;
extern uint32_t contacts_revision;
extern DiscoveryEntry discovery[MAX_DISCOVERY_ENTRIES];
extern int discovery_count;
extern uint32_t discovery_revision;
extern TelemetryEntry telemetry[MAX_TELEMETRY_ENTRIES];
extern uint32_t telemetry_revision;
extern TraceEntry traces[MAX_TRACE_ENTRIES];
extern uint32_t trace_revision;

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
void note_incoming_message(const char* from_name, const char* text);
void clear_unread_messages();
void mark_dirty(uint32_t flags);
uint32_t take_dirty();
void ingest_bridge_events();
void refresh_contacts();
void refresh_discovery();
const ContactEntry* find_contact_by_prefix(const uint8_t* prefix, int prefix_len = 7);
const ContactEntry* find_contact_by_name(const char* name);
const TelemetryEntry* find_telemetry(const uint8_t* prefix, int prefix_len = 7);
const TraceEntry* find_trace(uint32_t tag);

// Call from background tasks to refresh the model
void update_gps();
void update_battery();
void update_mesh();
void update_clock();

} // namespace model
