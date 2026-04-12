#include <Arduino.h>
#include "settings.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../board.h"

extern void do_power_off();

namespace ui::screen::settings {

static lv_obj_t* scr = NULL;

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
static void on_reboot(lv_event_t* e) { ESP.restart(); }

static void add_section_label(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_pad_top(label, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(label, 6, LV_PART_MAIN);
    lv_label_set_text(label, text);
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Settings", on_back);

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

    add_section_label(menu, "Device");
    ui::nav::menu_item(menu, NULL, "Reboot", on_reboot, NULL);
    ui::nav::menu_item(menu, NULL, "Power Off", on_power_off, NULL);

    // Version info at bottom
    lv_obj_t* ver = lv_label_create(menu);
    lv_obj_set_style_text_font(ver, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(ver, "FW: %s  HW: %s", T_PAPER_SW_VERSION, T_PAPER_HW_VERSION);
    lv_obj_set_style_pad_top(ver, 20, LV_PART_MAIN);
}

static void entry() {}
static void exit_fn() {}
static void destroy() { scr = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::settings
