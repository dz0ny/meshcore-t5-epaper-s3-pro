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

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(parent, UI_MENU_ITEM_PAD, LV_PART_MAIN);
    lv_obj_t* content = parent;

    // Reuse the bubble component for the message
    lv_obj_t* bubble_list = ui::msg_list::create(content);
    lv_obj_set_width(bubble_list, lv_pct(100));
    lv_obj_set_flex_grow(bubble_list, 1);
    ui::msg_list::append(bubble_list, msg.sender, msg.text, 0, msg.is_self);

    lv_obj_t* actions = lv_obj_create(content);
    lv_obj_set_width(actions, lv_pct(100));
    lv_obj_set_height(actions, UI_TEXT_BTN_HEIGHT);
    lv_obj_set_style_bg_opa(actions, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(actions, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(actions, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(actions, UI_MENU_ITEM_PAD, LV_PART_MAIN);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* reply_btn = NULL;
    if (!msg.is_self) {
        reply_btn = ui::nav::text_button(actions, "Reply", on_reply, NULL);
        lv_obj_set_width(reply_btn, lv_pct(50));
        lv_obj_set_flex_grow(reply_btn, 1);
    }

    lv_obj_t* del_btn = ui::nav::text_button(actions, "Delete", on_delete, NULL);
    lv_obj_set_width(del_btn, lv_pct(msg.is_self ? 100 : 50));
    lv_obj_set_flex_grow(del_btn, 1);
}

static void entry() {}
static void exit_fn() {}
static void destroy() {
    scr = NULL;
    msg_idx = -1;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::msg_detail
