#include <cstring>
#include "compose.h"
#include "../ui_theme.h"
#include "../ui_port.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/text_utils.h"
#include "../../model.h"
#include "../../sd_log.h"
#include "../../mesh/mesh_task.h"
#include "../../mesh/mesh_bridge.h"
#include <helpers/AdvertDataHelpers.h>

namespace ui::screen::compose {

// Layout Y positions — board-conditional
#if defined(BOARD_TDECK)
static constexpr int COMPOSE_RECIPIENT_Y = 60;
static constexpr int COMPOSE_RECIPIENT_H = 55;
static constexpr int COMPOSE_LIST_Y = 120;
static constexpr int COMPOSE_LIST_H = 115;
static constexpr int COMPOSE_EDITOR_Y = 120;
static constexpr int COMPOSE_EDITOR_H = 45;
static constexpr int COMPOSE_TA_Y = 125;
static constexpr int COMPOSE_TA_H = 70;
static constexpr int COMPOSE_SEND_BOTTOM = -5;
#else
static constexpr int COMPOSE_RECIPIENT_Y = 130;
static constexpr int COMPOSE_RECIPIENT_H = 110;
static constexpr int COMPOSE_LIST_Y = 255;
static constexpr int COMPOSE_LIST_H = 630;
static constexpr int COMPOSE_EDITOR_Y = 255;
static constexpr int COMPOSE_EDITOR_H = 260;
static constexpr int COMPOSE_TA_Y = 305;
static constexpr int COMPOSE_TA_H = 180;
static constexpr int COMPOSE_SEND_BOTTOM = -335;
#endif

static lv_obj_t* scr = NULL;
static lv_obj_t* recipient_list = NULL;
static lv_obj_t* recipient_card = NULL;
static lv_obj_t* lbl_to = NULL;
static lv_obj_t* lbl_hint = NULL;
static lv_obj_t* ta = NULL;
static lv_obj_t* kb = NULL;
static lv_obj_t* send_btn = NULL;
static lv_obj_t* editor_card = NULL;
static lv_obj_t* char_count = NULL;
static int saved_refresh_mode = UI_REFRESH_MODE_NORMAL;
static bool refresh_mode_overridden = false;

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
        recipient_is_channel = false;
    } else {
        recipient_name[0] = 0;
        recipient_chosen = false;
        recipient_is_channel = false;
    }
}

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void show_editor();
static void show_picker();

static void enable_typing_refresh_mode() {
    if (!refresh_mode_overridden) {
        saved_refresh_mode = ui::port::get_refresh_mode();
        refresh_mode_overridden = true;
    }
    ui::port::set_refresh_mode(UI_REFRESH_MODE_FAST);
}

static void restore_refresh_mode() {
    if (!refresh_mode_overridden) return;
    ui::port::set_refresh_mode(saved_refresh_mode);
    refresh_mode_overridden = false;
}

static void update_recipient_card() {
    if (!lbl_to || !lbl_hint) return;

    if (recipient_chosen) {
        lv_label_set_text(lbl_to, recipient_name);
    } else {
        lv_label_set_text(lbl_to, "Choose");
    }
    lv_label_set_text(lbl_hint, "");
}

static void update_char_count() {
    if (!char_count || !ta) return;

    const char* text = lv_textarea_get_text(ta);
    size_t len = text ? strlen(text) : 0;
    lv_label_set_text_fmt(char_count, "%d/150", (int)len);
}

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

    ui::screen_mgr::switch_to(SCREEN_CHAT, true);
}

static void on_kb_event(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        on_send(e);
    }
}


static void on_ta_focus(lv_event_t* e) {
    enable_typing_refresh_mode();
    if (kb) lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    if (send_btn) lv_obj_clear_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    model::touch_activity();
}

static void on_ta_blur(lv_event_t* e) {
    restore_refresh_mode();
}

static void on_ta_change(lv_event_t* e) {
    update_char_count();
    model::touch_activity();
}

static void on_recipient_click(lv_event_t* e) {
    if (recipient_chosen) {
        show_picker();
    }
}

static void show_editor() {
    update_recipient_card();

    if (recipient_list) lv_obj_add_flag(recipient_list, LV_OBJ_FLAG_HIDDEN);
    if (editor_card) lv_obj_clear_flag(editor_card, LV_OBJ_FLAG_HIDDEN);
    if (send_btn) lv_obj_clear_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    if (ta) lv_obj_clear_flag(ta, LV_OBJ_FLAG_HIDDEN);
    update_char_count();

    if (ta) lv_obj_send_event(ta, LV_EVENT_FOCUSED, NULL);
}

