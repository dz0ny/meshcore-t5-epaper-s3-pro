#include "contacts.h"
#include "contact_detail.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/text_utils.h"
#include "../../mesh/mesh_bridge.h"
#include "../../mesh/mesh_task.h"

namespace ui::screen::contacts {

static lv_obj_t* scr = NULL;
static lv_obj_t* contact_list = NULL;
static lv_timer_t* poll_timer = NULL;

// Stored contacts for display
struct DisplayContact {
    char name[32];
    uint8_t pub_key[32];
    uint8_t type;
    bool has_path;
    int32_t gps_lat;  // scaled by 1e6
    int32_t gps_lon;
};
static DisplayContact displayed[100];
static int display_count = 0;

// ---------- Handlers ----------

static void on_back(lv_event_t* e) {
    ui::screen_mgr::pop(true);
}

static void on_contact_click(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < display_count) {
        ui::screen::contact_detail::set_contact(
            displayed[idx].name,
            displayed[idx].gps_lat,
            displayed[idx].gps_lon,
            displayed[idx].type,
            displayed[idx].has_path,
            displayed[idx].pub_key
        );
        ui::screen_mgr::push(SCREEN_CONTACT_DETAIL, true);
    }
}

// ---------- Rebuild the list widget ----------

static void rebuild_list() {
    if (!contact_list) return;
    lv_obj_clean(contact_list);

    for (int i = 0; i < display_count; i++) {
        ui::nav::menu_item(contact_list, NULL, displayed[i].name, on_contact_click, (void*)(intptr_t)i);
    }
}

// ---------- Poll for new contacts from mesh bridge ----------

static void poll_contacts(lv_timer_t* t) {
    mesh::bridge::ContactUpdate cu;
    bool changed = false;
    while (mesh::bridge::pop_contact(cu)) {
        // Check if already displayed
        bool found = false;
        for (int i = 0; i < display_count; i++) {
            if (strcmp(displayed[i].name, cu.name) == 0) {
                memcpy(displayed[i].pub_key, cu.pub_key, 32);
                displayed[i].type = cu.type;
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
            displayed[display_count].has_path = (cu.path_len != 0xFF && cu.path_len > 0);
            displayed[display_count].gps_lat = cu.gps_lat;
            displayed[display_count].gps_lon = cu.gps_lon;
            display_count++;
        }
        changed = true;
    }
    if (changed) rebuild_list();
}

// ---------- Lifecycle ----------

static void create(lv_obj_t* parent) {
    scr = parent;

    ui::nav::back_button(parent, "Contacts", on_back);

    contact_list = ui::nav::scroll_list(parent);

    rebuild_list();
}

static void entry() {
    // Reset display list and reload all contacts from mesh
    display_count = 0;
    mesh::task::push_all_contacts();

    poll_timer = lv_timer_create(poll_contacts, 2000, NULL);
    poll_contacts(NULL); // drain the queue into display
}

static void exit_fn() {
    if (poll_timer) {
        lv_timer_del(poll_timer);
        poll_timer = NULL;
    }
}

static void destroy() {
    scr = NULL;
    contact_list = NULL;
}

screen_lifecycle_t lifecycle = {
    .create  = create,
    .entry   = entry,
    .exit    = exit_fn,
    .destroy = destroy,
};

} // namespace ui::screen::contacts
