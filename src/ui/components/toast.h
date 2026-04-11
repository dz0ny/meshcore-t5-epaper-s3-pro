#pragma once

#include "lvgl.h"

namespace ui::toast {

// Show a toast message overlay. Auto-disappears after timeout_ms.
void show(const char* text, uint32_t timeout_ms = 2000);

} // namespace ui::toast
