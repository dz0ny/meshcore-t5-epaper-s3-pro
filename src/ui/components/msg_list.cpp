#include "msg_list.h"
#include "text_utils.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../screens/msg_detail.h"
#include "../../model.h"

static void on_msg_click(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ui::screen::msg_detail::set_message(idx);
    ui::screen_mgr::push(SCREEN_MSG_DETAIL, true);
}

namespace ui::msg_list {

lv_obj_t* create(lv_obj_t* parent) {
    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(list, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(list, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list, 4, LV_PART_MAIN);
    // Disable elastic bounce and scroll momentum — bad on e-ink
    lv_obj_clear_flag(list, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM));
    return list;
}

void append(lv_obj_t* list, const char* sender, const char* text, uint32_t timestamp, bool is_self, int msg_idx) {
    // Strip emoji from sender and text (montserrat doesn't have emoji glyphs)
    char clean_sender[64] = {};
    char clean_text[256] = {};
    if (sender) { strncpy(clean_sender, sender, sizeof(clean_sender) - 1); ui::text::strip_emoji(clean_sender); }
    if (text) { strncpy(clean_text, text, sizeof(clean_text) - 1); ui::text::strip_emoji(clean_text); }

    // Wrapper container for bubble + time label
    lv_obj_t* wrapper = lv_obj_create(list);
    lv_obj_set_width(wrapper, lv_pct(100));
    lv_obj_set_height(wrapper, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(wrapper, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(wrapper, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(wrapper, 0, LV_PART_MAIN);
    lv_obj_clear_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
    if (msg_idx >= 0) {
        lv_obj_add_flag(wrapper, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(wrapper, on_msg_click, LV_EVENT_CLICKED, (void*)(intptr_t)msg_idx);
    }
    lv_obj_set_flex_flow(wrapper, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(wrapper, 2, LV_PART_MAIN);

    // Bubble
    lv_obj_t* bubble = lv_obj_create(wrapper);
    lv_obj_set_width(bubble, lv_pct(100));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(bubble, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(bubble, 8, LV_PART_MAIN);
    lv_obj_clear_flag(bubble, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(bubble, 2, LV_PART_MAIN);

    lv_obj_set_style_bg_color(bubble, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(bubble, UI_BORDER_THIN, LV_PART_MAIN);
    lv_obj_set_style_border_color(bubble, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);

    // Sent messages: indent from left to distinguish from received
    if (is_self) {
        lv_obj_set_style_pad_left(wrapper, 60, LV_PART_MAIN);
    }

    // Sender name
    if (clean_sender[0]) {
        lv_obj_t* lbl_name = lv_label_create(bubble);
        lv_obj_set_style_text_font(lbl_name, UI_FONT_SMALL, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_name, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(lbl_name, clean_sender);
    }

    // Message text
    lv_obj_t* lbl_text = lv_label_create(bubble);
    lv_obj_set_width(lbl_text, lv_pct(100));
    lv_label_set_long_mode(lbl_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_text, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_text, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_text, clean_text);

    // Time inside bubble, bottom-right
    lv_obj_t* lbl_time = lv_label_create(bubble);
    lv_obj_set_style_text_font(lbl_time, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_time, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_width(lbl_time, lv_pct(100));
    if (msg_idx >= 0 && msg_idx < model::message_count) {
        lv_label_set_text_fmt(lbl_time, "%02d:%02d",
            model::messages[msg_idx].hour, model::messages[msg_idx].minute);
    } else {
        lv_label_set_text_fmt(lbl_time, "%02d:%02d", model::clock.hour, model::clock.minute);
    }

    // Auto-scroll to bottom (newest messages at bottom)
    lv_obj_scroll_to_y(list, LV_COORD_MAX, LV_ANIM_OFF);
}

void clear(lv_obj_t* list) {
    lv_obj_clean(list);
}

} // namespace ui::msg_list
