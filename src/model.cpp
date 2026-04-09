#include "model.h"
#include "board.h"
#include "mesh/mesh_bridge.h"
#include "mesh/mesh_task.h"
#include "mesh/companion/target.h"
#include "nvs_param.h"

namespace model {

GPS     gps = {};
Battery battery = {};
Mesh    mesh = {};
Clock   clock = {};

void update_gps() {
    double lat, lng;
    board::gps_get_coord(&lat, &lng);
    gps.lat = lat;
    gps.lng = lng;
    gps.satellites = board::gps_satellites();
    gps.has_fix = (lat != 0.0 || lng != 0.0);
    gps.module_ok = board::peri_status[E_PERI_GPS];

    // Feed GPS coordinates to MeshCore SensorManager for location sharing
    if (gps.has_fix) {
        sensors.node_lat = lat;
        sensors.node_lon = lng;
    }

    if (!gps.module_ok) {
        gps.status_text = "No Module";
    } else if (board::gps.charsProcessed() < 10) {
        gps.status_text = "No Data";
    } else if (gps.has_fix) {
        gps.status_text = "Fix OK";
    } else {
        gps.status_text = "Searching...";
    }

    gps.chars_processed = board::gps.charsProcessed();

    if (board::gps.speed.isValid()) gps.speed_kmh = board::gps.speed.kmph();
    if (board::gps.altitude.isValid()) gps.altitude_m = board::gps.altitude.meters();
    if (board::gps.hdop.isValid()) gps.hdop = board::gps.hdop.hdop();
    if (board::gps.course.isValid()) gps.course_deg = board::gps.course.deg();

    if (board::gps.time.isValid()) {
        gps.hour = board::gps.time.hour();
        gps.minute = board::gps.time.minute();
        gps.second = board::gps.time.second();
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
    // RTC is always UTC
    uint8_t utc_h, utc_m, utc_s;
    board::rtc_get_time(&utc_h, &utc_m, &utc_s);
    board::rtc_get_date(&clock.year, &clock.month, &clock.day, nullptr);

    // Auto timezone from GPS coordinates, fallback to MeshCore advertised location.
    double lat = gps.lat;
    double lng = gps.lng;
    bool has_loc = gps.has_fix && (lat != 0.0 || lng != 0.0);

    // Fallback: use MeshCore advertised location if no GPS fix
    if (!has_loc && mesh.node_name) {
        lat = mesh::task::get_node_lat();
        lng = mesh::task::get_node_lon();
        has_loc = (lat != 0.0 || lng != 0.0);
    }

    if (has_loc) {

        // Europe special cases
        if (lat > 35 && lat < 72 && lng > -12 && lng < 40) {
            if (lng < 0)            clock.tz_offset_hours = 0;  // UK, Portugal, Iceland
            else if (lng < 16)      clock.tz_offset_hours = 1;  // CET: France, Germany, Italy, Spain, Slovenia, Croatia
            else if (lng < 30)      clock.tz_offset_hours = 2;  // EET: Finland, Greece, Romania, Turkey
            else                    clock.tz_offset_hours = 3;  // Moscow
        }
        // Americas
        else if (lng < -52) {
            if (lng < -120)         clock.tz_offset_hours = -8; // PST
            else if (lng < -105)    clock.tz_offset_hours = -7; // MST
            else if (lng < -90)     clock.tz_offset_hours = -6; // CST
            else if (lng < -70)     clock.tz_offset_hours = -5; // EST
            else                    clock.tz_offset_hours = -4; // AST
        }
        // Asia/Pacific
        else if (lng > 40) {
            if (lng < 60)           clock.tz_offset_hours = 4;  // Gulf
            else if (lng < 82)      clock.tz_offset_hours = 5;  // Pakistan/India
            else if (lng < 98)      clock.tz_offset_hours = 6;  // Bangladesh
            else if (lng < 105)     clock.tz_offset_hours = 7;  // Thailand
            else if (lng < 120)     clock.tz_offset_hours = 8;  // China
            else if (lng < 135)     clock.tz_offset_hours = 9;  // Japan/Korea
            else if (lng < 150)     clock.tz_offset_hours = 10; // Australia East
            else                    clock.tz_offset_hours = 12; // NZ
        }
        // Fallback: longitude / 15
        else {
            clock.tz_offset_hours = (int8_t)(lng / 15.0);
        }
    }

    // DST detection
    int8_t dst = 0;
    if (has_loc && clock.year > 0) {
        uint8_t m = clock.month;
        uint8_t d = clock.day;
        // European DST: last Sunday of March → last Sunday of October
        // Simplified: April-September always DST, March if d>=25, October if d<25
        if (lat > 35 && lat < 72 && lng > -12 && lng < 40) {
            if (m >= 4 && m <= 9) {
                dst = 1;  // April through September: always DST
            } else if (m == 3 && d >= 25) {
                dst = 1;  // Late March
            } else if (m == 10 && d < 25) {
                dst = 1;  // Early October
            }
        }
        // US DST: second Sunday of March → first Sunday of November
        // Simplified: April-October always DST, March if d>=8, November if d<8
        else if (lng < -52 && lng > -130) {
            if (m >= 4 && m <= 10) {
                dst = 1;
            } else if (m == 3 && d >= 8) {
                dst = 1;
            } else if (m == 11 && d < 8) {
                dst = 1;
            }
        }
    }

    // Apply timezone + DST to get local time
    int local_h = utc_h + clock.tz_offset_hours + dst;
    if (local_h < 0) local_h += 24;
    if (local_h >= 24) local_h -= 24;

    clock.hour = (uint8_t)local_h;
    clock.minute = utc_m;
    clock.second = utc_s;
}

// Message history
StoredMessage messages[MAX_STORED_MESSAGES] = {};
int message_count = 0;

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
