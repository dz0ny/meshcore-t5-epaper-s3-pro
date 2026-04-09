#include "msg_list.h"
#include "../ui_theme.h"

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
    lv_obj_t* bubble = lv_obj_create(list);
    lv_obj_set_width(bubble, lv_pct(90));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(bubble, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(bubble, 8, LV_PART_MAIN);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(bubble, 2, LV_PART_MAIN);

    if (is_self) {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
        lv_obj_set_style_border_width(bubble, 0, LV_PART_MAIN);
        lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
    } else {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
        lv_obj_set_style_border_width(bubble, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(bubble, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
    }

    // Sender name — bold, compact
    if (sender && sender[0]) {
        lv_obj_t* lbl_name = lv_label_create(bubble);
        lv_obj_set_style_text_font(lbl_name, &Font_Mono_Bold_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_name,
            lv_color_hex(is_self ? EPD_COLOR_BG : EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(lbl_name, sender);
    }

    // Message text
    lv_obj_t* lbl_text = lv_label_create(bubble);
    lv_obj_set_width(lbl_text, lv_pct(100));
    lv_label_set_long_mode(lbl_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_text, &Font_Mono_Bold_25, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_text,
        lv_color_hex(is_self ? EPD_COLOR_BG : EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl_text, text);

    // Auto-scroll to bottom
    lv_obj_scroll_to_y(list, LV_COORD_MAX, LV_ANIM_OFF);
}

void clear(lv_obj_t* list) {
    lv_obj_clean(list);
}

} // namespace ui::msg_list
