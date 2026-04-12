#include "mesh_settings.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"

namespace ui::screen::mesh_settings {

static lv_obj_t* scr = NULL;
static lv_timer_t* update_timer = NULL;

static lv_obj_t* lbl_name = NULL;
static lv_obj_t* lbl_freq = NULL;
static lv_obj_t* lbl_bw = NULL;
static lv_obj_t* lbl_sf = NULL;
static lv_obj_t* lbl_cr = NULL;
static lv_obj_t* lbl_txpow = NULL;
static lv_obj_t* lbl_peers = NULL;
static lv_obj_t* lbl_rx = NULL;
static lv_obj_t* lbl_tx = NULL;
static lv_obj_t* lbl_rssi = NULL;
static lv_obj_t* lbl_snr = NULL;

static lv_obj_t* info_row(lv_obj_t* parent, const char* label) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(row, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl, label);

    lv_obj_t* val = lv_label_create(row);
    lv_obj_set_style_text_font(val, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(val, "--");
    return val;
}

static lv_obj_t* section_header(lv_obj_t* parent, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_pad_top(lbl, 10, LV_PART_MAIN);
    lv_label_set_text(lbl, text);
    return lbl;
}

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void update_cb(lv_timer_t* t) {
    auto& m = model::mesh;
    lv_label_set_text(lbl_name, m.node_name ? m.node_name : "--");
    lv_label_set_text_fmt(lbl_freq, "%.3f MHz", m.freq_mhz);
    lv_label_set_text_fmt(lbl_bw, "%.1f kHz", m.bw_khz);
    lv_label_set_text_fmt(lbl_sf, "%d", m.sf);
    lv_label_set_text_fmt(lbl_cr, "4/%d", m.cr);
    lv_label_set_text_fmt(lbl_txpow, "%d dBm", m.tx_power_dbm);
    lv_label_set_text_fmt(lbl_peers, "%d", m.peer_count);
    lv_label_set_text_fmt(lbl_rx, "%lu", (unsigned long)m.rx_packets);
    lv_label_set_text_fmt(lbl_tx, "%lu", (unsigned long)m.tx_packets);
    lv_label_set_text_fmt(lbl_rssi, "%.0f dBm", m.last_rssi);
    lv_label_set_text_fmt(lbl_snr, "%.1f dB", m.last_snr);
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Mesh", on_back);

    lv_obj_t* list = ui::nav::scroll_list(parent);

    section_header(list, "Radio");
    lbl_name   = info_row(list, "Node");
    lbl_freq   = info_row(list, "Freq");
    lbl_bw     = info_row(list, "BW");
    lbl_sf     = info_row(list, "SF");
    lbl_cr     = info_row(list, "CR");
    lbl_txpow  = info_row(list, "TX Power");

    section_header(list, "Stats");
    lbl_peers  = info_row(list, "Peers");
    lbl_rx     = info_row(list, "RX Pkts");
    lbl_tx     = info_row(list, "TX Pkts");
    lbl_rssi   = info_row(list, "RSSI");
    lbl_snr    = info_row(list, "SNR");
}

static void entry() {
    update_timer = lv_timer_create(update_cb, 3000, NULL);
    update_cb(NULL);
}

static void exit_fn() {
    if (update_timer) { lv_timer_del(update_timer); update_timer = NULL; }
}

static void destroy() {
    scr = NULL;
    lbl_name = lbl_freq = lbl_bw = lbl_sf = lbl_cr = lbl_txpow = NULL;
    lbl_peers = lbl_rx = lbl_tx = lbl_rssi = lbl_snr = NULL;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::mesh_settings
