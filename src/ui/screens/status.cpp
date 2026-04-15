#include "status.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"

namespace ui::screen::status {

static lv_obj_t* scr = NULL;

static void on_back(lv_event_t* e) {
    ui::screen_mgr::pop(true);
}

static void on_mesh(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_MESH, true);
}

static void on_gps(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_GPS, true);
}

static void on_battery(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_BATTERY, true);
}

static void create(lv_obj_t* parent) {
    scr = parent;

    lv_obj_t* menu = ui::nav::scroll_list(parent);

    ui::nav::menu_item(menu, NULL, "Battery", on_battery, NULL);
    ui::nav::menu_item(menu, NULL, "GPS Info", on_gps, NULL);
    ui::nav::menu_item(menu, NULL, "Mesh Info", on_mesh, NULL);
}

static void entry() {}
static void exit_fn() {}
static void destroy() { scr = NULL; }

screen_lifecycle_t lifecycle = {
    .create  = create,
    .entry   = entry,
    .exit    = exit_fn,
    .destroy = destroy,
};

} // namespace ui::screen::status
