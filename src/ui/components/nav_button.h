#pragma once

#include "lvgl.h"

namespace ui::nav {

// Create a back button with label at top-left. Calls cb on click.
lv_obj_t* back_button(lv_obj_t* parent, const char* title, lv_event_cb_t cb);

// Create a back button with a trailing action on the same row. Returns the action label.
lv_obj_t* back_button_action(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb,
                             const char* action_text, lv_event_cb_t action_cb, void* action_user_data);
lv_obj_t* back_button_action_ex(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb,
                                const char* action_text, lv_event_cb_t action_cb, void* action_user_data,
                                lv_obj_t** action_label_out);
lv_obj_t* back_button_actions_ex(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb,
                                 const char* first_action_text, lv_event_cb_t first_action_cb, void* first_action_user_data,
                                 const char* second_action_text, lv_event_cb_t second_action_cb, void* second_action_user_data,
                                 lv_obj_t** first_action_label_out, lv_obj_t** second_action_label_out);
lv_obj_t* back_button_three_actions_ex(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb,
                                       const char* first_action_text, lv_event_cb_t first_action_cb, void* first_action_user_data,
                                       const char* second_action_text, lv_event_cb_t second_action_cb, void* second_action_user_data,
                                       const char* third_action_text, lv_event_cb_t third_action_cb, void* third_action_user_data,
                                       lv_obj_t** first_action_label_out, lv_obj_t** second_action_label_out,
                                       lv_obj_t** third_action_label_out);

// Create a menu item row: icon + label, fills width.
lv_obj_t* menu_item(lv_obj_t* parent, const void* icon_src, const char* label, lv_event_cb_t cb, void* user_data);

// Create a setting toggle row: label on left, value on right. Returns the value label.
lv_obj_t* toggle_item(lv_obj_t* parent, const char* label, const char* value, lv_event_cb_t cb, void* user_data);

// Create a simple text button.
lv_obj_t* text_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data);

// Create the shared content area below the nav row.
lv_obj_t* content_area(lv_obj_t* parent);

// Create a scrollable list container — no elastic bounce, no momentum (e-ink friendly).
lv_obj_t* scroll_list(lv_obj_t* parent);
bool hit_area_debug_enabled();
void set_hit_area_debug(bool enabled);

} // namespace ui::nav
