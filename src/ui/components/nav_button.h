#pragma once

#include "lvgl.h"

namespace ui::nav {

// Create a back button with label at top-left. Calls cb on click.
lv_obj_t* back_button(lv_obj_t* parent, const char* title, lv_event_cb_t cb);

// Create a back button with a trailing action on the same row. Returns the action label.
lv_obj_t* back_button_action(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb,
                             const char* action_text, lv_event_cb_t action_cb, void* action_user_data);

// Create a menu item row: icon + label, fills width.
lv_obj_t* menu_item(lv_obj_t* parent, const void* icon_src, const char* label, lv_event_cb_t cb, void* user_data);

// Create a setting toggle row: label on left, value on right. Returns the value label.
lv_obj_t* toggle_item(lv_obj_t* parent, const char* label, const char* value, lv_event_cb_t cb, void* user_data);

// Create a simple text button.
lv_obj_t* text_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data);

// Create a scrollable list container — no elastic bounce, no momentum (e-ink friendly).
lv_obj_t* scroll_list(lv_obj_t* parent);

} // namespace ui::nav
