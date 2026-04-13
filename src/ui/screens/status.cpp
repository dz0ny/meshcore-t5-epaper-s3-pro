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


    lv_obj_t* menu = lv_obj_create(parent);
    lv_obj_set_size(menu, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_align(menu, LV_ALIGN_TOP_MID, 0, UI_BACK_BTN_Y + UI_BACK_BTN_HEIGHT);
    lv_obj_set_style_bg_opa(menu, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu, 0, LV_PART_MAIN);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui::nav::menu_item(menu, NULL, "Mesh", on_mesh, NULL);
    ui::nav::menu_item(menu, NULL, "GPS", on_gps, NULL);
    ui::nav::menu_item(menu, NULL, "Battery", on_battery, NULL);
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
