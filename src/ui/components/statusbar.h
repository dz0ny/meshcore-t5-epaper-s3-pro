#pragma once

#include "lvgl.h"
#include "../../model.h"

namespace ui::statusbar {

// Create the status bar on the top layer. Updates are triggered externally.
lv_obj_t* create();

// Force an immediate update.
void update_now(uint32_t flags = model::DIRTY_CLOCK | model::DIRTY_BATTERY | model::DIRTY_GPS | model::DIRTY_MESH);
bool memory_enabled();
void set_memory_enabled(bool enabled);
void show();
void hide();

} // namespace ui::statusbar
