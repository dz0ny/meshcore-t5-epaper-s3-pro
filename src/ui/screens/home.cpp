#include <cstdio>
#include <cstring>
#include "home.h"
#include "compose.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"

namespace ui::screen::home {

static constexpr lv_coord_t HOME_NODE_NAME_Y = 55;
static constexpr lv_coord_t HOME_CLOCK_Y = 100;
static constexpr lv_coord_t HOME_DATE_Y = 225;
static constexpr lv_coord_t HOME_MENU_Y = 340;

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_node_name = NULL;
static lv_obj_t* lbl_clock = NULL;
static lv_obj_t* lbl_date = NULL;
static lv_obj_t* lbl_msg_badge = NULL;
static char cached_node_name[64] = {};
static char cached_clock[8] = {};
static char cached_date[16] = {};
static char cached_msg_badge[16] = {};

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

static void on_settings_click(lv_event_t* e) {
    ui::screen_mgr::push(SCREEN_SETTINGS, true);
}

// ---------- Lifecycle ----------

static void create(lv_obj_t* parent) {
    scr = parent;

    // Node name (mesh identity)
    lbl_node_name = lv_label_create(parent);
    lv_obj_align(lbl_node_name, LV_ALIGN_TOP_MID, 0, HOME_NODE_NAME_Y);
    lv_obj_set_style_text_font(lbl_node_name, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_node_name, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_node_name, model::mesh.node_name ? model::mesh.node_name : "T-Paper");

    // Big clock
    lbl_clock = lv_label_create(parent);
    lv_obj_align(lbl_clock, LV_ALIGN_TOP_MID, 0, HOME_CLOCK_Y);
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_bold_120, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl_clock, "%02d:%02d", model::clock.hour, model::clock.minute);

    // Date below clock
    lbl_date = lv_label_create(parent);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, HOME_DATE_Y);
    lv_obj_set_style_text_font(lbl_date, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl_date, "%02d/%02d/20%02d",
        model::clock.day, model::clock.month, model::clock.year);

    // Menu items container
    lv_obj_t* menu = lv_obj_create(parent);
    lv_obj_set_size(menu, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_align(menu, LV_ALIGN_TOP_MID, 0, HOME_MENU_Y);
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
    ui::nav::menu_item(menu, NULL, "Settings", on_settings_click, NULL);
}

// Called by model update cycle (every 2s) via lv_timer — just update labels
void update(uint32_t flags) {
    char buf[64];
    if ((flags & model::DIRTY_CLOCK) && lbl_clock) {
        snprintf(buf, sizeof(buf), "%02d:%02d", model::clock.hour, model::clock.minute);
        if (strcmp(cached_clock, buf) != 0) {
            strncpy(cached_clock, buf, sizeof(cached_clock) - 1);
            cached_clock[sizeof(cached_clock) - 1] = 0;
            lv_label_set_text(lbl_clock, cached_clock);
        }
    }
    if ((flags & model::DIRTY_CLOCK) && lbl_date) {
        snprintf(buf, sizeof(buf), "%02d/%02d/20%02d",
            model::clock.day, model::clock.month, model::clock.year);
        if (strcmp(cached_date, buf) != 0) {
            strncpy(cached_date, buf, sizeof(cached_date) - 1);
            cached_date[sizeof(cached_date) - 1] = 0;
            lv_label_set_text(lbl_date, cached_date);
        }
    }
    if ((flags & model::DIRTY_MESH) && lbl_node_name && model::mesh.node_name) {
        if (strcmp(cached_node_name, model::mesh.node_name) != 0) {
            strncpy(cached_node_name, model::mesh.node_name, sizeof(cached_node_name) - 1);
            cached_node_name[sizeof(cached_node_name) - 1] = 0;
            lv_label_set_text(lbl_node_name, cached_node_name);
        }
    }
    if ((flags & model::DIRTY_SLEEP) && lbl_msg_badge) {
        int unread = model::sleep_cfg.unread_messages;
        if (unread > 0) {
            snprintf(buf, sizeof(buf), "(%d)", unread);
        } else {
            buf[0] = 0;
        }
        if (strcmp(cached_msg_badge, buf) != 0) {
            strncpy(cached_msg_badge, buf, sizeof(cached_msg_badge) - 1);
            cached_msg_badge[sizeof(cached_msg_badge) - 1] = 0;
            lv_label_set_text(lbl_msg_badge, cached_msg_badge);
        }
    }
}

static void entry() {
    update(model::DIRTY_CLOCK | model::DIRTY_MESH | model::DIRTY_SLEEP);
}

static void exit_fn() {}

static void destroy() {
    scr = NULL;
    lbl_node_name = NULL;
    lbl_clock = NULL;
    lbl_msg_badge = NULL;
    lbl_date = NULL;
    cached_node_name[0] = 0;
    cached_clock[0] = 0;
    cached_date[0] = 0;
    cached_msg_badge[0] = 0;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::home
