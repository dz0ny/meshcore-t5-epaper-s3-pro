#include "msg_list.h"
#include "../ui_theme.h"
#include "../../model.h"

namespace ui::msg_list {

lv_obj_t* create(lv_obj_t* parent) {
    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(list, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list, 4, LV_PART_MAIN);
    return list;
}

void append(lv_obj_t* list, const char* sender, const char* text, uint32_t timestamp, bool is_self) {
    // Wrapper container for bubble + time label
    lv_obj_t* wrapper = lv_obj_create(list);
    lv_obj_set_width(wrapper, lv_pct(100));
    lv_obj_set_height(wrapper, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(wrapper, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(wrapper, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(wrapper, 0, LV_PART_MAIN);
    lv_obj_clear_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(wrapper, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(wrapper, 2, LV_PART_MAIN);

    // Bubble
    lv_obj_t* bubble = lv_obj_create(wrapper);
    lv_obj_set_width(bubble, lv_pct(100));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(bubble, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(bubble, 8, LV_PART_MAIN);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(bubble, 2, LV_PART_MAIN);

    if (is_self) {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
        lv_obj_set_style_border_width(bubble, 0, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
        lv_obj_set_style_border_width(bubble, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(bubble, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
    }

    // Sender name
    if (sender && sender[0]) {
        lv_obj_t* lbl_name = lv_label_create(bubble);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_name,
            lv_color_hex(is_self ? EPD_COLOR_BG : EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(lbl_name, sender);
    }

    // Message text — bold for e-ink readability, montserrat for Unicode
    lv_obj_t* lbl_text = lv_label_create(bubble);
    lv_obj_set_width(lbl_text, lv_pct(100));
    lv_label_set_long_mode(lbl_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_text, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_text,
        lv_color_hex(is_self ? EPD_COLOR_BG : EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_text, text);

    // Time inside bubble, bottom-right — like WhatsApp
    lv_obj_t* lbl_time = lv_label_create(bubble);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time,
        lv_color_hex(is_self ? EPD_COLOR_BG : EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_time, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_width(lbl_time, lv_pct(100));
    lv_label_set_text_fmt(lbl_time, "%02d:%02d", model::clock.hour, model::clock.minute);

    // Auto-scroll to bottom (newest messages at bottom)
    lv_obj_scroll_to_y(list, LV_COORD_MAX, LV_ANIM_OFF);
}

void clear(lv_obj_t* list) {
    lv_obj_clean(list);
}

} // namespace ui::msg_list
