#include "chat.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/msg_list.h"
#include "../../model.h"
#include "../../mesh/mesh_bridge.h"

namespace ui::screen::chat {

static lv_obj_t* scr = NULL;
static lv_obj_t* msg_container = NULL;
static lv_timer_t* poll_timer = NULL;
static int last_displayed = 0;

static void on_back(lv_event_t* e) {
    ui::screen_mgr::pop(true);
}

// Check for new messages from model
static void poll_new(lv_timer_t* t) {
    // Also drain the bridge queue into model (in case BridgeUITask stored them)
    mesh::bridge::MessageIn mi;
    while (mesh::bridge::pop_message(mi)) {
        // already stored in model by BridgeUITask
    }

    // Rebuild if a message was deleted (indices shifted)
    if (last_displayed > model::message_count) {
        ui::msg_list::clear(msg_container);
        for (int i = 0; i < model::message_count; i++) {
            auto& msg = model::messages[i];
            ui::msg_list::append(msg_container, msg.sender, msg.text, 0, msg.is_self, i);
        }
        last_displayed = model::message_count;
        return;
    }

    // Display any new messages since last check
    while (last_displayed < model::message_count) {
        auto& msg = model::messages[last_displayed];
        ui::msg_list::append(msg_container, msg.sender, msg.text, 0, msg.is_self, last_displayed);
        last_displayed++;
    }
}

static void create(lv_obj_t* parent) {
    scr = parent;

    lv_obj_t* content = ui::nav::content_area(parent);
    msg_container = ui::msg_list::create(content);
    lv_obj_set_size(msg_container, lv_pct(100), lv_pct(100));
    lv_obj_center(msg_container);

    // Load all stored messages
    if (model::message_count == 0) {
        lv_obj_t* empty = lv_label_create(msg_container);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_flex_grow(empty, 1);
        lv_obj_set_style_text_font(empty, UI_FONT_TITLE, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_text(empty, "\n\n\nNo messages yet");
    } else {
        for (int i = 0; i < model::message_count; i++) {
            auto& msg = model::messages[i];
            ui::msg_list::append(msg_container, msg.sender, msg.text, 0, msg.is_self, i);
        }
    }
    last_displayed = model::message_count;

    // Clear unread on open
    model::clear_unread_messages();
}

static void entry() {
    poll_timer = lv_timer_create(poll_new, 2000, NULL);
}

static void exit_fn() {
    if (poll_timer) { lv_timer_del(poll_timer); poll_timer = NULL; }
}

static void destroy() {
    scr = NULL;
    msg_container = NULL;
    last_displayed = 0;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::chat
