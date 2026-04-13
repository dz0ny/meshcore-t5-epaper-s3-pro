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
void reload_stack();
lv_obj_t* set_nav_action(const char* action_text, lv_event_cb_t action_cb, void* action_user_data);
lv_obj_t* set_nav_actions(const char* first_action_text, lv_event_cb_t first_action_cb, void* first_action_user_data,
                          const char* second_action_text, lv_event_cb_t second_action_cb, void* second_action_user_data);
void set_nav_title(const char* title);
const char* previous_nav_title(const char* fallback);
int  top_id();
lv_obj_t* top_obj();

} // namespace ui::screen_mgr
