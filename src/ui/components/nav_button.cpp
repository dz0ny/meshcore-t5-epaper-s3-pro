#include "nav_button.h"
#include "../ui_theme.h"

namespace ui::nav {

lv_obj_t* back_button(lv_obj_t* parent, const char* title, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_height(btn, 50);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* arrow = lv_label_create(btn);
    lv_obj_align(arrow, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_color(arrow, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(arrow, " " LV_SYMBOL_LEFT);

    lv_obj_t* label = lv_label_create(parent);
    lv_obj_align_to(label, btn, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_text_font(label, &Font_Mono_Bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(label, title);
    lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(label, 30);

    return btn;
}

lv_obj_t* menu_item(lv_obj_t* parent, const void* icon_src, const char* label_text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 70);
    lv_obj_set_style_bg_color(cont, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 10, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cont, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (icon_src) {
        lv_obj_t* icon = lv_img_create(cont);
        lv_img_set_src(icon, icon_src);
    }

    lv_obj_t* lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(lbl, &Font_Mono_Bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl, label_text);

    lv_obj_t* arrow = lv_label_create(cont);
    lv_obj_set_flex_grow(arrow, 1);
    lv_obj_set_style_text_color(arrow, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    return cont;
}

lv_obj_t* text_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
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
