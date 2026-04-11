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
static void on_display(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_DISPLAY, true); }
static void on_gps(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_GPS, true); }
static void on_mesh(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_MESH, true); }
static void on_ble(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_BLE, true); }
static void on_storage(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_STORAGE, true); }
static void on_power_off(lv_event_t* e) { do_power_off(); }
static void on_reboot(lv_event_t* e) { ESP.restart(); }

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Settings", on_back);

    lv_obj_t* menu = ui::nav::scroll_list(parent);

    ui::nav::menu_item(menu, NULL, "Display", on_display, NULL);
    ui::nav::menu_item(menu, NULL, "Bluetooth", on_ble, NULL);
    ui::nav::menu_item(menu, NULL, "GPS", on_gps, NULL);
    ui::nav::menu_item(menu, NULL, "Mesh", on_mesh, NULL);
    ui::nav::menu_item(menu, NULL, "Storage", on_storage, NULL);

    ui::nav::menu_item(menu, NULL, "Power Off", on_power_off, NULL);
    ui::nav::menu_item(menu, NULL, "Reboot", on_reboot, NULL);

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
