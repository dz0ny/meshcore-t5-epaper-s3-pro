#include "home.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"

namespace ui::screen::home {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_node_name = NULL;
static lv_obj_t* lbl_clock = NULL;
static lv_obj_t* lbl_date = NULL;
static lv_timer_t* clock_timer = NULL;

// ---------- Event handlers ----------

static void on_contacts_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_CONTACTS, true);
}

static void on_chat_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_CHAT, true);
}

static void on_settings_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_SETTINGS, true);
}

static void on_status_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_STATUS, true);
}

// ---------- Clock update ----------

static void clock_cb(lv_timer_t* t) {
    if (!lbl_clock) return;
    lv_label_set_text_fmt(lbl_clock, "%02d:%02d", model::clock.hour, model::clock.minute);
    if (lbl_date) {
        lv_label_set_text_fmt(lbl_date, "%02d/%02d/20%02d",
            model::clock.day, model::clock.month, model::clock.year);
    }
}

// ---------- Lifecycle ----------

static void create(lv_obj_t* parent) {
    scr = parent;

    // Node name (mesh identity)
    lbl_node_name = lv_label_create(parent);
    lv_obj_align(lbl_node_name, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_text_font(lbl_node_name, &Font_Mono_Bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_node_name, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_node_name, model::mesh.node_name ? model::mesh.node_name : "T-Paper");

    // Big clock
    lbl_clock = lv_label_create(parent);
    lv_obj_align(lbl_clock, LV_ALIGN_TOP_MID, 0, 95);
    lv_obj_set_style_text_font(lbl_clock, &Font_Mono_Bold_90, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_clock, "--:--");

    // Date below clock
    lbl_date = lv_label_create(parent);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 195);
    lv_obj_set_style_text_font(lbl_date, &Font_Mono_Bold_25, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_date, "--/--/----");

    // Menu items container
    lv_obj_t* menu = lv_obj_create(parent);
    lv_obj_set_size(menu, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_align(menu, LV_ALIGN_TOP_MID, 0, 240);
    lv_obj_set_style_bg_opa(menu, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu, 0, LV_PART_MAIN);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui::nav::menu_item(menu, NULL, "Contacts", on_contacts_click, NULL);
    ui::nav::menu_item(menu, NULL, "Messages", on_chat_click, NULL);
    ui::nav::menu_item(menu, NULL, "Status", on_status_click, NULL);
    ui::nav::menu_item(menu, NULL, "Settings", on_settings_click, NULL);
}

static void entry() {
    clock_timer = lv_timer_create(clock_cb, 5000, NULL);
    clock_cb(NULL);
}

static void exit_fn() {
    if (clock_timer) {
        lv_timer_del(clock_timer);
        clock_timer = NULL;
    }
}

static void destroy() {
    scr = NULL;
    lbl_node_name = NULL;
    lbl_clock = NULL;
    lbl_date = NULL;
}

screen_lifecycle_t lifecycle = {
    .create  = create,
    .entry   = entry,
    .exit    = exit_fn,
    .destroy = destroy,
};

} // namespace ui::screen::home
