#include <Arduino.h>
#include <esp_heap_caps.h>
#include "statusbar.h"
#include "../ui_theme.h"
#include "../../model.h"
#include "../../nvs_param.h"

namespace ui::statusbar {

static lv_obj_t* bar_obj = NULL;
static lv_obj_t* lbl_time = NULL;
static lv_obj_t* lbl_battery = NULL;
static lv_obj_t* lbl_gps = NULL;
static lv_obj_t* lbl_ble = NULL;
static lv_obj_t* spacer = NULL;
static lv_obj_t* lbl_memory = NULL;
static bool memory_enabled_state = false;
static char cached_time[8] = {};
static char cached_gps[8] = {};
static char cached_ble[8] = {};
static char cached_memory[24] = {};
static char cached_battery[24] = {};

static void set_label_text_if_changed(lv_obj_t* label, char* cache, size_t cache_size, const char* text) {
    if (!label || !text) return;
    if (strcmp(cache, text) == 0) return;
    strncpy(cache, text, cache_size - 1);
    cache[cache_size - 1] = 0;
    lv_label_set_text(label, cache);
}

static void set_label_visible(lv_obj_t* label, bool visible) {
    if (!label) return;
    if (visible) {
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void sync_memory_visibility() {
    set_label_visible(lbl_memory, memory_enabled_state);
}

static void do_update(uint32_t flags) {
    if (!bar_obj) return;

    char buf[32];
    if (flags & model::DIRTY_CLOCK) {
        snprintf(buf, sizeof(buf), "%02d:%02d", model::clock.hour, model::clock.minute);
        set_label_text_if_changed(lbl_time, cached_time, sizeof(cached_time), buf);

        if (memory_enabled_state) {
            size_t dram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
            size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
            size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

            unsigned dram_used_pct = 0;
            unsigned psram_used_pct = 0;
            if (dram_total > 0) {
                dram_used_pct = (unsigned)(((dram_total - dram_free) * 100U) / dram_total);
            }
            if (psram_total > 0) {
                psram_used_pct = (unsigned)(((psram_total - psram_free) * 100U) / psram_total);
            }

            if (psram_total > 0) {
                snprintf(buf, sizeof(buf), "D%u%% P%u%%", dram_used_pct, psram_used_pct);
            } else {
                snprintf(buf, sizeof(buf), "D%u%% P--", dram_used_pct);
            }
            set_label_text_if_changed(lbl_memory, cached_memory, sizeof(cached_memory), buf);
        }
    }

    // FA5 symbols for statusbar
    #define SB_GPS_FIX    "\xEF\x8F\x85" /* 0xF3C5 map-marker-alt (location pin) */
    #define SB_GPS_SEARCH "\xEF\x9A\x89" /* 0xF689 satellite (searching) */
    #define SB_BT_ON      "\xEF\x8A\x94" /* 0xF294 bluetooth-b */

    if (flags & model::DIRTY_GPS) {
        if (model::gps.has_fix) {
            set_label_visible(lbl_gps, true);
            set_label_text_if_changed(lbl_gps, cached_gps, sizeof(cached_gps), SB_GPS_FIX);
        } else if (model::gps.module_ok) {
            set_label_visible(lbl_gps, true);
            set_label_text_if_changed(lbl_gps, cached_gps, sizeof(cached_gps), SB_GPS_SEARCH);
        } else {
            set_label_visible(lbl_gps, false);
        }
    }

    if (flags & model::DIRTY_MESH) {
        if (model::mesh.ble_enabled) {
            set_label_visible(lbl_ble, true);
            set_label_text_if_changed(lbl_ble, cached_ble, sizeof(cached_ble), SB_BT_ON);
        } else {
            set_label_visible(lbl_ble, false);
        }
    }

    if (flags & model::DIRTY_BATTERY) {
        // Battery
        uint16_t pct = model::battery.percent;
        const char* bat_icon = LV_SYMBOL_BATTERY_FULL;
        if (pct < 20) bat_icon = LV_SYMBOL_BATTERY_EMPTY;
        else if (pct < 50) bat_icon = LV_SYMBOL_BATTERY_2;
        else if (pct < 80) bat_icon = LV_SYMBOL_BATTERY_3;

        if (model::battery.charging) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_CHARGE " %d%%", pct);
        } else {
            snprintf(buf, sizeof(buf), "%s %d%%", bat_icon, pct);
        }
        set_label_text_if_changed(lbl_battery, cached_battery, sizeof(cached_battery), buf);
    }
}

lv_obj_t* create() {
    lv_obj_t* layer = lv_layer_top();
    memory_enabled_state = nvs_param_get_u8(NVS_ID_STATUSBAR_MEMORY) != 0;

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
    lv_label_set_text(lbl_ble, LV_SYMBOL_BLUETOOTH);
    lv_obj_add_flag(lbl_ble, LV_OBJ_FLAG_HIDDEN);

    lbl_gps = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_gps, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_gps, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_gps, LV_SYMBOL_GPS);
    lv_obj_add_flag(lbl_gps, LV_OBJ_FLAG_HIDDEN);

    spacer = lv_obj_create(bar_obj);
    lv_obj_set_size(spacer, 0, 0);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(spacer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(spacer, 0, LV_PART_MAIN);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    lbl_memory = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_memory, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_memory, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_memory, "D-- P--");
    sync_memory_visibility();

    lbl_battery = lv_label_create(bar_obj);
    lv_obj_set_style_text_font(lbl_battery, sb_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_battery, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_battery, LV_SYMBOL_BATTERY_FULL " --%");

    do_update(model::DIRTY_CLOCK | model::DIRTY_BATTERY | model::DIRTY_GPS | model::DIRTY_MESH);
    return bar_obj;
}

void update_now(uint32_t flags) { do_update(flags); }

bool memory_enabled() { return memory_enabled_state; }

void set_memory_enabled(bool enabled) {
    memory_enabled_state = enabled;
    sync_memory_visibility();
    if (memory_enabled_state) {
        do_update(model::DIRTY_CLOCK);
    }
}

void show() { if (bar_obj) lv_obj_clear_flag(bar_obj, LV_OBJ_FLAG_HIDDEN); }
void hide() { if (bar_obj) lv_obj_add_flag(bar_obj, LV_OBJ_FLAG_HIDDEN); }

} // namespace ui::statusbar