static void show_picker() {
    recipient_chosen = false;
    recipient_is_channel = false;
    recipient_name[0] = 0;
    update_recipient_card();

    if (recipient_list) lv_obj_clear_flag(recipient_list, LV_OBJ_FLAG_HIDDEN);
    if (editor_card) lv_obj_add_flag(editor_card, LV_OBJ_FLAG_HIDDEN);
    if (ta) lv_obj_add_flag(ta, LV_OBJ_FLAG_HIDDEN);
    if (send_btn) lv_obj_add_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    if (kb) lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
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

    recipient_card = lv_obj_create(parent);
    lv_obj_set_size(recipient_card, lv_pct(95), COMPOSE_RECIPIENT_H);
    lv_obj_align(recipient_card, LV_ALIGN_TOP_MID, 0, COMPOSE_RECIPIENT_Y);
    lv_obj_set_style_bg_color(recipient_card, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(recipient_card, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(recipient_card, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(recipient_card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(recipient_card, 16, LV_PART_MAIN);
    lv_obj_clear_flag(recipient_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(recipient_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(recipient_card, on_recipient_click, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(recipient_card, 15);

    lv_obj_t* recipient_title = lv_label_create(recipient_card);
    lv_obj_set_style_text_font(recipient_title, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(recipient_title, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(recipient_title, "Recipient");
    lv_obj_align(recipient_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* recipient_arrow = lv_label_create(recipient_card);
    lv_obj_set_style_text_font(recipient_arrow, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(recipient_arrow, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(recipient_arrow, LV_SYMBOL_RIGHT);
    lv_obj_align(recipient_arrow, LV_ALIGN_TOP_RIGHT, 0, 0);

    lbl_to = lv_label_create(recipient_card);
    lv_obj_set_width(lbl_to, lv_pct(100));
    lv_obj_set_style_text_font(lbl_to, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_to, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_long_mode(lbl_to, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl_to, LV_ALIGN_TOP_LEFT, 0, 30);

    lbl_hint = lv_label_create(recipient_card);
    lv_obj_set_width(lbl_hint, lv_pct(92));
    lv_obj_set_style_text_font(lbl_hint, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_long_mode(lbl_hint, LV_LABEL_LONG_DOT);
    lv_obj_add_flag(lbl_hint, LV_OBJ_FLAG_HIDDEN);
    update_recipient_card();

    recipient_list = ui::nav::scroll_list(parent);
    lv_obj_set_size(recipient_list, lv_pct(95), COMPOSE_LIST_H);
    lv_obj_align(recipient_list, LV_ALIGN_TOP_MID, 0, COMPOSE_LIST_Y);

    load_entries();
    if (pick_count == 0) {
        lv_obj_t* empty = lv_label_create(recipient_list);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_flex_grow(empty, 1);
        lv_obj_set_style_text_font(empty, UI_FONT_TITLE, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_text(empty, "\n\nNo contacts yet");
    } else {
        for (int i = 0; i < pick_count; i++) {
            ui::nav::menu_item(recipient_list, NULL, pick_entries[i].name,
                on_entry_pick, (void*)(intptr_t)i);
        }
    }

    editor_card = lv_obj_create(parent);
    lv_obj_set_size(editor_card, lv_pct(95), COMPOSE_EDITOR_H);
    lv_obj_align(editor_card, LV_ALIGN_TOP_MID, 0, COMPOSE_EDITOR_Y);
    lv_obj_set_style_bg_color(editor_card, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(editor_card, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(editor_card, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(editor_card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(editor_card, 16, LV_PART_MAIN);
    lv_obj_clear_flag(editor_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* msg_title = lv_label_create(editor_card);
    lv_obj_set_style_text_font(msg_title, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(msg_title, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(msg_title, "Message");
    lv_obj_align(msg_title, LV_ALIGN_TOP_LEFT, 0, 0);

    char_count = lv_label_create(editor_card);
    lv_obj_set_style_text_font(char_count, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(char_count, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(char_count, "0/150");
    lv_obj_align(char_count, LV_ALIGN_TOP_RIGHT, 0, 0);

    ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, lv_pct(89), COMPOSE_TA_H);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, COMPOSE_TA_Y);
    lv_textarea_set_placeholder_text(ta, "Write a short message");
    lv_textarea_set_max_length(ta, 150);
    lv_textarea_set_one_line(ta, false);
    lv_obj_set_style_text_font(ta, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(ta, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ta, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_color(ta, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(ta, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ta, 12, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(ta, 0, LV_PART_CURSOR);
    lv_obj_add_event_cb(ta, on_ta_focus, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta, on_ta_blur, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta, on_ta_change, LV_EVENT_VALUE_CHANGED, NULL);
    update_char_count();

#ifndef BOARD_TDECK
    // On-screen keyboard — not needed on T-Deck which has a physical keyboard
    kb = lv_keyboard_create(parent);
    lv_keyboard_set_textarea(kb, ta);
    lv_buttonmatrix_clear_button_ctrl_all(kb, LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_button_ctrl_all(kb, LV_BUTTONMATRIX_CTRL_NO_REPEAT);
    lv_obj_set_size(kb, lv_pct(100), 320);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(kb, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(kb, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_anim_duration(kb, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, lv_color_hex(EPD_COLOR_BG), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_hex(EPD_COLOR_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_hex(EPD_COLOR_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 2, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 10, LV_PART_ITEMS);
    lv_obj_set_style_anim_duration(kb, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(kb, on_kb_event, LV_EVENT_ALL, NULL);
#endif

    send_btn = ui::nav::text_button(parent, "Send", on_send, NULL);
    lv_obj_align(send_btn, LV_ALIGN_BOTTOM_MID, 0, COMPOSE_SEND_BOTTOM);

    if (recipient_chosen) {
        lv_obj_add_flag(recipient_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(editor_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(editor_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void entry() {
    if (recipient_chosen) {
        enable_typing_refresh_mode();
    }
}

static void exit_fn() {
    restore_refresh_mode();
}

static void destroy() {
    restore_refresh_mode();
    scr = NULL;
    recipient_list = NULL;
    recipient_card = NULL;
    lbl_to = NULL;
    lbl_hint = NULL;
    ta = NULL;
    kb = NULL;
    send_btn = NULL;
    editor_card = NULL;
    char_count = NULL;
    saved_refresh_mode = UI_REFRESH_MODE_NORMAL;
    refresh_mode_overridden = false;
    recipient_name[0] = 0;
    recipient_chosen = false;
    recipient_is_channel = false;
    pick_count = 0;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::compose
