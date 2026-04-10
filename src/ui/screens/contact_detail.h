#pragma once

#include "../ui_screen_mgr.h"
#include <stdint.h>

namespace ui::screen::contact_detail {

// Set contact data before pushing the screen.
// pubkey_prefix may be NULL (existing contact from contacts screen).
void set_contact(const char* name, int32_t gps_lat, int32_t gps_lon, uint8_t type, bool has_path,
                 const uint8_t* pubkey_prefix = nullptr);

extern screen_lifecycle_t lifecycle;

} // namespace ui::screen::contact_detail
