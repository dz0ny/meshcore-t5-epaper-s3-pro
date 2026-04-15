#include <Arduino.h>
#include "settings_device.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../board.h"
#include "../../mesh/mesh_task.h"

extern void do_power_off();

namespace ui::screen::settings_device {

static lv_obj_t* scr = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_reboot(lv_event_t* e) {
    mesh::task::flush_storage();
    ESP.restart();
}

static void on_power_off(lv_event_t* e) { do_power_off(); }

static void create(lv_obj_t* parent) {
    scr = parent;

    lv_obj_t* list = ui::nav::scroll_list(parent);

    ui::nav::menu_item(list, NULL, "Reboot", on_reboot, NULL);
    ui::nav::menu_item(list, NULL, "Power Off", on_power_off, NULL);

    lv_obj_t* ver = lv_label_create(list);
    lv_obj_set_style_text_font(ver, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(ver, "FW: %s  HW: %s", T_PAPER_FW_VERSION, T_PAPER_HW_VERSION);
    lv_obj_set_style_pad_top(ver, 20, LV_PART_MAIN);
}

static void entry() {}
static void exit_fn() {}
static void destroy() { scr = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::settings_device
