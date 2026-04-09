#include <Arduino.h>
#include "statusbar.h"
#include "../ui_theme.h"
#include "../../model.h"

namespace ui::statusbar {

static lv_obj_t* bar_obj = NULL;
static lv_obj_t* lbl_time = NULL;
static lv_obj_t* lbl_battery = NULL;
static lv_obj_t* lbl_mesh = NULL;
static lv_obj_t* lbl_gps = NULL;
static lv_obj_t* lbl_rxtx = NULL;

static void do_update() {
    if (!bar_obj) return;

    lv_label_set_text_fmt(lbl_time, "%02d:%02d", model::clock.hour, model::clock.minute);
    lv_label_set_text_fmt(lbl_mesh, LV_SYMBOL_WIFI " %d", model::mesh.peer_count);
    lv_label_set_text_fmt(lbl_rxtx, "%lu/%lu",
        (unsigned long)model::mesh.rx_packets, (unsigned long)model::mesh.tx_packets);

    if (model::gps.satellites > 0) {
        lv_label_set_text_fmt(lbl_gps, LV_SYMBOL_GPS " %lu", (unsigned long)model::gps.satellites);
    } else {
        lv_label_set_text(lbl_gps, LV_SYMBOL_GPS " --");
    }

    uint16_t pct = model::battery.percent;
    const char* bat_icon = LV_SYMBOL_BATTERY_FULL;
    if (pct < 20) bat_icon = LV_SYMBOL_BATTERY_EMPTY;
    else if (pct < 50) bat_icon = LV_SYMBOL_BATTERY_2;
    else if (pct < 80) bat_icon = LV_SYMBOL_BATTERY_3;

    if (model::battery.charging) {
        lv_label_set_text_fmt(lbl_battery, LV_SYMBOL_CHARGE " %d%%", pct);
    } else {
        lv_label_set_text_fmt(lbl_battery, "%s %d%%", bat_icon, pct);
    }
}

lv_obj_t* create(lv_obj_t* parent) {
    // Create on lv_layer_top() — persists across all screen switches.
    // Called ONCE from ui::task::start(), never destroyed.
    lv_obj_t* layer = lv_layer_top();

    bar_obj = lv_obj_create(layer);
    lv_obj_set_size(bar_obj, lv_pct(98), 50);
    lv_obj_set_pos(bar_obj, 5, 5);
    lv_obj_set_style_bg_color(bar_obj, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_obj, 5, LV_PART_MAIN);
    lv_obj_clear_flag(bar_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar_obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar_obj, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const lv_font_t *sb_font = &lv_font_montserrat_24;

    lbl_time = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_time, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_time, "--:--");

    lbl_mesh = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_mesh, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_mesh, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_mesh, LV_SYMBOL_WIFI " 0");

    lbl_rxtx = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_rxtx, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_rxtx, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_rxtx, "0/0");

    lbl_gps = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_gps, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_gps, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_gps, LV_SYMBOL_GPS " --");

    lbl_battery = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_battery, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_battery, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_battery, LV_SYMBOL_BATTERY_FULL " --%");

    do_update();
    return bar_obj;
}

void update_now() { do_update(); }

void show() { if (bar_obj) lv_obj_clear_flag(bar_obj, LV_OBJ_FLAG_HIDDEN); }
void hide() { if (bar_obj) lv_obj_add_flag(bar_obj, LV_OBJ_FLAG_HIDDEN); }

} // namespace ui::statusbar
