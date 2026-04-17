#include "toast.h"
#include "../ui_theme.h"

namespace ui::toast {

static lv_obj_t* toast_obj = NULL;
static lv_obj_t* toast_label = NULL;
static lv_timer_t* dismiss_timer = NULL;

static void ensure_toast() {
    if (toast_obj) return;

    lv_obj_t* layer = lv_layer_top();

    toast_obj = lv_obj_create(layer);
    lv_obj_set_size(toast_obj, lv_pct(80), LV_SIZE_CONTENT);
    lv_obj_align(toast_obj, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(toast_obj, lv_color_hex(EPD_COLOR_PROMPT_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(toast_obj, UI_BORDER_CARD, LV_PART_MAIN);
    lv_obj_set_style_border_color(toast_obj, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_radius(toast_obj, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(toast_obj, 20, LV_PART_MAIN);
    lv_obj_clear_flag(toast_obj, LV_OBJ_FLAG_SCROLLABLE);

    toast_label = lv_label_create(toast_obj);
    lv_obj_set_style_text_font(toast_label, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(toast_label, lv_color_hex(EPD_COLOR_PROMPT_TXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(toast_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(toast_label, lv_pct(100));
    lv_obj_center(toast_label);
}

static void on_dismiss(lv_timer_t* t) {
    if (toast_obj) {
        lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (dismiss_timer) {
        lv_timer_delete(dismiss_timer);
        dismiss_timer = NULL;
    }
}

void show(const char* text, uint32_t timeout_ms) {
    ensure_toast();
    lv_label_set_text(toast_label, text);
    lv_obj_clear_flag(toast_obj, LV_OBJ_FLAG_HIDDEN);

    if (dismiss_timer) {
        lv_timer_reset(dismiss_timer);
        lv_timer_set_period(dismiss_timer, timeout_ms);
    } else {
        dismiss_timer = lv_timer_create(on_dismiss, timeout_ms, NULL);
        lv_timer_set_repeat_count(dismiss_timer, 1);
    }
}

void destroy() {
    if (dismiss_timer) {
        lv_timer_delete(dismiss_timer);
        dismiss_timer = NULL;
    }
    if (toast_obj) {
        lv_obj_delete(toast_obj);
        toast_obj = NULL;
        toast_label = NULL;
    }
}

} // namespace ui::toast
