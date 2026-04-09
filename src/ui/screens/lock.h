#pragma once
#include "../ui_screen_mgr.h"
namespace ui::screen::lock {
extern screen_lifecycle_t lifecycle;
void show();  // enter lock screen + trigger sleep
}
