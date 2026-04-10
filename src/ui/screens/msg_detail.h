#pragma once

#include "../ui_screen_mgr.h"

namespace ui::screen::msg_detail {

// Set the message index before pushing the screen.
void set_message(int idx);

extern screen_lifecycle_t lifecycle;

} // namespace ui::screen::msg_detail
