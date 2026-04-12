#include <esp_heap_caps.h>
#include <time.h>
#include <sys/time.h>
#include "model.h"
#include "board.h"
#include "mesh/mesh_bridge.h"
#include "mesh/mesh_task.h"
#include "mesh/companion/target.h"
#include <helpers/sensors/LocationProvider.h>
#include "nvs_param.h"

namespace model {

GPS     gps = {};
Battery battery = {};
Mesh    mesh = {};
Clock   clock = {};

void update_gps() {
    // Read from MeshCore's EnvironmentSensorManager / MicroNMEALocationProvider
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc) {
        gps.module_ok = true;
        gps.has_fix = loc->isValid();
        if (gps.has_fix) {
            gps.lat = loc->getLatitude() / 1e6;
            gps.lng = loc->getLongitude() / 1e6;
            gps.altitude_m = loc->getAltitude() / 1000.0;
        }
        gps.satellites = loc->satellitesCount();
        gps.status_text = gps.has_fix ? "Fix OK" : "Searching...";
    } else {
        gps.module_ok = false;
        gps.status_text = "No Module";
    }

    // MicroNMEALocationProvider syncs ESP32 system clock via rtc_clock.setCurrentTime().
    // Also sync hardware RTC so it persists across reboots.
    static bool hw_rtc_synced = false;
    if (gps.has_fix && !hw_rtc_synced && board::peri_status[E_PERI_RTC]) {
        time_t now;
        time(&now);
        struct tm utc;
        gmtime_r(&now, &utc);
        board::rtc.setDateTime(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                               utc.tm_hour, utc.tm_min, utc.tm_sec);
        hw_rtc_synced = true;
        Serial.println("GPS: hardware RTC synced");
    }
}

void update_battery() {
    battery.percent = board::battery_percent();
    battery.voltage_mv = board::battery_voltage_mv();
    battery.current_ma = board::battery_current_ma();
    uint16_t temp_raw = board::battery_temperature();
    battery.temperature_c = (temp_raw > 0 && temp_raw < 10000) ? (temp_raw / 10.0f) - 273.15f : 0;
    battery.remain_mah = board::battery_remain_capacity();
    battery.full_mah = board::battery_full_capacity();
    battery.design_mah = board::battery_design_capacity();
    battery.health_pct = board::battery_health();
    battery.charging = board::battery_is_charging();

    battery.charger_ok = board::charger_is_valid();
    if (battery.charger_ok) {
        battery.charge_status = board::charger_status_str();
        battery.bus_status = board::charger_bus_status_str();
        battery.ntc_status = board::charger_ntc_status_str();
        battery.vbus_v = board::charger_vbus_v();
        battery.vsys_v = board::charger_vsys_v();
        battery.vbat_v = board::charger_vbat_v();
        battery.charge_current_ma = board::charger_current_ma();
    }
}

void update_mesh() {
    auto ms = mesh::bridge::get_status();
    mesh.peer_count = ms.peer_count;
    mesh.last_rssi = ms.last_rssi;
    mesh.last_snr = ms.last_snr;
    mesh.radio_ok = ms.radio_ok;
    mesh.ble_enabled = mesh::task::ble_is_enabled();
    mesh.rx_packets = mesh::task::get_packets_recv();
    mesh.tx_packets = mesh::task::get_packets_sent();
    mesh.node_name = mesh::task::node_name();
    mesh.freq_mhz = mesh::task::get_freq();
    mesh.bw_khz = mesh::task::get_bw();
    mesh.sf = mesh::task::get_sf();
    mesh.cr = mesh::task::get_cr();
    mesh.tx_power_dbm = mesh::task::get_tx_power();
}

