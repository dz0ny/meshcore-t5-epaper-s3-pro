#pragma once

#include "lvgl.h"

// E-paper refresh modes
#define UI_REFRESH_MODE_NORMAL 0
#define UI_REFRESH_MODE_FAST   1
#define UI_REFRESH_MODE_NEAT   2

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

// Backlight: 4 levels (0=Off, 1=Low, 2=Mid, 3=High)
void set_backlight(int level);
int  get_backlight();
const char* get_backlight_name();

} // namespace ui::port
