#include <cstdio>
#include "set_mesh.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"
#include "../../mesh/mesh_task.h"

namespace ui::screen::set_mesh {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_freq = NULL;
static lv_obj_t* lbl_bw = NULL;
static lv_obj_t* lbl_sf = NULL;
static lv_obj_t* lbl_cr = NULL;
static lv_obj_t* lbl_txpow = NULL;
static lv_obj_t* lbl_gps_share = NULL;
static lv_obj_t* lbl_ble = NULL;

static const float freqs[] = {868.0, 869.525, 869.618, 915.0, 433.0};
static const int n_freqs = 5;
static const float bws[] = {31.25, 62.5, 125.0, 250.0, 500.0};
static const int n_bws = 5;
static const uint8_t sfs[] = {7, 8, 9, 10, 11, 12};
static const int n_sfs = 6;
static const uint8_t crs[] = {5, 6, 7, 8};
static const int n_crs = 4;
static const int8_t powers[] = {2, 5, 10, 14, 17, 20, 22};
static const int n_powers = 7;

static int find_f(const float* a, int n, float v) { for (int i=0;i<n;i++) if(a[i]==v) return i; return 0; }
static int find_u8(const uint8_t* a, int n, uint8_t v) { for (int i=0;i<n;i++) if(a[i]==v) return i; return 0; }
static int find_i8(const int8_t* a, int n, int8_t v) { for (int i=0;i<n;i++) if(a[i]==v) return i; return 0; }

static char buf[32]; // temp for formatting

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_freq(lv_event_t* e) {
    int i = (find_f(freqs, n_freqs, mesh::task::get_freq()) + 1) % n_freqs;
    mesh::task::set_freq(freqs[i]);
    snprintf(buf, sizeof(buf), "%.3f", freqs[i]);
    if (lbl_freq) lv_label_set_text(lbl_freq, buf);
}
static void on_bw(lv_event_t* e) {
    int i = (find_f(bws, n_bws, mesh::task::get_bw()) + 1) % n_bws;
    mesh::task::set_bw(bws[i]);
    snprintf(buf, sizeof(buf), "%.1f kHz", bws[i]);
    if (lbl_bw) lv_label_set_text(lbl_bw, buf);
}
static void on_sf(lv_event_t* e) {
    int i = (find_u8(sfs, n_sfs, mesh::task::get_sf()) + 1) % n_sfs;
    mesh::task::set_sf(sfs[i]);
    snprintf(buf, sizeof(buf), "%d", sfs[i]);
    if (lbl_sf) lv_label_set_text(lbl_sf, buf);
}
static void on_cr(lv_event_t* e) {
    int i = (find_u8(crs, n_crs, mesh::task::get_cr()) + 1) % n_crs;
    mesh::task::set_cr(crs[i]);
    snprintf(buf, sizeof(buf), "4/%d", crs[i]);
    if (lbl_cr) lv_label_set_text(lbl_cr, buf);
}
static void on_txpow(lv_event_t* e) {
    int i = (find_i8(powers, n_powers, mesh::task::get_tx_power()) + 1) % n_powers;
    mesh::task::set_tx_power(powers[i]);
    snprintf(buf, sizeof(buf), "%d dBm", powers[i]);
    if (lbl_txpow) lv_label_set_text(lbl_txpow, buf);
}
static void on_gps_share(lv_event_t* e) {
    bool cur = mesh::task::get_advert_location();
    mesh::task::set_advert_location(!cur);
    if (lbl_gps_share) lv_label_set_text(lbl_gps_share, !cur ? "On" : "Off");
}
static void on_ble_toggle(lv_event_t* e) {
    if (mesh::task::ble_is_enabled()) {
        mesh::task::ble_disable();
        if (lbl_ble) lv_label_set_text(lbl_ble, "Off");
    } else {
        mesh::task::ble_enable();
        if (lbl_ble) lv_label_set_text(lbl_ble, "On");
    }
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Mesh Config", on_back);

    lv_obj_t* list = ui::nav::scroll_list(parent);

    auto& m = model::mesh;
    snprintf(buf, sizeof(buf), "%.3f", m.freq_mhz);
    lbl_freq = ui::nav::toggle_item(list, "Freq", buf, on_freq, NULL);
    snprintf(buf, sizeof(buf), "%.1f kHz", m.bw_khz);
    lbl_bw = ui::nav::toggle_item(list, "BW", buf, on_bw, NULL);
    snprintf(buf, sizeof(buf), "%d", m.sf);
    lbl_sf = ui::nav::toggle_item(list, "SF", buf, on_sf, NULL);
    snprintf(buf, sizeof(buf), "4/%d", m.cr);
    lbl_cr = ui::nav::toggle_item(list, "CR", buf, on_cr, NULL);
    snprintf(buf, sizeof(buf), "%d dBm", m.tx_power_dbm);
    lbl_txpow = ui::nav::toggle_item(list, "TX Power", buf, on_txpow, NULL);
    lbl_gps_share = ui::nav::toggle_item(list, "GPS Share", mesh::task::get_advert_location() ? "On" : "Off", on_gps_share, NULL);

    lbl_ble = ui::nav::toggle_item(list, "BLE", mesh::task::ble_is_enabled() ? "On" : "Off", on_ble_toggle, NULL);

    // Read-only info
    ui::nav::toggle_item(list, "Node", m.node_name ? m.node_name : "--", nullptr, NULL);
    snprintf(buf, sizeof(buf), "%d", BLE_PIN_CODE);
    ui::nav::toggle_item(list, "BLE PIN", buf, nullptr, NULL);
}

static void entry() {}
static void exit_fn() {}
static void destroy() {
    scr = NULL;
    lbl_freq = lbl_bw = lbl_sf = lbl_cr = lbl_txpow = lbl_gps_share = lbl_ble = NULL;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::set_mesh
