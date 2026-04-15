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

// Mark the keyboard focus tree dirty so it is rebuilt on the next key read.
void keyboard_focus_invalidate();
void keyboard_focus_register(lv_obj_t* obj);

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
