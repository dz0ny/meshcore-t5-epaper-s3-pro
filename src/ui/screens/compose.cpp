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

// Layout from per-device layout header

static lv_obj_t* scr = NULL;
static lv_obj_t* recipient_list = NULL;
static lv_obj_t* recipient_list_area = NULL;
static lv_obj_t* recipient_card = NULL;
static lv_obj_t* lbl_to = NULL;
static lv_obj_t* lbl_hint = NULL;
static lv_obj_t* ta = NULL;
static lv_obj_t* kb = NULL;
static lv_obj_t* send_btn = NULL;
static lv_obj_t* editor_card = NULL;
static lv_obj_t* char_count = NULL;
static lv_obj_t* lbl_filter_fav = NULL;
static lv_obj_t* lbl_filter_channels = NULL;
static lv_obj_t* lbl_filter_people = NULL;
static int saved_refresh_mode = UI_REFRESH_MODE_NORMAL;
static bool refresh_mode_overridden = false;

static char recipient_name[32] = {};
static bool recipient_chosen = false;
static bool recipient_is_channel = false;
static uint8_t recipient_channel_idx = 0;

enum PickFilter {
    PICK_FILTER_FAVORITES = 0,
    PICK_FILTER_CHANNELS = 1,
    PICK_FILTER_PEOPLE = 2,
};

static PickFilter current_filter = PICK_FILTER_PEOPLE;

// Contact/channel list for picker
struct PickEntry {
    char name[32];
    bool is_channel;
    uint8_t channel_idx;
    uint8_t flags;
};
static PickEntry pick_entries[MAX_CONTACTS + MAX_GROUP_CHANNELS];
static int pick_count = 0;

void set_recipient(const char* name) {
    if (name) {
        strncpy(recipient_name, name, sizeof(recipient_name) - 1);
        recipient_name[31] = 0;
        recipient_chosen = true;
        recipient_is_channel = false;
        recipient_channel_idx = 0;
    } else {
        recipient_name[0] = 0;
        recipient_chosen = false;
        recipient_is_channel = false;
        recipient_channel_idx = 0;
    }
}

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void show_editor();
static void show_picker();
static void render_recipient_list();

