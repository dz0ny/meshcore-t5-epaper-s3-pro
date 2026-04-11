#include <cstring>
#include "compose.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/text_utils.h"
#include "../../model.h"
#include "../../sd_log.h"
#include "../../mesh/mesh_task.h"
#include "../../mesh/mesh_bridge.h"
#include <helpers/AdvertDataHelpers.h>

namespace ui::screen::compose {

static lv_obj_t* scr = NULL;
static lv_obj_t* recipient_list = NULL;
static lv_obj_t* lbl_to = NULL;
static lv_obj_t* ta = NULL;
static lv_obj_t* kb = NULL;
static lv_obj_t* send_btn = NULL;

static char recipient_name[32] = {};
static bool recipient_chosen = false;
static bool recipient_is_channel = false;

// Contact/channel list for picker
struct PickEntry {
    char name[32];
    bool is_channel;
};
static PickEntry pick_entries[MAX_CONTACTS + MAX_GROUP_CHANNELS + 1];  // contacts + channels + public
static int pick_count = 0;

void set_recipient(const char* name) {
    if (name) {
        strncpy(recipient_name, name, sizeof(recipient_name) - 1);
        recipient_name[31] = 0;
        recipient_chosen = true;
    } else {
        recipient_name[0] = 0;
        recipient_chosen = false;
    }
}

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void show_editor();

static void on_entry_pick(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < pick_count) {
        strncpy(recipient_name, pick_entries[idx].name, sizeof(recipient_name) - 1);
        recipient_is_channel = pick_entries[idx].is_channel;
        recipient_chosen = true;
        show_editor();
    }
}

static void on_send(lv_event_t* e) {
    const char* text = lv_textarea_get_text(ta);
    if (!text || !text[0] || !recipient_chosen) return;

    bool sent = recipient_is_channel
        ? mesh::task::send_public(text)
        : mesh::task::send_to_name(recipient_name, text);

    if (sent && model::message_count < MAX_STORED_MESSAGES) {
        auto& m = model::messages[model::message_count];
        strncpy(m.sender, mesh::task::node_name(), sizeof(m.sender) - 1);
        strncpy(m.text, text, sizeof(m.text) - 1);
        m.hour = model::clock.hour;
        m.minute = model::clock.minute;
        m.is_self = true;
        model::message_count++;
        sd_log::mark_dirty();
    }

    ui::screen_mgr::pop(true);
}

static void on_kb_event(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        on_send(e);
    } else if (code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_ta_focus(lv_event_t* e) {
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    model::touch_activity();
}

static void show_editor() {
    // Hide contact picker, show text input
    if (recipient_list) {
        lv_obj_add_flag(recipient_list, LV_OBJ_FLAG_HIDDEN);
    }

    // Update "To:" label
    if (lbl_to) {
        char buf[48];
        snprintf(buf, sizeof(buf), "To: %s", recipient_name);
        lv_label_set_text(lbl_to, buf);
    }

    if (ta) lv_obj_clear_flag(ta, LV_OBJ_FLAG_HIDDEN);
    if (send_btn) lv_obj_clear_flag(send_btn, LV_OBJ_FLAG_HIDDEN);

    // Focus textarea to show keyboard
    if (ta) lv_obj_send_event(ta, LV_EVENT_FOCUSED, NULL);
}

static void load_entries() {
    pick_count = 0;

    // Public channel is always first
    strncpy(pick_entries[pick_count].name, "Public", 31);
    pick_entries[pick_count].is_channel = true;
    pick_count++;

    // Then contacts — only chat nodes and rooms, not relays/sensors
    mesh::task::push_all_contacts();
    mesh::bridge::ContactUpdate cu;
    while (mesh::bridge::pop_contact(cu) && pick_count < 65) {
        if (cu.type == ADV_TYPE_REPEATER || cu.type == ADV_TYPE_SENSOR) continue;
        strncpy(pick_entries[pick_count].name, cu.name, 31);
        pick_entries[pick_count].name[31] = 0;
        pick_entries[pick_count].is_channel = false;
        ui::text::strip_emoji(pick_entries[pick_count].name);
        pick_count++;
    }
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Compose", on_back);

    // "To:" label
    lbl_to = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_to, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_to, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_to, LV_ALIGN_TOP_LEFT, 15, 130);

    if (recipient_chosen) {
        char buf[48];
        snprintf(buf, sizeof(buf), "To: %s", recipient_name);
        lv_label_set_text(lbl_to, buf);
    } else {
        lv_label_set_text(lbl_to, "Select recipient:");
    }

    // Contact picker list (shown when no recipient pre-selected)
    recipient_list = ui::nav::scroll_list(parent);
    lv_obj_set_size(recipient_list, lv_pct(95), lv_pct(80));
    lv_obj_align(recipient_list, LV_ALIGN_TOP_MID, 0, 170);

    if (!recipient_chosen) {
        load_entries();
        for (int i = 0; i < pick_count; i++) {
            ui::nav::menu_item(recipient_list, NULL, pick_entries[i].name,
                on_entry_pick, (void*)(intptr_t)i);
        }
    }

    // Textarea
    ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, lv_pct(90), 180);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 170);
    lv_textarea_set_placeholder_text(ta, "Type message...");
    lv_textarea_set_max_length(ta, 150);
    lv_textarea_set_one_line(ta, false);
    lv_obj_set_style_text_font(ta, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(ta, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ta, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_color(ta, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ta, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(ta, on_ta_focus, LV_EVENT_FOCUSED, NULL);

    // Keyboard
    kb = lv_keyboard_create(parent);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, lv_pct(100), 350);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(kb, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(kb, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, lv_color_hex(EPD_COLOR_BG), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_hex(EPD_COLOR_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_hex(EPD_COLOR_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_add_event_cb(kb, on_kb_event, LV_EVENT_ALL, NULL);

    // Send button
    send_btn = ui::nav::text_button(parent, "Send", on_send, NULL);
    lv_obj_align(send_btn, LV_ALIGN_BOTTOM_MID, 0, -370);

    if (recipient_chosen) {
        // Skip picker, go straight to editor
        lv_obj_add_flag(recipient_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(send_btn, LV_OBJ_FLAG_HIDDEN);  // hidden while kb is visible
    } else {
        // Show picker, hide editor
        lv_obj_add_flag(ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void entry() {}
static void exit_fn() {}
static void destroy() {
    scr = NULL;
    recipient_list = NULL;
    lbl_to = NULL;
    ta = NULL;
    kb = NULL;
    send_btn = NULL;
    recipient_name[0] = 0;
    recipient_chosen = false;
    recipient_is_channel = false;
    pick_count = 0;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::compose
