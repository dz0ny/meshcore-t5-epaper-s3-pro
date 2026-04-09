#pragma once

#include "lvgl.h"

namespace ui::statusbar {

// Create the status bar at the top of a screen.
// Includes a self-updating timer — no need to call update() manually.
lv_obj_t* create(lv_obj_t* parent);

// Force an immediate update.
void update_now();
void show();
void hide();

} // namespace ui::statusbar
