#include <Arduino.h>
#include <esp_heap_caps.h>
#include "statusbar.h"
#include "../ui_theme.h"
#include "../../model.h"
#include "../../mesh/mesh_task.h"

namespace ui::statusbar {

static lv_obj_t* bar_obj = NULL;
static lv_obj_t* lbl_time = NULL;
static lv_obj_t* lbl_battery = NULL;
static lv_obj_t* lbl_gps = NULL;
static lv_obj_t* lbl_ble = NULL;
static lv_obj_t* lbl_dram = NULL;

static void do_update() {
    if (!bar_obj) return;

    lv_label_set_text_fmt(lbl_time, "%02d:%02d", model::clock.hour, model::clock.minute);

    // GPS: icon when fix, warning icon when no fix
    if (model::gps.has_fix) {
        lv_label_set_text(lbl_gps, LV_SYMBOL_GPS);
    } else if (model::gps.module_ok) {
        lv_label_set_text(lbl_gps, LV_SYMBOL_WARNING);
    } else {
        lv_label_set_text(lbl_gps, "  ");
    }

    // Free memory
    uint32_t dram_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
    uint32_t psram_kb = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
    lv_label_set_text_fmt(lbl_dram, "D%luK P%luK", (unsigned long)dram_kb, (unsigned long)psram_kb);

    // BLE: icon when active
    if (mesh::task::ble_is_enabled()) {
        lv_label_set_text(lbl_ble, LV_SYMBOL_BLUETOOTH);
    } else {
        lv_label_set_text(lbl_ble, "  ");
    }

    // Battery
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
    lv_obj_t* layer = lv_layer_top();

    bar_obj = lv_obj_create(layer);
    lv_obj_set_size(bar_obj, lv_pct(98), 50);
    lv_obj_set_pos(bar_obj, 5, 5);
    lv_obj_set_style_bg_color(bar_obj, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_obj, 5, LV_PART_MAIN);
    lv_obj_clear_flag(bar_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar_obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar_obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar_obj, 8, LV_PART_MAIN);

    const lv_font_t *sb_font = &lv_font_noto_24;

    // Left side: time, BLE, GPS
    lbl_time = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_time, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_time, "--:--");

    lbl_ble = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_ble, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_ble, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_ble, "  ");

    lbl_gps = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_gps, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_gps, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_gps, "  ");

    lbl_dram = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_dram, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_dram, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_dram, "");

    // Right side: battery (flex-grow pushes it right)
    lbl_battery = lv_label_create(bar_obj);
    lv_obj_set_flex_grow(lbl_battery, 1);
    lv_obj_set_style_text_align(lbl_battery, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
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
