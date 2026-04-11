#pragma once

#include "../ui_screen_mgr.h"

namespace ui::screen::compose {

// Pre-select a recipient by name before pushing. NULL = show picker.
void set_recipient(const char* name);

extern screen_lifecycle_t lifecycle;

} // namespace ui::screen::compose
