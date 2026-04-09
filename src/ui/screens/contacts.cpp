#include "contacts.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../mesh/mesh_bridge.h"
#include "../../mesh/mesh_task.h"

namespace ui::screen::contacts {

static lv_obj_t* scr = NULL;
static lv_obj_t* contact_list = NULL;
static lv_timer_t* poll_timer = NULL;

// Stored contacts for display
struct DisplayContact {
    char name[32];
    uint8_t type;
    bool has_path;
};
static DisplayContact displayed[100];
static int display_count = 0;

// ---------- Handlers ----------

static void on_back(lv_event_t* e) {
    ui::screen_mgr::pop(true);
}

static void on_contact_click(lv_event_t* e) {
    // TODO: open chat with this contact
}

// ---------- Rebuild the list widget ----------

static void rebuild_list() {
    if (!contact_list) return;
    lv_obj_clean(contact_list);

    for (int i = 0; i < display_count; i++) {
        lv_obj_t* row = lv_obj_create(contact_list);
        lv_obj_set_size(row, lv_pct(100), 50);
        lv_obj_set_style_bg_color(row, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 8, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_contact_click, LV_EVENT_CLICKED, NULL);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Name
        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(lbl, displayed[i].name);

        // Path indicator
        lv_obj_t* path_lbl = lv_label_create(row);
        lv_obj_set_flex_grow(path_lbl, 1);
        lv_obj_set_style_text_align(path_lbl, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(path_lbl,
            lv_color_hex(displayed[i].has_path ? EPD_COLOR_TEXT : EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(path_lbl, displayed[i].has_path ? LV_SYMBOL_OK : "...");
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
                displayed[i].type = cu.type;
                displayed[i].has_path = (cu.path_len != 0xFF && cu.path_len > 0);
                found = true;
                break;
            }
        }
        if (!found && display_count < 100) {
            strncpy(displayed[display_count].name, cu.name, 31);
            displayed[display_count].name[31] = 0;
            displayed[display_count].type = cu.type;
            displayed[display_count].has_path = (cu.path_len != 0xFF && cu.path_len > 0);
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

    contact_list = lv_obj_create(parent);
    lv_obj_set_size(contact_list, lv_pct(95), lv_pct(85));
    lv_obj_align(contact_list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(contact_list, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(contact_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(contact_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(contact_list, LV_FLEX_FLOW_COLUMN);

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
