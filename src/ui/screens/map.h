#pragma once

#include "../ui_screen_mgr.h"

namespace ui::screen::map {

void process_events();
void toggle_fullscreen();
extern screen_lifecycle_t lifecycle;

} // namespace ui::screen::map
