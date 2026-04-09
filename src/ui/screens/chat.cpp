#include "chat.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/msg_list.h"
#include "../../mesh/mesh_bridge.h"

namespace ui::screen::chat {

static lv_obj_t* scr = NULL;
static lv_obj_t* msg_container = NULL;
static lv_timer_t* poll_timer = NULL;

// ---------- Handlers ----------

static void on_back(lv_event_t* e) {
    ui::screen_mgr::pop(true);
}

// ---------- Poll for new messages ----------

static void poll_messages(lv_timer_t* t) {
    mesh::bridge::MessageIn mi;
    while (mesh::bridge::pop_message(mi)) {
        const char* sender = mi.is_channel ? "" : mi.sender_name;
        ui::msg_list::append(msg_container, sender, mi.text, mi.timestamp, false);
    }
}

// ---------- Lifecycle ----------

static void create(lv_obj_t* parent) {
    scr = parent;

    ui::nav::back_button(parent, "Messages", on_back);

    msg_container = ui::msg_list::create(parent);
    lv_obj_set_size(msg_container, lv_pct(95), lv_pct(85));
    lv_obj_align(msg_container, LV_ALIGN_BOTTOM_MID, 0, -10);
}

static void entry() {
    poll_timer = lv_timer_create(poll_messages, 1000, NULL);
    poll_messages(NULL);
}

static void exit_fn() {
    if (poll_timer) {
        lv_timer_del(poll_timer);
        poll_timer = NULL;
    }
}

static void destroy() {
    scr = NULL;
    msg_container = NULL;
}

screen_lifecycle_t lifecycle = {
    .create  = create,
    .entry   = entry,
    .exit    = exit_fn,
    .destroy = destroy,
};

} // namespace ui::screen::chat
