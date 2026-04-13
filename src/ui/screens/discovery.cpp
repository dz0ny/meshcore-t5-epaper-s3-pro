#include "discovery.h"
#include "contact_detail.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/text_utils.h"
#include "../../mesh/mesh_task.h"
#include "../../mesh/mesh_bridge.h"

namespace ui::screen::discovery {

static lv_obj_t* scr = NULL;
static lv_obj_t* node_list = NULL;
static lv_timer_t* poll_timer = NULL;
static lv_obj_t* lbl_filter = NULL;
static lv_obj_t* node_rows[16] = {};
static lv_obj_t* node_row_labels[16] = {};
static lv_obj_t* node_row_values[16] = {};
static bool row_visible[16] = {};
static lv_obj_t* empty_label = NULL;

static mesh::task::DiscoveredNode nodes[16];
static int node_count = 0;
static int filter_mode = 0;
static const char* filter_names[] = {"New", "Added", "All"};

static void rebuild_list();

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_node_click(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < node_count) {
        char clean_name[32];
        strncpy(clean_name, nodes[idx].name, sizeof(clean_name) - 1);
        clean_name[31] = 0;
        ui::text::strip_emoji(clean_name);

        ui::screen::contact_detail::set_contact(
            clean_name, 0, 0, 0, false, nodes[idx].pubkey_prefix);
        ui::screen_mgr::push(SCREEN_CONTACT_DETAIL, true);
    }
}

static bool passes_filter(const mesh::task::DiscoveredNode& node) {
    bool already_added = mesh::task::is_contact(node.pubkey_prefix);
    switch (filter_mode) {
        case 0: return !already_added;
        case 1: return already_added;
        case 2: return true;
        default: return true;
    }
}

static void on_filter_cycle(lv_event_t* e) {
    filter_mode = (filter_mode + 1) % 3;
    if (lbl_filter) lv_label_set_text(lbl_filter, filter_names[filter_mode]);
    rebuild_list();
}

static void ensure_row(int idx) {
    if (!node_list || idx < 0 || idx >= 16 || node_rows[idx]) return;

    lv_obj_t* value = ui::nav::toggle_item(node_list, "", "", on_node_click, (void*)(intptr_t)idx);
    lv_obj_t* row = lv_obj_get_parent(value);
    lv_obj_t* label = lv_obj_get_child(row, 0);

    node_rows[idx] = row;
    node_row_labels[idx] = label;
    node_row_values[idx] = value;
    row_visible[idx] = false;
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
}

static void rebuild_list() {
    if (!node_list) return;
    lv_display_t* disp = lv_obj_get_display(node_list);
    lv_display_enable_invalidation(disp, false);

    if (lbl_filter) {
        lv_label_set_text(lbl_filter, filter_names[filter_mode]);
    }

    for (int i = 0; i < 16; i++) {
        ensure_row(i);
    }

    int shown = 0;

    for (int i = 0; i < node_count; i++) {
        if (!passes_filter(nodes[i])) continue;
        char clean_name[32];
        strncpy(clean_name, nodes[i].name, sizeof(clean_name) - 1);
        clean_name[31] = 0;
        ui::text::strip_emoji(clean_name);

        bool already_added = mesh::task::is_contact(nodes[i].pubkey_prefix);
        if (strcmp(lv_label_get_text(node_row_labels[shown]), clean_name) != 0) {
            lv_label_set_text(node_row_labels[shown], clean_name);
        }
        const char* value = already_added ? LV_SYMBOL_OK : "+Add";
        if (strcmp(lv_label_get_text(node_row_values[shown]), value) != 0) {
            lv_label_set_text(node_row_values[shown], value);
        }
        if (!row_visible[shown]) {
            lv_obj_clear_flag(node_rows[shown], LV_OBJ_FLAG_HIDDEN);
            row_visible[shown] = true;
        }
        shown++;
    }

    for (int i = shown; i < 16; i++) {
        if (row_visible[i]) {
            lv_obj_add_flag(node_rows[i], LV_OBJ_FLAG_HIDDEN);
            row_visible[i] = false;
        }
    }

    if (empty_label) {
        if (shown == 0) {
            lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_mark_layout_as_dirty(node_list);
    lv_obj_update_layout(node_list);
    lv_display_enable_invalidation(disp, true);
    lv_obj_invalidate(node_list);
}

static void poll_discovered(lv_timer_t* t) {
    if (!mesh::bridge::discovery_changed) return;
    mesh::bridge::discovery_changed = false;
    node_count = mesh::task::get_discovered(nodes, 16);
    rebuild_list();
}

static void create(lv_obj_t* parent) {
    scr = parent;
    lbl_filter = ui::screen_mgr::set_nav_action(filter_names[filter_mode], on_filter_cycle, NULL);

    node_list = ui::nav::scroll_list(parent);

    empty_label = lv_label_create(node_list);
    lv_obj_set_width(empty_label, lv_pct(100));
    lv_obj_set_flex_grow(empty_label, 1);
    lv_obj_set_style_text_font(empty_label, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(empty_label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(empty_label, "\n\n\nListening...");
    lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
}

static void entry() {
    node_count = mesh::task::get_discovered(nodes, 16);
    rebuild_list();
    mesh::bridge::discovery_changed = false;
    poll_timer = lv_timer_create(poll_discovered, 500, NULL);
}

static void exit_fn() {
    if (poll_timer) { lv_timer_del(poll_timer); poll_timer = NULL; }
}

static void destroy() {
    scr = NULL;
    node_list = NULL;
    lbl_filter = NULL;
    empty_label = NULL;
    for (int i = 0; i < 16; i++) {
        node_rows[i] = NULL;
        node_row_labels[i] = NULL;
        node_row_values[i] = NULL;
        row_visible[i] = false;
    }
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::discovery
