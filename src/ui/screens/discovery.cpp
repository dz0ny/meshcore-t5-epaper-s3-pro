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

static mesh::task::DiscoveredNode nodes[16];
static int node_count = 0;

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

static void rebuild_list() {
    if (!node_list) return;
    lv_obj_clean(node_list);

    if (node_count == 0) {
        lv_obj_t* empty = lv_label_create(node_list);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_flex_grow(empty, 1);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_bold_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_text(empty, "\n\n\nListening...");
        return;
    }

    for (int i = 0; i < node_count; i++) {
        char clean_name[32];
        strncpy(clean_name, nodes[i].name, sizeof(clean_name) - 1);
        clean_name[31] = 0;
        ui::text::strip_emoji(clean_name);

        bool already_added = mesh::task::is_contact(nodes[i].pubkey_prefix);
        ui::nav::toggle_item(node_list, clean_name,
            already_added ? LV_SYMBOL_OK : "+Add",
            on_node_click, (void*)(intptr_t)i);
    }
}

static void poll_discovered(lv_timer_t* t) {
    if (!mesh::bridge::discovery_changed) return;
    mesh::bridge::discovery_changed = false;
    node_count = mesh::task::get_discovered(nodes, 16);
    rebuild_list();
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Discovery", on_back);

    node_list = ui::nav::scroll_list(parent);
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

static void destroy() { scr = NULL; node_list = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::discovery
