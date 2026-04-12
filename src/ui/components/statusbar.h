#pragma once

#include "lvgl.h"

namespace ui::statusbar {

// Create the status bar on the top layer. Updates are triggered externally.
lv_obj_t* create();

// Force an immediate update.
void update_now();
void show();
void hide();

} // namespace ui::statusbar
