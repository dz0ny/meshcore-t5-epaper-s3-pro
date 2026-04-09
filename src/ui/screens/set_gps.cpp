#include "set_gps.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"
#include "../../mesh/mesh_task.h"

namespace ui::screen::set_gps {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_gps_enabled = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_gps_toggle(lv_event_t* e) {
    bool cur = mesh::task::get_gps_enabled();
    mesh::task::set_gps_enabled(!cur);
    if (lbl_gps_enabled) lv_label_set_text(lbl_gps_enabled, !cur ? "On" : "Off");
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "GPS Settings", on_back);

    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(95), lv_pct(85));
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(list, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    lbl_gps_enabled = ui::nav::toggle_item(list, "GPS", mesh::task::get_gps_enabled() ? "On" : "Off", on_gps_toggle, NULL);
    ui::nav::toggle_item(list, "Module", model::gps.module_ok ? "OK" : "None", nullptr, NULL);
    ui::nav::toggle_item(list, "RTC Sync", "Auto", nullptr, NULL);
}

static void entry() {}
static void exit_fn() {}
static void destroy() { scr = NULL; lbl_gps_enabled = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::set_gps
