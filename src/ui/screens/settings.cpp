#include <Arduino.h>
#include "settings.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"

namespace ui::screen::settings {

static lv_obj_t* scr = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }
static void on_status(lv_event_t* e) { ui::screen_mgr::push(SCREEN_STATUS, true); }
static void on_preferences(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SETTINGS_PREFERENCES, true); }
static void on_debug(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SETTINGS_DEBUG, true); }
static void on_device(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SETTINGS_DEVICE, true); }

static void create(lv_obj_t* parent) {
    scr = parent;

    lv_obj_t* menu = ui::nav::scroll_list(parent);

    ui::nav::menu_item(menu, NULL, "Status", on_status, NULL);
    ui::nav::menu_item(menu, NULL, "Preferences", on_preferences, NULL);
    ui::nav::menu_item(menu, NULL, "Debug", on_debug, NULL);
    ui::nav::menu_item(menu, NULL, "Device", on_device, NULL);
}

static void entry() {}
static void exit_fn() {}
static void destroy() { scr = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::settings
