#include "contacts.h"
#include "contact_detail.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/text_utils.h"
#include "../../mesh/mesh_bridge.h"
#include "../../mesh/mesh_task.h"
#include <helpers/AdvertDataHelpers.h>

namespace ui::screen::contacts {

static lv_obj_t* scr = NULL;
static lv_obj_t* contact_list = NULL;
static lv_obj_t* lbl_filter = NULL;
static lv_timer_t* poll_timer = NULL;

// Filter: 0=Chat, 1=Relay, 2=Favorite, 3=All
static int filter_mode = 0;
static const char* filter_names[] = {"Chat", "Relay", "Fav", "All"};

struct DisplayContact {
    char name[32];
    uint8_t pub_key[32];
    uint8_t type;
    uint8_t flags;
    bool has_path;
    int32_t gps_lat;
    int32_t gps_lon;
};
static DisplayContact displayed[MAX_CONTACTS];
static int display_count = 0;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_contact_click(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < display_count) {
        ui::screen::contact_detail::set_contact(
            displayed[idx].name, displayed[idx].gps_lat, displayed[idx].gps_lon,
            displayed[idx].type, displayed[idx].has_path, displayed[idx].pub_key);
        ui::screen_mgr::push(SCREEN_CONTACT_DETAIL, true);
    }
}

static bool passes_filter(const DisplayContact& c) {
    switch (filter_mode) {
        case 0: return (c.type == ADV_TYPE_CHAT || c.type == ADV_TYPE_ROOM);
        case 1: return (c.type == ADV_TYPE_REPEATER);
        case 2: return (c.flags & 0x01) != 0;
        case 3: return true;
        default: return true;
    }
}

static void rebuild_list();

static void on_filter_cycle(lv_event_t* e) {
    filter_mode = (filter_mode + 1) % 4;
    if (lbl_filter) lv_label_set_text(lbl_filter, filter_names[filter_mode]);
    rebuild_list();
}

static void rebuild_list() {
    if (!contact_list) return;
    lv_obj_clean(contact_list);

    // Filter toggle row
    lbl_filter = ui::nav::toggle_item(contact_list, "Filter", filter_names[filter_mode], on_filter_cycle, NULL);

    int shown = 0;
    for (int i = 0; i < display_count; i++) {
        if (!passes_filter(displayed[i])) continue;
        const char* icon = (displayed[i].flags & 0x01) ? LV_SYMBOL_OK " " : "";
        char label[40];
        snprintf(label, sizeof(label), "%s%s", icon, displayed[i].name);
        ui::nav::menu_item(contact_list, NULL, label, on_contact_click, (void*)(intptr_t)i);
        shown++;
    }

    if (shown == 0) {
        lv_obj_t* empty = lv_label_create(contact_list);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_flex_grow(empty, 1);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_bold_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_text(empty, "\n\n\nNo contacts");
    }
}

static void poll_contacts(lv_timer_t* t) {
    mesh::bridge::ContactUpdate cu;
    bool changed = false;
    while (mesh::bridge::pop_contact(cu)) {
        bool found = false;
        for (int i = 0; i < display_count; i++) {
            if (strcmp(displayed[i].name, cu.name) == 0) {
                memcpy(displayed[i].pub_key, cu.pub_key, 32);
                displayed[i].type = cu.type;
                displayed[i].flags = cu.flags;
                displayed[i].has_path = (cu.path_len != 0xFF && cu.path_len > 0);
                displayed[i].gps_lat = cu.gps_lat;
                displayed[i].gps_lon = cu.gps_lon;
                found = true;
                break;
            }
        }
        if (!found && display_count < 100) {
            strncpy(displayed[display_count].name, cu.name, 31);
            displayed[display_count].name[31] = 0;
            ui::text::strip_emoji(displayed[display_count].name);
            memcpy(displayed[display_count].pub_key, cu.pub_key, 32);
            displayed[display_count].type = cu.type;
            displayed[display_count].flags = cu.flags;
            displayed[display_count].has_path = (cu.path_len != 0xFF && cu.path_len > 0);
            displayed[display_count].gps_lat = cu.gps_lat;
            displayed[display_count].gps_lon = cu.gps_lon;
            display_count++;
        }
        changed = true;
    }
    if (changed) rebuild_list();
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Contacts", on_back);
    contact_list = ui::nav::scroll_list(parent);
    rebuild_list();
}

static void entry() {
    display_count = 0;
    mesh::task::push_all_contacts();
    poll_timer = lv_timer_create(poll_contacts, 2000, NULL);
    poll_contacts(NULL);
}

static void exit_fn() {
    if (poll_timer) { lv_timer_del(poll_timer); poll_timer = NULL; }
}

static void destroy() {
    scr = NULL;
    contact_list = NULL;
    lbl_filter = NULL;
}

screen_lifecycle_t lifecycle = {
    .create  = create,
    .entry   = entry,
    .exit    = exit_fn,
    .destroy = destroy,
};

} // namespace ui::screen::contacts
