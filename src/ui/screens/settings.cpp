#include <Arduino.h>
#include "settings.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/statusbar.h"
#include "../../board.h"
#include "../../mesh/mesh_task.h"
#include "../../nvs_param.h"

extern void do_power_off();

namespace ui::screen::settings {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_memory = NULL;
static lv_obj_t* lbl_hit_areas = NULL;
static lv_obj_t* lbl_hit_hint = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }
static void on_battery(lv_event_t* e) { ui::screen_mgr::push(SCREEN_BATTERY, true); }
static void on_gps_status(lv_event_t* e) { ui::screen_mgr::push(SCREEN_GPS, true); }
static void on_mesh_status(lv_event_t* e) { ui::screen_mgr::push(SCREEN_MESH, true); }
static void on_display(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_DISPLAY, true); }
static void on_gps(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_GPS, true); }
static void on_mesh(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_MESH, true); }
static void on_ble(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_BLE, true); }
static void on_storage(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_STORAGE, true); }
static void on_power_off(lv_event_t* e) { do_power_off(); }
static void on_memory_toggle(lv_event_t* e) {
    bool enabled = !ui::statusbar::memory_enabled();
    ui::statusbar::set_memory_enabled(enabled);
    nvs_param_set_u8(NVS_ID_STATUSBAR_MEMORY, enabled ? 1 : 0);
    if (lbl_memory) lv_label_set_text(lbl_memory, enabled ? "On" : "Off");
}

static void sync_hit_hint(bool enabled) {
    if (!lbl_hit_hint) return;
    if (enabled) {
        lv_obj_clear_flag(lbl_hit_hint, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_hit_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_hit_area_toggle(lv_event_t* e) {
    bool enabled = !ui::nav::hit_area_debug_enabled();
    ui::nav::set_hit_area_debug(enabled);
    nvs_param_set_u8(NVS_ID_HIT_AREA_DEBUG, enabled ? 1 : 0);
    if (lbl_hit_areas) lv_label_set_text(lbl_hit_areas, enabled ? "On" : "Off");
    sync_hit_hint(enabled);
    ui::screen_mgr::reload_stack();
}
static void on_reboot(lv_event_t* e) {
    mesh::task::flush_storage();
    ESP.restart();
}

static void add_section_label(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
#ifdef BOARD_TDECK
    lv_obj_set_style_pad_top(label, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(label, 2, LV_PART_MAIN);
#else
    lv_obj_set_style_pad_top(label, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(label, 6, LV_PART_MAIN);
#endif
    lv_label_set_text(label, text);
}

static void create(lv_obj_t* parent) {
    scr = parent;

    lv_obj_t* menu = ui::nav::scroll_list(parent);

    add_section_label(menu, "Status");
    ui::nav::menu_item(menu, NULL, "Battery", on_battery, NULL);
    ui::nav::menu_item(menu, NULL, "GPS Info", on_gps_status, NULL);
    ui::nav::menu_item(menu, NULL, "Mesh Info", on_mesh_status, NULL);

    add_section_label(menu, "Preferences");
    ui::nav::menu_item(menu, NULL, "Display", on_display, NULL);
    ui::nav::menu_item(menu, NULL, "Bluetooth", on_ble, NULL);
    ui::nav::menu_item(menu, NULL, "GPS Settings", on_gps, NULL);
    ui::nav::menu_item(menu, NULL, "Mesh Settings", on_mesh, NULL);
    ui::nav::menu_item(menu, NULL, "Storage", on_storage, NULL);

    add_section_label(menu, "Debug");
    lbl_hit_areas = ui::nav::toggle_item(menu, "Hit Areas", ui::nav::hit_area_debug_enabled() ? "On" : "Off", on_hit_area_toggle, NULL);
    lbl_hit_hint = lv_label_create(menu);
    lv_obj_set_width(lbl_hit_hint, lv_pct(100));
    lv_obj_set_style_text_font(lbl_hit_hint, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_hit_hint, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_pad_left(lbl_hit_hint, UI_MENU_ITEM_INSET, LV_PART_MAIN);
    lv_obj_set_style_pad_right(lbl_hit_hint, UI_MENU_ITEM_INSET, LV_PART_MAIN);
    lv_obj_set_style_pad_top(lbl_hit_hint, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(lbl_hit_hint, 4, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_hit_hint, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl_hit_hint, "Hint: outlined rows show the full touch target.");
    sync_hit_hint(ui::nav::hit_area_debug_enabled());
    lbl_memory = ui::nav::toggle_item(menu, "Memory Bar", ui::statusbar::memory_enabled() ? "On" : "Off", on_memory_toggle, NULL);

    add_section_label(menu, "Device");
    ui::nav::menu_item(menu, NULL, "Reboot", on_reboot, NULL);
    ui::nav::menu_item(menu, NULL, "Power Off", on_power_off, NULL);

    // Version info at bottom
    lv_obj_t* ver = lv_label_create(menu);
    lv_obj_set_style_text_font(ver, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(ver, "FW: %s  HW: %s", T_PAPER_FW_VERSION, T_PAPER_HW_VERSION);
    lv_obj_set_style_pad_top(ver, 20, LV_PART_MAIN);
}

static void entry() {}
static void exit_fn() {}
static void destroy() { scr = NULL; lbl_memory = NULL; lbl_hit_areas = NULL; lbl_hit_hint = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::settings
