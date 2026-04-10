#include "nav_button.h"
#include "../ui_theme.h"

namespace ui::nav {

lv_obj_t* back_button(lv_obj_t* parent, const char* title, lv_event_cb_t cb) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 5, 50);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(row, 20);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* arrow = lv_label_create(row);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(arrow, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);

    lv_obj_t* label = lv_label_create(row);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_ext_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(label, title);

    return row;
}

lv_obj_t* menu_item(lv_obj_t* parent, const void* icon_src, const char* label_text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 85);
    lv_obj_set_style_bg_color(cont, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 15, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cont, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_set_ext_click_area(cont, 10);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (icon_src) {
        lv_obj_t* icon = lv_image_create(cont);
        lv_image_set_src(icon, icon_src);
    }

    lv_obj_t* lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_ext_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl, label_text);

    lv_obj_t* arrow = lv_label_create(cont);
    lv_obj_set_flex_grow(arrow, 1);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(arrow, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    return cont;
}

lv_obj_t* toggle_item(lv_obj_t* parent, const char* label_text, const char* value, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 85);
    lv_obj_set_style_bg_color(cont, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 15, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cont, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_set_ext_click_area(cont, 10);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_ext_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl, label_text);

    lv_obj_t* val = lv_label_create(cont);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_ext_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(val, value);

    return val;  // return the value label so caller can update it
}

lv_obj_t* text_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_style_bg_color(btn, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);

    return btn;
}

} // namespace ui::nav