void update_clock() {
    // Read UTC from ESP32 system clock (seeded from hardware RTC at boot, updated by GPS)
    time_t now;
    time(&now);
    struct tm utc;
    gmtime_r(&now, &utc);

    uint8_t utc_h = utc.tm_hour;
    uint8_t utc_m = utc.tm_min;
    uint8_t utc_s = utc.tm_sec;
    int full_year = utc.tm_year + 1900;
    clock.year  = (full_year >= 2000 && full_year <= 2099) ? full_year - 2000 : 0;

    static bool clock_debug_once = false;
    if (!clock_debug_once) {
        Serial.printf("update_clock: time_t=%ld year=%d UTC=%02d:%02d:%02d\n",
            (long)now, full_year, utc_h, utc_m, utc_s);
        clock_debug_once = true;
    }
    clock.month = utc.tm_mon + 1;
    clock.day   = utc.tm_mday;

    // Auto timezone from GPS coordinates, fallback to MeshCore advertised location.
    double lat = gps.lat;
    double lng = gps.lng;
    bool has_loc = gps.has_fix && (lat != 0.0 || lng != 0.0);

    if (!has_loc && mesh.node_name) {
        lat = mesh::task::get_node_lat();
        lng = mesh::task::get_node_lon();
        has_loc = (lat != 0.0 || lng != 0.0);
    }

    if (has_loc) {
        // Europe special cases
        if (lat > 35 && lat < 72 && lng > -12 && lng < 40) {
            if (lng < 0)            clock.tz_offset_hours = 0;  // UK, Portugal, Iceland
            else if (lng < 16)      clock.tz_offset_hours = 1;  // CET
            else if (lng < 30)      clock.tz_offset_hours = 2;  // EET
            else                    clock.tz_offset_hours = 3;  // Moscow
        }
        else if (lng < -52) {
            if (lng < -120)         clock.tz_offset_hours = -8; // PST
            else if (lng < -105)    clock.tz_offset_hours = -7; // MST
            else if (lng < -90)     clock.tz_offset_hours = -6; // CST
            else if (lng < -70)     clock.tz_offset_hours = -5; // EST
            else                    clock.tz_offset_hours = -4; // AST
        }
        else if (lng > 40) {
            if (lng < 60)           clock.tz_offset_hours = 4;
            else if (lng < 82)      clock.tz_offset_hours = 5;
            else if (lng < 98)      clock.tz_offset_hours = 6;
            else if (lng < 105)     clock.tz_offset_hours = 7;
            else if (lng < 120)     clock.tz_offset_hours = 8;
            else if (lng < 135)     clock.tz_offset_hours = 9;
            else if (lng < 150)     clock.tz_offset_hours = 10;
            else                    clock.tz_offset_hours = 12;
        }
        else {
            clock.tz_offset_hours = (int8_t)(lng / 15.0);
        }
    }

    // DST detection
    int8_t dst = 0;
    if (has_loc && clock.year > 0) {
        uint8_t m = clock.month;
        uint8_t d = clock.day;
        if (lat > 35 && lat < 72 && lng > -12 && lng < 40) {
            if (m >= 4 && m <= 9) dst = 1;
            else if (m == 3 && d >= 25) dst = 1;
            else if (m == 10 && d < 25) dst = 1;
        }
        else if (lng < -52 && lng > -130) {
            if (m >= 4 && m <= 10) dst = 1;
            else if (m == 3 && d >= 8) dst = 1;
            else if (m == 11 && d < 8) dst = 1;
        }
    }

    int local_h = utc_h + clock.tz_offset_hours + dst;
    if (local_h < 0) local_h += 24;
    if (local_h >= 24) local_h -= 24;

    clock.hour = (uint8_t)local_h;
    clock.minute = utc_m;
    clock.second = utc_s;
}

// Message history — allocated in PSRAM to save DRAM
StoredMessage* messages = nullptr;
int message_count = 0;

void init_messages() {
    if (!messages) {
        messages = (StoredMessage*)heap_caps_calloc(MAX_STORED_MESSAGES, sizeof(StoredMessage), MALLOC_CAP_SPIRAM);
    }
}

void delete_message(int idx) {
    if (idx < 0 || idx >= message_count) return;
    for (int i = idx; i < message_count - 1; i++) {
        messages[i] = messages[i + 1];
    }
    message_count--;
}

// Sleep
static const uint32_t timeout_presets[] = {0, 60000, 120000, 300000, 900000, 1800000};
Sleep sleep_cfg = {};
static bool sleep_loaded = false;

void touch_activity() {
    sleep_cfg.last_activity_ms = millis();
}

bool should_sleep() {
    // Load from NVS on first check
    if (!sleep_loaded) {
        sleep_cfg.timeout_idx = nvs_param_get_u8(NVS_ID_SLEEP_TIMEOUT);
        if (sleep_cfg.timeout_idx >= 6) sleep_cfg.timeout_idx = 3;
        sleep_cfg.timeout_ms = timeout_presets[sleep_cfg.timeout_idx];
        sleep_cfg.last_activity_ms = millis();
        sleep_loaded = true;
    }
    if (sleep_cfg.timeout_ms == 0) return false;
    return (millis() - sleep_cfg.last_activity_ms) > sleep_cfg.timeout_ms;
}

} // namespace model
