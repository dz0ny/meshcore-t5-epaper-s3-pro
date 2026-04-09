#include "model.h"
#include "board.h"
#include "mesh/mesh_bridge.h"
#include "mesh/mesh_task.h"

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
    board::rtc_get_time(&clock.hour, &clock.minute, &clock.second);
    board::rtc_get_date(&clock.year, &clock.month, &clock.day, nullptr);
}

} // namespace model
