#include <Arduino.h>
#include "lock.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../ui_port.h"
#include "../components/statusbar.h"
#include "../../model.h"
#include "../../board.h"

namespace ui::screen::lock {

static lv_obj_t* lbl_node_name = NULL;
static lv_obj_t* lbl_time = NULL;
static lv_obj_t* lbl_date = NULL;
static lv_obj_t* lbl_unread = NULL;
static lv_obj_t* lbl_info = NULL;
static char cached_node_name[64] = {};
static char cached_time[8] = {};
static char cached_date[16] = {};
static char cached_unread[96] = {};
static char cached_info[16] = {};

static void set_label_text(lv_obj_t* label, char* cached, size_t cached_size, const char* text) {
    if (!label || !text) return;
    if (strcmp(cached, text) != 0) {
        strncpy(cached, text, cached_size - 1);
        cached[cached_size - 1] = 0;
        lv_label_set_text(label, cached);
    }
}

static void on_unread_click(lv_event_t* e) {
    model::touch_activity();
    model::clear_unread_messages();
    ui::statusbar::show();
    ui::screen_mgr::pop(false);
    ui::screen_mgr::switch_to(SCREEN_HOME, false);
    ui::screen_mgr::push(SCREEN_CHAT, false);
}

static void create(lv_obj_t* parent) {
    lbl_node_name = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_node_name, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_node_name, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
#if defined(BOARD_TDECK)
    lv_obj_align(lbl_node_name, LV_ALIGN_TOP_MID, 0, 26);
#else
    lv_obj_align(lbl_node_name, LV_ALIGN_TOP_MID, 0, 55);
#endif
    lv_label_set_text(lbl_node_name, model::mesh.node_name ? model::mesh.node_name : T_PAPER_HW_VERSION);

    lbl_time = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_time, UI_FONT_CLOCK_LG, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
#if defined(BOARD_TDECK)
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, 40);
#else
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, 100);
#endif
    lv_label_set_text(lbl_time, "");

    lbl_date = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_date, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
#if defined(BOARD_TDECK)
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 72);
#else
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 210);
#endif
    lv_label_set_text(lbl_date, "");

    // Unread messages — tappable, jumps directly to chat
    lbl_unread = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_unread, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_unread, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_unread, LV_ALIGN_CENTER, 0, 30);
    lv_label_set_text(lbl_unread, "");
    lv_obj_add_flag(lbl_unread, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(lbl_unread, 30);
    lv_obj_add_event_cb(lbl_unread, on_unread_click, LV_EVENT_CLICKED, NULL);

    lbl_info = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_info, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_info, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_label_set_text(lbl_info, "");
}

void update(uint32_t flags) {
    char buf[96];

    if (flags & model::DIRTY_MESH) {
        set_label_text(
            lbl_node_name,
            cached_node_name,
            sizeof(cached_node_name),
            model::mesh.node_name ? model::mesh.node_name : "LilyGo T5 ePaper S3 Pro"
        );
    }

    if (flags & model::DIRTY_CLOCK) {
        snprintf(buf, sizeof(buf), "%02d:%02d", model::clock.hour, model::clock.minute);
        set_label_text(lbl_time, cached_time, sizeof(cached_time), buf);

        snprintf(buf, sizeof(buf), "%02d/%02d/20%02d",
            model::clock.day, model::clock.month, model::clock.year);
        set_label_text(lbl_date, cached_date, sizeof(cached_date), buf);
    }

    if (flags & model::DIRTY_SLEEP) {
        if (model::sleep_cfg.unread_messages > 0) {
            if (model::sleep_cfg.last_sender[0]) {
                snprintf(buf, sizeof(buf), "%d new: %s",
                    model::sleep_cfg.unread_messages, model::sleep_cfg.last_sender);
            } else {
                snprintf(buf, sizeof(buf), "%d new messages", model::sleep_cfg.unread_messages);
            }
        } else {
            buf[0] = 0;
        }
        if (buf[0]) {
            lv_obj_clear_flag(lbl_unread, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_unread, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_add_flag(lbl_unread, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lbl_unread, LV_OBJ_FLAG_CLICKABLE);
        }
        set_label_text(lbl_unread, cached_unread, sizeof(cached_unread), buf);
    }

    if (flags & model::DIRTY_BATTERY) {
        snprintf(buf, sizeof(buf), "BAT %d%%", model::battery.percent);
        set_label_text(lbl_info, cached_info, sizeof(cached_info), buf);
    }
}

static void entry() {
    ui::port::set_backlight(0);
    ui::statusbar::hide();
    update(model::DIRTY_CLOCK | model::DIRTY_BATTERY | model::DIRTY_MESH | model::DIRTY_SLEEP);
}

static void exit_fn() {
}

static void destroy() {
    lbl_node_name = NULL;
    lbl_time = lbl_date = lbl_unread = lbl_info = NULL;
    cached_node_name[0] = 0;
    cached_time[0] = 0;
    cached_date[0] = 0;
    cached_unread[0] = 0;
    cached_info[0] = 0;
}

void show() {
    if (ui::screen_mgr::top_id() == SCREEN_LOCK) return;
    model::update_clock();
    model::update_battery();
    model::update_mesh();
    ui::screen_mgr::push(SCREEN_LOCK, false);
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::lock
