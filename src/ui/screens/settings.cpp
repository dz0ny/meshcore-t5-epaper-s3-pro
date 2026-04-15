#include <Arduino.h>
#include "settings.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"

namespace ui::screen::settings {

static lv_obj_t* scr = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }
static void on_status(lv_event_t* e) { ui::screen_mgr::push(SCREEN_STATUS, true); }
static void on_display(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_DISPLAY, true); }
static void on_ble(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_BLE, true); }
static void on_gps(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_GPS, true); }
static void on_mesh(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_MESH, true); }
static void on_storage(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_STORAGE, true); }
static void on_debug(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SETTINGS_DEBUG, true); }
static void on_device(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SETTINGS_DEVICE, true); }

static void create(lv_obj_t* parent) {
    scr = parent;

    lv_obj_t* menu = ui::nav::scroll_list(parent);

    ui::nav::menu_item(menu, NULL, "Status", on_status, NULL);
    ui::nav::menu_item(menu, NULL, "Display", on_display, NULL);
    ui::nav::menu_item(menu, NULL, "Bluetooth", on_ble, NULL);
    ui::nav::menu_item(menu, NULL, "GPS Settings", on_gps, NULL);
    ui::nav::menu_item(menu, NULL, "Mesh Settings", on_mesh, NULL);
    ui::nav::menu_item(menu, NULL, "Storage", on_storage, NULL);
    ui::nav::menu_item(menu, NULL, "Debug", on_debug, NULL);
    ui::nav::menu_item(menu, NULL, "Device", on_device, NULL);
}

static void entry() {}
static void exit_fn() {}
static void destroy() { scr = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::settings
