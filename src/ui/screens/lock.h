#pragma once
#include "../ui_screen_mgr.h"
#include "../../model.h"
namespace ui::screen::lock {
extern screen_lifecycle_t lifecycle;
void show();
void update(uint32_t flags = model::DIRTY_CLOCK | model::DIRTY_BATTERY | model::DIRTY_MESH | model::DIRTY_SLEEP);
}
