#include "toast.h"
#include "../ui_theme.h"

namespace ui::toast {

static lv_obj_t* toast_obj = NULL;
static lv_timer_t* dismiss_timer = NULL;

static void on_dismiss(lv_timer_t* t) {
    if (toast_obj) {
        lv_obj_delete(toast_obj);
        toast_obj = NULL;
    }
    if (dismiss_timer) {
        lv_timer_delete(dismiss_timer);
        dismiss_timer = NULL;
    }
}

void show(const char* text, uint32_t timeout_ms) {
    // Remove any existing toast
    if (toast_obj) {
        lv_obj_delete(toast_obj);
        toast_obj = NULL;
    }
    if (dismiss_timer) {
        lv_timer_delete(dismiss_timer);
        dismiss_timer = NULL;
    }

    // Create on top layer so it overlays everything
    lv_obj_t* layer = lv_layer_top();

    toast_obj = lv_obj_create(layer);
    lv_obj_set_size(toast_obj, lv_pct(80), LV_SIZE_CONTENT);
    lv_obj_align(toast_obj, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(toast_obj, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_border_width(toast_obj, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(toast_obj, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(toast_obj, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(toast_obj, 20, LV_PART_MAIN);
    lv_obj_clear_flag(toast_obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(toast_obj);
    lv_obj_set_style_text_font(lbl, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);

    dismiss_timer = lv_timer_create(on_dismiss, timeout_ms, NULL);
    lv_timer_set_repeat_count(dismiss_timer, 1);
}

} // namespace ui::toast