static void set_filter_button_state(lv_obj_t* label, bool active) {
    if (!label) return;
    lv_obj_t* button = lv_obj_get_parent(label);
    if (!button) return;
    lv_obj_set_style_bg_color(button, lv_color_hex(active ? EPD_COLOR_TEXT : EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(active ? EPD_COLOR_BG : EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
}

static void sync_filter_nav() {
    set_filter_button_state(lbl_filter_fav, current_filter == PICK_FILTER_FAVORITES);
    set_filter_button_state(lbl_filter_channels, current_filter == PICK_FILTER_CHANNELS);
    set_filter_button_state(lbl_filter_people, current_filter == PICK_FILTER_PEOPLE);
}

static bool entry_matches_filter(const PickEntry& entry) {
    switch (current_filter) {
        case PICK_FILTER_FAVORITES:
            return !entry.is_channel && (entry.flags & 0x01) != 0;
        case PICK_FILTER_CHANNELS:
            return entry.is_channel;
        case PICK_FILTER_PEOPLE:
        default:
            return !entry.is_channel;
    }
}

static void enable_typing_refresh_mode() {
    if (!refresh_mode_overridden) {
        saved_refresh_mode = ui::port::get_refresh_mode();
        refresh_mode_overridden = true;
    }
    ui::port::set_refresh_mode(UI_REFRESH_MODE_NORMAL);
}

static void restore_refresh_mode() {
    if (!refresh_mode_overridden) return;
    ui::port::set_refresh_mode(saved_refresh_mode);
    refresh_mode_overridden = false;
}

static void update_recipient_card() {
    if (!lbl_to || !lbl_hint) return;

    if (recipient_chosen) {
#ifdef BOARD_TDECK
        static char to_buf[40];
        snprintf(to_buf, sizeof(to_buf), "To: %s", recipient_name);
        lv_label_set_text(lbl_to, to_buf);
#else
        lv_label_set_text(lbl_to, recipient_name);
#endif
    } else {
#ifdef BOARD_TDECK
        lv_label_set_text(lbl_to, "To: Choose");
#else
        lv_label_set_text(lbl_to, "Choose");
#endif
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
        recipient_channel_idx = pick_entries[idx].channel_idx;
        recipient_chosen = true;
        show_editor();
    }
}

static void on_send(lv_event_t* e) {
    const char* text = lv_textarea_get_text(ta);
    if (!text || !text[0] || !recipient_chosen) return;

    bool sent = recipient_is_channel
        ? mesh::task::send_channel(recipient_channel_idx, text)
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

static void set_filter(PickFilter filter) {
    current_filter = filter;
    sync_filter_nav();
    render_recipient_list();
}

static void on_filter_favorites(lv_event_t* e) {
    set_filter(PICK_FILTER_FAVORITES);
}

static void on_filter_channels(lv_event_t* e) {
    set_filter(PICK_FILTER_CHANNELS);
}

static void on_filter_people(lv_event_t* e) {
    set_filter(PICK_FILTER_PEOPLE);
}

static void show_editor() {
    update_recipient_card();

    if (recipient_list_area) lv_obj_add_flag(recipient_list_area, LV_OBJ_FLAG_HIDDEN);
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

    if (recipient_list_area) lv_obj_clear_flag(recipient_list_area, LV_OBJ_FLAG_HIDDEN);
    if (editor_card) lv_obj_add_flag(editor_card, LV_OBJ_FLAG_HIDDEN);
    if (ta) lv_obj_add_flag(ta, LV_OBJ_FLAG_HIDDEN);
    if (send_btn) lv_obj_add_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    if (kb) lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

static void load_entries() {
    pick_count = 0;

    mesh::task::ChannelEntry channels[MAX_GROUP_CHANNELS] = {};
    int channel_count = mesh::task::get_channels(channels, MAX_GROUP_CHANNELS);
    for (int i = 0; i < channel_count && pick_count < (int)(sizeof(pick_entries) / sizeof(pick_entries[0])); i++) {
        strncpy(pick_entries[pick_count].name, channels[i].name, sizeof(pick_entries[pick_count].name) - 1);
        pick_entries[pick_count].name[sizeof(pick_entries[pick_count].name) - 1] = 0;
        pick_entries[pick_count].is_channel = true;
        pick_entries[pick_count].channel_idx = channels[i].idx;
        pick_entries[pick_count].flags = 0;
        pick_count++;
    }

    // Then contacts — only chat nodes and rooms, not relays/sensors
    mesh::task::push_all_contacts();
    mesh::bridge::ContactUpdate cu;
    while (mesh::bridge::pop_contact(cu) && pick_count < (int)(sizeof(pick_entries) / sizeof(pick_entries[0]))) {
        if (cu.type == ADV_TYPE_REPEATER || cu.type == ADV_TYPE_SENSOR) continue;
        strncpy(pick_entries[pick_count].name, cu.name, 31);
        pick_entries[pick_count].name[31] = 0;
        pick_entries[pick_count].is_channel = false;
        pick_entries[pick_count].channel_idx = 0;
        pick_entries[pick_count].flags = cu.flags;
        ui::text::strip_emoji(pick_entries[pick_count].name);
        pick_count++;
    }
}

static void render_recipient_list() {
    if (!recipient_list) return;

    lv_obj_clean(recipient_list);

    int shown = 0;
    for (int i = 0; i < pick_count; i++) {
        if (!entry_matches_filter(pick_entries[i])) continue;

        char label[40];
        if (pick_entries[i].is_channel) {
            snprintf(label, sizeof(label), "# %s", pick_entries[i].name);
        } else if ((pick_entries[i].flags & 0x01) != 0) {
            snprintf(label, sizeof(label), "\xE2\x98\x85 %s", pick_entries[i].name);
        } else {
            strncpy(label, pick_entries[i].name, sizeof(label) - 1);
            label[sizeof(label) - 1] = 0;
        }

        ui::nav::menu_item(recipient_list, NULL, label, on_entry_pick, (void*)(intptr_t)i);
        shown++;
    }

    if (shown == 0) {
        lv_obj_t* empty = lv_label_create(recipient_list);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_flex_grow(empty, 1);
        lv_obj_set_style_text_font(empty, UI_FONT_TITLE, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        if (current_filter == PICK_FILTER_FAVORITES) {
            lv_label_set_text(empty, "\n\nNo favorites yet");
        } else if (current_filter == PICK_FILTER_CHANNELS) {
            lv_label_set_text(empty, "\n\nNo channels yet");
        } else {
            lv_label_set_text(empty, "\n\nNo contacts yet");
        }
    }
}

static void create(lv_obj_t* parent) {
    scr = parent;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);

    ui::nav::back_button_three_actions_ex(parent, "Compose", on_back,
        "Fav", on_filter_favorites, NULL,
        "Chan", on_filter_channels, NULL,
        "Ppl", on_filter_people, NULL,
        &lbl_filter_fav, &lbl_filter_channels, &lbl_filter_people);

    // --- Recipient card ---
    recipient_card = lv_obj_create(parent);
    lv_obj_set_size(recipient_card, lv_pct(95), UI_COMPOSE_RECIPIENT_H);
    lv_obj_align(recipient_card, LV_ALIGN_TOP_MID, 0, UI_COMPOSE_RECIPIENT_Y);
    lv_obj_set_style_bg_color(recipient_card, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(recipient_card, UI_BORDER_CARD, LV_PART_MAIN);
    lv_obj_set_style_border_color(recipient_card, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
#ifdef BOARD_TDECK
    lv_obj_set_style_radius(recipient_card, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(recipient_card, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(recipient_card, 4, LV_PART_MAIN);
#else
    lv_obj_set_style_radius(recipient_card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(recipient_card, 16, LV_PART_MAIN);
#endif
    lv_obj_clear_flag(recipient_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(recipient_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(recipient_card, on_recipient_click, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(recipient_card, 15);

#ifdef BOARD_TDECK
    // Compact single-line: "To: Name  >"
    lbl_to = lv_label_create(recipient_card);
    lv_obj_set_width(lbl_to, lv_pct(85));
    lv_obj_set_style_text_font(lbl_to, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_to, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_long_mode(lbl_to, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl_to, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* recipient_arrow = lv_label_create(recipient_card);
    lv_obj_set_style_text_font(recipient_arrow, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(recipient_arrow, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(recipient_arrow, LV_SYMBOL_RIGHT);
    lv_obj_align(recipient_arrow, LV_ALIGN_RIGHT_MID, 0, 0);
#else
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
#endif

    lbl_hint = lv_label_create(recipient_card);
    lv_obj_set_width(lbl_hint, lv_pct(92));
    lv_obj_set_style_text_font(lbl_hint, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_long_mode(lbl_hint, LV_LABEL_LONG_DOT);
    lv_obj_add_flag(lbl_hint, LV_OBJ_FLAG_HIDDEN);
    update_recipient_card();

    // --- Recipient picker list ---
    recipient_list_area = ui::nav::content_area(parent);
    lv_coord_t picker_height = lv_display_get_vertical_resolution(lv_display_get_default()) - UI_COMPOSE_LIST_Y - UI_OUTER_MARGIN_X;
    if (picker_height < 0) {
        picker_height = 0;
    }
    lv_obj_set_size(recipient_list_area, lv_pct(95), picker_height);
    lv_obj_align(recipient_list_area, LV_ALIGN_TOP_MID, 0, UI_COMPOSE_LIST_Y);

    recipient_list = lv_obj_create(recipient_list_area);
    lv_obj_set_size(recipient_list, lv_pct(100), lv_pct(100));
    lv_obj_align(recipient_list, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(recipient_list, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(recipient_list, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(recipient_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(recipient_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(recipient_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(recipient_list, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM));

    load_entries();
    sync_filter_nav();
    render_recipient_list();

    // --- Editor card (Message title + char count) ---
#ifdef BOARD_TDECK
    // On T-Deck: plain labels, no bordered card — save vertical space
    editor_card = lv_obj_create(parent);
    lv_obj_set_size(editor_card, lv_pct(95), UI_COMPOSE_EDITOR_H);
    lv_obj_align(editor_card, LV_ALIGN_TOP_MID, 0, UI_COMPOSE_EDITOR_Y);
    lv_obj_set_style_bg_opa(editor_card, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(editor_card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(editor_card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(editor_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* msg_title = lv_label_create(editor_card);
    lv_obj_set_style_text_font(msg_title, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(msg_title, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(msg_title, "Message");
    lv_obj_align(msg_title, LV_ALIGN_LEFT_MID, 0, 0);

    char_count = lv_label_create(editor_card);
    lv_obj_set_style_text_font(char_count, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(char_count, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(char_count, "0/150");
    lv_obj_align(char_count, LV_ALIGN_RIGHT_MID, 0, 0);
#else
    editor_card = lv_obj_create(parent);
    lv_obj_set_size(editor_card, lv_pct(95), UI_COMPOSE_EDITOR_H);
    lv_obj_align(editor_card, LV_ALIGN_TOP_MID, 0, UI_COMPOSE_EDITOR_Y);
    lv_obj_set_style_bg_color(editor_card, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(editor_card, UI_BORDER_CARD, LV_PART_MAIN);
    lv_obj_set_style_border_color(editor_card, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
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
#endif

    // --- Text area ---
    ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, lv_pct(95), UI_COMPOSE_TA_H);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, UI_COMPOSE_TA_Y);
    lv_textarea_set_placeholder_text(ta, "Write a short message");
    lv_textarea_set_max_length(ta, 150);
    lv_textarea_set_one_line(ta, false);
    lv_obj_set_style_text_font(ta, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(ta, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ta, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_color(ta, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(ta, UI_BORDER_THIN, LV_PART_MAIN);
#ifdef BOARD_TDECK
    lv_obj_set_style_radius(ta, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ta, 4, LV_PART_MAIN);
#else
    lv_obj_set_style_radius(ta, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ta, 12, LV_PART_MAIN);
#endif
    lv_obj_set_style_anim_duration(ta, 0, LV_PART_CURSOR);
    lv_obj_add_event_cb(ta, on_ta_focus, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta, on_ta_blur, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta, on_ta_change, LV_EVENT_VALUE_CHANGED, NULL);
    update_char_count();

#if UI_COMPOSE_SHOW_KB
    // On-screen keyboard (hidden on devices with physical keyboard)
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
    lv_obj_set_style_border_color(kb, lv_color_hex(EPD_COLOR_BORDER), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, UI_BORDER_THIN, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 10, LV_PART_ITEMS);
    lv_obj_set_style_anim_duration(kb, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(kb, on_kb_event, LV_EVENT_ALL, NULL);
#endif

    send_btn = ui::nav::text_button(parent, "Send", on_send, NULL);
    lv_obj_align(send_btn, LV_ALIGN_BOTTOM_MID, 0, UI_COMPOSE_SEND_BOTTOM);

    if (recipient_chosen) {
        lv_obj_add_flag(recipient_list_area, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(editor_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ta, LV_OBJ_FLAG_HIDDEN);
        if (kb) lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(send_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(editor_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ta, LV_OBJ_FLAG_HIDDEN);
        if (kb) lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
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
    recipient_list_area = NULL;
    recipient_card = NULL;
    lbl_to = NULL;
    lbl_hint = NULL;
    ta = NULL;
    kb = NULL;
    send_btn = NULL;
    editor_card = NULL;
    char_count = NULL;
    lbl_filter_fav = NULL;
    lbl_filter_channels = NULL;
    lbl_filter_people = NULL;
    saved_refresh_mode = UI_REFRESH_MODE_NORMAL;
    refresh_mode_overridden = false;
    recipient_name[0] = 0;
    recipient_chosen = false;
    recipient_is_channel = false;
    recipient_channel_idx = 0;
    current_filter = PICK_FILTER_PEOPLE;
    pick_count = 0;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::compose
