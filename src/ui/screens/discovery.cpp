#include "discovery.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/text_utils.h"
#include "../../mesh/mesh_task.h"

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
        mesh::task::add_contact_by_prefix(nodes[idx].pubkey_prefix);
        // Visual feedback �� switch to contacts
        ui::screen_mgr::pop(true);
    }
}

static void rebuild_list() {
    if (!node_list) return;
    lv_obj_clean(node_list);

    for (int i = 0; i < node_count; i++) {
        lv_obj_t* row = lv_obj_create(node_list);
        lv_obj_set_size(row, lv_pct(100), 60);
        lv_obj_set_style_bg_color(row, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 8, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_node_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Name
        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        char clean_name[32];
        strncpy(clean_name, nodes[i].name, sizeof(clean_name) - 1);
        clean_name[31] = 0;
        ui::text::strip_emoji(clean_name);
        lv_label_set_text(lbl, clean_name);

        // "Add" hint
        lv_obj_t* hint = lv_label_create(row);
        lv_obj_set_style_text_font(hint, &Font_Mono_Bold_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(hint, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(hint, "+Add");
    }

    if (node_count == 0) {
        lv_obj_t* empty = lv_label_create(node_list);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(empty, "Listening for nodes...");
    }
}

static void poll_discovered(lv_timer_t* t) {
    node_count = mesh::task::get_discovered(nodes, 16);
    rebuild_list();
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Discovery", on_back);

    node_list = lv_obj_create(parent);
    lv_obj_set_size(node_list, lv_pct(95), lv_pct(85));
    lv_obj_align(node_list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(node_list, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(node_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(node_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(node_list, LV_FLEX_FLOW_COLUMN);
}

static void entry() {
    node_count = mesh::task::get_discovered(nodes, 16);
    rebuild_list();
    poll_timer = lv_timer_create(poll_discovered, 3000, NULL);
}

static void exit_fn() {
    if (poll_timer) { lv_timer_del(poll_timer); poll_timer = NULL; }
}

static void destroy() { scr = NULL; node_list = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::discovery
