#include "msg_detail.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"
#include "../../sd_log.h"

namespace ui::screen::msg_detail {

static lv_obj_t* scr = NULL;
static int msg_idx = -1;

void set_message(int idx) {
    msg_idx = idx;
}

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_delete(lv_event_t* e) {
    if (msg_idx >= 0 && msg_idx < model::message_count) {
        model::delete_message(msg_idx);
        sd_log::mark_dirty();
    }
    ui::screen_mgr::pop(true);
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Message", on_back);

    if (msg_idx < 0 || msg_idx >= model::message_count) return;

    auto& msg = model::messages[msg_idx];

    // Sender
    lv_obj_t* lbl_from = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_from, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_from, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_from, LV_ALIGN_TOP_LEFT, 15, 100);
    lv_label_set_text(lbl_from, msg.is_self ? "You" : msg.sender);

    // Time
    lv_obj_t* lbl_time = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_time, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_LEFT, 15, 140);
    lv_label_set_text_fmt(lbl_time, "%02d:%02d", msg.hour, msg.minute);

    // Message text
    lv_obj_t* lbl_text = lv_label_create(parent);
    lv_obj_set_width(lbl_text, lv_pct(90));
    lv_label_set_long_mode(lbl_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_text, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_text, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_text, LV_ALIGN_TOP_LEFT, 15, 180);
    lv_label_set_text(lbl_text, msg.text);

    // Delete button
    lv_obj_t* btn = ui::nav::text_button(parent, "Delete Message", on_delete, NULL);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void entry() {}
static void exit_fn() {}
static void destroy() {
    scr = NULL;
    msg_idx = -1;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::msg_detail
