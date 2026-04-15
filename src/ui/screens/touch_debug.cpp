#include "touch_debug.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../board.h"
#include <cstdio>

namespace ui::screen::touch_debug {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_coords = NULL;
static lv_obj_t* canvas = NULL;
static lv_obj_t* crosshair_h = NULL;
static lv_obj_t* crosshair_v = NULL;
static lv_obj_t* dot = NULL;
static lv_timer_t* timer = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void draw_crosshair(int16_t x, int16_t y) {
    if (!crosshair_h || !crosshair_v || !dot) return;

    // Horizontal line
    lv_obj_set_pos(crosshair_h, 0, y);
    lv_obj_clear_flag(crosshair_h, LV_OBJ_FLAG_HIDDEN);

    // Vertical line
    lv_obj_set_pos(crosshair_v, x, 0);
    lv_obj_clear_flag(crosshair_v, LV_OBJ_FLAG_HIDDEN);

    // Dot at intersection
    lv_obj_set_pos(dot, x - 4, y - 4);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
}

static void hide_crosshair() {
    if (crosshair_h) lv_obj_add_flag(crosshair_h, LV_OBJ_FLAG_HIDDEN);
    if (crosshair_v) lv_obj_add_flag(crosshair_v, LV_OBJ_FLAG_HIDDEN);
    if (dot) lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
}

static void poll_touch(lv_timer_t* t) {
    (void)t;
    if (!lbl_coords) return;

    int16_t x = 0, y = 0;
    if (board::touch.isPressed() && board::touch.getPoint(&x, &y, 1)) {
        char buf[48];
        snprintf(buf, sizeof(buf), "x=%d  y=%d", x, y);
        lv_label_set_text(lbl_coords, buf);
        draw_crosshair(x, y);
    } else {
        lv_label_set_text(lbl_coords, "Touch the screen");
        hide_crosshair();
    }
}

static void create(lv_obj_t* parent) {
    scr = parent;

    // Draw area — full screen overlay below nav
    canvas = lv_obj_create(parent);
    lv_obj_set_size(canvas, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(canvas, 0, 0);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(canvas, 0, LV_PART_MAIN);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

    // Grid lines for reference (every 40px)
    for (int gx = 40; gx < lv_display_get_horizontal_resolution(lv_display_get_default()); gx += 40) {
        lv_obj_t* line = lv_obj_create(canvas);
        lv_obj_set_size(line, 1, lv_display_get_vertical_resolution(lv_display_get_default()));
        lv_obj_set_pos(line, gx, 0);
        lv_obj_set_style_bg_color(line, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line, LV_OPA_30, LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_clear_flag(line, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    }
    for (int gy = 40; gy < lv_display_get_vertical_resolution(lv_display_get_default()); gy += 40) {
        lv_obj_t* line = lv_obj_create(canvas);
        lv_obj_set_size(line, lv_display_get_horizontal_resolution(lv_display_get_default()), 1);
        lv_obj_set_pos(line, 0, gy);
        lv_obj_set_style_bg_color(line, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line, LV_OPA_30, LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_clear_flag(line, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    }

    // Crosshair lines (hidden initially)
    crosshair_h = lv_obj_create(canvas);
    lv_obj_set_size(crosshair_h, lv_display_get_horizontal_resolution(lv_display_get_default()), 1);
    lv_obj_set_style_bg_color(crosshair_h, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_width(crosshair_h, 0, LV_PART_MAIN);
    lv_obj_clear_flag(crosshair_h, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_flag(crosshair_h, LV_OBJ_FLAG_HIDDEN);

    crosshair_v = lv_obj_create(canvas);
    lv_obj_set_size(crosshair_v, 1, lv_display_get_vertical_resolution(lv_display_get_default()));
    lv_obj_set_style_bg_color(crosshair_v, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_width(crosshair_v, 0, LV_PART_MAIN);
    lv_obj_clear_flag(crosshair_v, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_flag(crosshair_v, LV_OBJ_FLAG_HIDDEN);

    dot = lv_obj_create(canvas);
    lv_obj_set_size(dot, 9, 9);
    lv_obj_set_style_bg_color(dot, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dot, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);

    // Coordinate label at top
    lbl_coords = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_coords, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_coords, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(lbl_coords, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl_coords, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lbl_coords, 4, LV_PART_MAIN);
    lv_label_set_text(lbl_coords, "Touch the screen");
    lv_obj_align(lbl_coords, LV_ALIGN_TOP_MID, 0, 40);

    // Back button on top
    ui::nav::back_button(parent, "Touch Debug", on_back);
}

static void entry() {
    timer = lv_timer_create(poll_touch, 30, NULL);
}

static void exit_fn() {
    if (timer) { lv_timer_delete(timer); timer = NULL; }
}

static void destroy() {
    scr = NULL; lbl_coords = NULL; canvas = NULL;
    crosshair_h = NULL; crosshair_v = NULL; dot = NULL;
    timer = NULL;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::touch_debug
