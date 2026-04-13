#include "msg_detail.h"
#include "compose.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/msg_list.h"
#include "../../model.h"
#include "../../sd_log.h"

namespace ui::screen::msg_detail {

static lv_obj_t* scr = NULL;
static int msg_idx = -1;

void set_message(int idx) {
    msg_idx = idx;
}

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_delete(lv_event_t* e) {
    if (msg_idx >= 0 && msg_idx < model::message_count) {
        model::delete_message(msg_idx);
        sd_log::mark_dirty();
    }
    ui::screen_mgr::pop(true);
}

static void on_reply(lv_event_t* e) {
    if (msg_idx >= 0 && msg_idx < model::message_count) {
        ui::screen::compose::set_recipient(model::messages[msg_idx].sender);
        ui::screen_mgr::push(SCREEN_COMPOSE, true);
    }
}

static void create(lv_obj_t* parent) {
    scr = parent;

    if (msg_idx < 0 || msg_idx >= model::message_count) return;

    auto& msg = model::messages[msg_idx];

    // Reuse the bubble component for the message
    lv_obj_t* bubble_list = ui::msg_list::create(parent);
    lv_obj_set_size(bubble_list, lv_pct(95), lv_pct(45));
    lv_obj_align(bubble_list, LV_ALIGN_TOP_MID, 0, UI_BACK_BTN_Y + UI_BACK_BTN_HEIGHT);
    ui::msg_list::append(bubble_list, msg.sender, msg.text, 0, msg.is_self);

    // Reply button (only for received messages)
    if (!msg.is_self) {
        lv_obj_t* reply_btn = ui::nav::text_button(parent, "Reply", on_reply, NULL);
        lv_obj_align(reply_btn, LV_ALIGN_BOTTOM_MID, 0, -110);
    }

    // Delete button
    lv_obj_t* del_btn = ui::nav::text_button(parent, "Delete", on_delete, NULL);
    lv_obj_align(del_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void entry() {}
static void exit_fn() {}
static void destroy() {
    scr = NULL;
    msg_idx = -1;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::msg_detail
