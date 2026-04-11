#include "home.h"
#include "compose.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"

namespace ui::screen::home {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_node_name = NULL;
static lv_obj_t* lbl_clock = NULL;
static lv_obj_t* lbl_date = NULL;
static lv_obj_t* lbl_msg_badge = NULL;

// ---------- Event handlers ----------

static void on_chat_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_CHAT, true);
}

static void on_compose_click(lv_event_t* e) {
    ui::screen::compose::set_recipient(NULL);
    ui::screen_mgr::push(SCREEN_COMPOSE, true);
}

static void on_contacts_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_CONTACTS, true);
}

static void on_discovery_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_DISCOVERY, true);
}

static void on_map_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_MAP, true);
}

static void on_status_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_STATUS, true);
}

static void on_settings_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_SETTINGS, true);
}

// ---------- Lifecycle ----------

static void create(lv_obj_t* parent) {
    scr = parent;

    // Node name (mesh identity)
    lbl_node_name = lv_label_create(parent);
    lv_obj_align(lbl_node_name, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_text_font(lbl_node_name, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_node_name, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_node_name, model::mesh.node_name ? model::mesh.node_name : "T-Paper");

    // Big clock
    lbl_clock = lv_label_create(parent);
    lv_obj_align(lbl_clock, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_bold_120, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl_clock, "%02d:%02d", model::clock.hour, model::clock.minute);

    // Date below clock
    lbl_date = lv_label_create(parent);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 210);
    lv_obj_set_style_text_font(lbl_date, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl_date, "%02d/%02d/20%02d",
        model::clock.day, model::clock.month, model::clock.year);

    // Menu items container
    lv_obj_t* menu = lv_obj_create(parent);
    lv_obj_set_size(menu, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_align(menu, LV_ALIGN_TOP_MID, 0, 255);
    lv_obj_set_style_bg_opa(menu, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu, 0, LV_PART_MAIN);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_msg_badge = ui::nav::toggle_item(menu, "Messages", "", on_chat_click, NULL);
    ui::nav::menu_item(menu, NULL, "Compose", on_compose_click, NULL);
    ui::nav::menu_item(menu, NULL, "Contacts", on_contacts_click, NULL);
    ui::nav::menu_item(menu, NULL, "Discovery", on_discovery_click, NULL);
    ui::nav::menu_item(menu, NULL, "Map", on_map_click, NULL);
    ui::nav::menu_item(menu, NULL, "Status", on_status_click, NULL);
    ui::nav::menu_item(menu, NULL, "Settings", on_settings_click, NULL);
}

// Called by model update cycle (every 2s) via lv_timer — just update labels
void update() {
    if (lbl_clock) {
        lv_label_set_text_fmt(lbl_clock, "%02d:%02d", model::clock.hour, model::clock.minute);
    }
    if (lbl_date) {
        lv_label_set_text_fmt(lbl_date, "%02d/%02d/20%02d",
            model::clock.day, model::clock.month, model::clock.year);
    }
    if (lbl_node_name && model::mesh.node_name) {
        lv_label_set_text(lbl_node_name, model::mesh.node_name);
    }
    if (lbl_msg_badge) {
        int unread = model::sleep_cfg.unread_messages;
        if (unread > 0) {
            lv_label_set_text_fmt(lbl_msg_badge, "(%d)", unread);
        } else {
            lv_label_set_text(lbl_msg_badge, "");
        }
    }
}

static void entry() {
    update();
}

static void exit_fn() {}

static void destroy() {
    scr = NULL;
    lbl_node_name = NULL;
    lbl_clock = NULL;
    lbl_msg_badge = NULL;
    lbl_date = NULL;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::home
