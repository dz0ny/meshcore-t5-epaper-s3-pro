#include "nav_button.h"
#include "../ui_screen_mgr.h"
#include "../ui_theme.h"

namespace ui::nav {

static void create_back_content(lv_obj_t* parent, const char* title) {
    lv_obj_t* arrow = lv_label_create(parent);
    lv_obj_set_style_text_font(arrow, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(arrow, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);

    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(label, title);
}

lv_obj_t* back_button(lv_obj_t* parent, const char* title, lv_event_cb_t cb) {
    ui::screen_mgr::set_nav_title(title);

    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, UI_BACK_BTN_Y);
    lv_obj_set_size(row, lv_pct(95), UI_BACK_BTN_HEIGHT);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(row, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(row, UI_BACK_BTN_PAD_V, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, UI_BACK_BTN_COL_PAD, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(row, UI_EXT_CLICK_BACK);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_back_content(row, ui::screen_mgr::previous_nav_title(title));

    return row;
}

lv_obj_t* back_button_action(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb,
                             const char* action_text, lv_event_cb_t action_cb, void* action_user_data) {
    ui::screen_mgr::set_nav_title(title);

    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, UI_BACK_BTN_Y);
    lv_obj_set_size(row, lv_pct(95), UI_BACK_BTN_HEIGHT);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);

    lv_obj_t* back = lv_obj_create(row);
    lv_obj_set_size(back, lv_pct(60), lv_pct(100));
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(back, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(back, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_right(back, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(back, UI_BACK_BTN_PAD_V, LV_PART_MAIN);
    lv_obj_set_style_pad_column(back, UI_BACK_BTN_COL_PAD, LV_PART_MAIN);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(back, UI_EXT_CLICK_BACK);
    lv_obj_set_flex_flow(back, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(back, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_back_content(back, ui::screen_mgr::previous_nav_title(title));

    lv_obj_t* action = lv_obj_create(row);
    lv_obj_set_size(action, LV_SIZE_CONTENT, UI_ACTION_BTN_H);
    lv_obj_align(action, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(action, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(action, UI_ACTION_BTN_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_color(action, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(action, UI_ACTION_BTN_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(action, UI_ACTION_BTN_PAD_H, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(action, UI_ACTION_BTN_PAD_V, LV_PART_MAIN);
    lv_obj_clear_flag(action, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(action, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(action, action_cb, LV_EVENT_CLICKED, action_user_data);
    lv_obj_set_ext_click_area(action, UI_EXT_CLICK_ACTION);

    lv_obj_t* label = lv_label_create(action);
    lv_obj_set_style_text_font(label, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(label, action_text);
    lv_obj_center(label);

    return label;
}

lv_obj_t* menu_item(lv_obj_t* parent, const void* icon_src, const char* label_text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), UI_MENU_ITEM_HEIGHT);
    lv_obj_set_style_bg_color(cont, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, UI_MENU_ITEM_PAD, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cont, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_set_ext_click_area(cont, UI_EXT_CLICK_LIST);

    (void)icon_src;  // unused — all callers pass NULL

    lv_obj_t* lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(lbl, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl, label_text);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_MENU_ITEM_INSET, 0);

    lv_obj_t* arrow = lv_label_create(cont);
    lv_obj_set_style_text_font(arrow, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(arrow, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -UI_MENU_ITEM_INSET, 0);

    return cont;
}

lv_obj_t* toggle_item(lv_obj_t* parent, const char* label_text, const char* value, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), UI_MENU_ITEM_HEIGHT);
    lv_obj_set_style_bg_color(cont, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, UI_MENU_ITEM_PAD, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cont, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_set_ext_click_area(cont, UI_EXT_CLICK_LIST);

    lv_obj_t* lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(lbl, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(lbl, label_text);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_MENU_ITEM_INSET, 0);

    lv_obj_t* val = lv_label_create(cont);
    lv_obj_set_style_text_font(val, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(val, value);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -UI_MENU_ITEM_INSET, 0);

    return val;  // return the value label so caller can update it
}

lv_obj_t* text_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, lv_pct(85), UI_TEXT_BTN_HEIGHT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, UI_TEXT_BTN_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, UI_TEXT_BTN_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, UI_TEXT_BTN_PAD, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_set_ext_click_area(btn, UI_EXT_CLICK_ACTION);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);

    return btn;
}

lv_obj_t* scroll_list(lv_obj_t* parent) {
    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(95), lv_pct(UI_SCROLL_LIST_H));
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, UI_SCROLL_LIST_Y);
    lv_obj_set_style_bg_opa(list, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    // Disable elastic bounce and scroll momentum — bad on e-ink
    lv_obj_clear_flag(list, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM));
    return list;
}

} // namespace ui::nav
