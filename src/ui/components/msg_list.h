#pragma once

#include "lvgl.h"
#include <stdint.h>

namespace ui::msg_list {

// Create a scrollable message list inside parent.
lv_obj_t* create(lv_obj_t* parent);

// Append a message bubble to the list. msg_idx is the index in model::messages[].
// Caller should call scroll_to_bottom() after bulk appends to avoid per-append scrolling.
void append(lv_obj_t* list, const char* sender, const char* text, uint32_t timestamp, bool is_self, int msg_idx = -1);

// Clear all messages from the list.
void clear(lv_obj_t* list);

// Scroll the list so the newest message is visible. Call once after appends.
void scroll_to_bottom(lv_obj_t* list);

} // namespace ui::msg_list
