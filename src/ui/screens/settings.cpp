#include "settings.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../board.h"

namespace ui::screen::settings {

static lv_obj_t* scr = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }
static void on_display(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_DISPLAY, true); }
static void on_gps(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_GPS, true); }
static void on_mesh(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_MESH, true); }
static void on_ble(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_BLE, true); }
static void on_storage(lv_event_t* e) { ui::screen_mgr::push(SCREEN_SET_STORAGE, true); }

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Settings", on_back);

    lv_obj_t* menu = lv_obj_create(parent);
    lv_obj_set_size(menu, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_align(menu, LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_set_style_bg_opa(menu, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu, 0, LV_PART_MAIN);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui::nav::menu_item(menu, NULL, "Display", on_display, NULL);
    ui::nav::menu_item(menu, NULL, "Bluetooth", on_ble, NULL);
    ui::nav::menu_item(menu, NULL, "GPS", on_gps, NULL);
    ui::nav::menu_item(menu, NULL, "Mesh", on_mesh, NULL);
    ui::nav::menu_item(menu, NULL, "Storage", on_storage, NULL);

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
