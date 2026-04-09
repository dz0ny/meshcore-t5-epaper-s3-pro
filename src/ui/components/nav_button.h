#pragma once

#include "lvgl.h"

namespace ui::nav {

// Create a back button with label at top-left. Calls cb on click.
lv_obj_t* back_button(lv_obj_t* parent, const char* title, lv_event_cb_t cb);

// Create a menu item row: icon + label, fills width.
lv_obj_t* menu_item(lv_obj_t* parent, const void* icon_src, const char* label, lv_event_cb_t cb, void* user_data);

// Create a simple text button.
lv_obj_t* text_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data);

} // namespace ui::nav
