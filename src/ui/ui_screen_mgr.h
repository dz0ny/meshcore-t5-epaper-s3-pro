#pragma once

#include "lvgl.h"

// Screen lifecycle callbacks — each screen namespace provides one of these.
struct screen_lifecycle_t {
    void (*create)(lv_obj_t* parent);
    void (*entry)(void);
    void (*exit)(void);
    void (*destroy)(void);
};

namespace ui::screen_mgr {

void init();
bool register_screen(int id, screen_lifecycle_t* life);
bool switch_to(int id, bool anim);
bool push(int id, bool anim);
bool pop(bool anim);
int  top_id();
lv_obj_t* top_obj();

} // namespace ui::screen_mgr
