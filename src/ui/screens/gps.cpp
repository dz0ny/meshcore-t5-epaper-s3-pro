#include "gps.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"

namespace ui::screen::gps {

static lv_obj_t* scr = NULL;
static lv_timer_t* update_timer = NULL;

static lv_obj_t* lbl_status = NULL;
static lv_obj_t* lbl_lat = NULL;
static lv_obj_t* lbl_lng = NULL;
static lv_obj_t* lbl_sats = NULL;
static lv_obj_t* lbl_time = NULL;
static lv_obj_t* lbl_speed = NULL;
static lv_obj_t* lbl_altitude = NULL;
static lv_obj_t* lbl_hdop = NULL;
static lv_obj_t* lbl_course = NULL;
static lv_obj_t* lbl_chars = NULL;

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
    lv_obj_set_style_text_font(lbl, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl, label);

    lv_obj_t* val = lv_label_create(row);
    lv_obj_set_style_text_font(val, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(val, "--");
    return val;
}

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void update_cb(lv_timer_t* t) {
    auto& g = model::gps;
    lv_label_set_text(lbl_status, g.status_text ? g.status_text : "--");
    lv_label_set_text_fmt(lbl_lat, "%.6f", g.lat);
    lv_label_set_text_fmt(lbl_lng, "%.6f", g.lng);
    lv_label_set_text_fmt(lbl_sats, "%lu", (unsigned long)g.satellites);
    lv_label_set_text_fmt(lbl_time, "%02d:%02d:%02d", g.hour, g.minute, g.second);
    lv_label_set_text_fmt(lbl_speed, "%.1f km/h", g.speed_kmh);
    lv_label_set_text_fmt(lbl_altitude, "%.1f m", g.altitude_m);
    lv_label_set_text_fmt(lbl_hdop, "%.1f", g.hdop);
    lv_label_set_text_fmt(lbl_course, "%.1f", g.course_deg);
    lv_label_set_text_fmt(lbl_chars, "%lu", (unsigned long)g.chars_processed);
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "GPS", on_back);

    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(95), lv_pct(85));
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(list, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    lbl_status   = info_row(list, "Status");
    lbl_lat      = info_row(list, "Latitude");
    lbl_lng      = info_row(list, "Longitude");
    lbl_sats     = info_row(list, "Satellites");
    lbl_time     = info_row(list, "GPS Time");
    lbl_speed    = info_row(list, "Speed");
    lbl_altitude = info_row(list, "Altitude");
    lbl_hdop     = info_row(list, "HDOP");
    lbl_course   = info_row(list, "Course");
    lbl_chars    = info_row(list, "Chars");
}

static void entry() {
    update_timer = lv_timer_create(update_cb, 2000, NULL);
    update_cb(NULL);
}

static void exit_fn() {
    if (update_timer) { lv_timer_del(update_timer); update_timer = NULL; }
}

static void destroy() {
    scr = NULL;
    lbl_status = lbl_lat = lbl_lng = lbl_sats = lbl_time = NULL;
    lbl_speed = lbl_altitude = lbl_hdop = lbl_course = lbl_chars = NULL;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::gps
