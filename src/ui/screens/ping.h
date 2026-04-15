#pragma once

#include "../ui_screen_mgr.h"
#include <stdint.h>

namespace ui::screen::ping {

void set_contact(const char* name, int32_t gps_lat, int32_t gps_lon, uint8_t type, bool has_path,
                 const uint8_t* pubkey_prefix);
void process_events();

extern screen_lifecycle_t lifecycle;

} // namespace ui::screen::ping
