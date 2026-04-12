#pragma once
#include "../ui_screen_mgr.h"
namespace ui::screen::lock {
extern screen_lifecycle_t lifecycle;
void show();
void update();
void update_time_date();
void update_unread();
void update_battery();
void update_node_name();
}
