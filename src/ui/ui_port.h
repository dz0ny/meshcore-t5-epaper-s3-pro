#pragma once

#include "lvgl.h"

// E-paper refresh modes
#define UI_REFRESH_MODE_NORMAL 0
#define UI_REFRESH_MODE_FAST   1

namespace ui::port {

// Initialize LVGL display driver and touch input for the e-paper.
void init();

// Refresh mode control
void set_refresh_mode(int mode);
int  get_refresh_mode();

// Full display operations
void full_refresh();
void full_clean();

// Touch enable/disable
void touch_enable();
void touch_disable();

// Keyboard/encoder focus: add widget to default group for encoder/keypad navigation
static inline void keyboard_focus_invalidate() {}
static inline void keyboard_focus_register(lv_obj_t* obj) {
    lv_group_t* g = lv_group_get_default();
    if (g && obj) {
        lv_group_add_obj(g, obj);
    }
}

// Group-level focus callback: scroll the newly focused object into view so
// encoder/keypad navigation never leaves the selection off-screen. Install with
// lv_group_set_focus_cb() once per group.
static inline void keyboard_focus_group_cb(lv_group_t* g) {
    if (!g) return;
    lv_obj_t* obj = lv_group_get_focused(g);
    if (!obj || !lv_obj_is_valid(obj) || !lv_obj_is_visible(obj)) return;
    lv_obj_scroll_to_view_recursive(obj, LV_ANIM_OFF);
}

// Backlight mode: 0=Auto, 1=Off
void set_backlight(int mode);
int  get_backlight();
const char* get_backlight_name();
bool is_backlight_auto();

// Brightness: 0=Low, 1=Mid, 2=High (intensity when backlight is on)
void set_brightness(int level);
int  get_brightness();
const char* get_brightness_name();
int  get_brightness_pwm();
void apply_backlight();  // turn on at current brightness

} // namespace ui::port
