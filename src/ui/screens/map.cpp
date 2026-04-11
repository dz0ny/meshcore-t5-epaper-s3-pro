#include <cstring>
#include <cstdio>
#include <cmath>
#include "map.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/geo_utils.h"
#include "../components/text_utils.h"
#include "../../model.h"
#include "../../mesh/mesh_bridge.h"
#include "../../mesh/mesh_task.h"

namespace ui::screen::map {

static lv_obj_t* scr = NULL;
static lv_obj_t* map_area = NULL;
static lv_obj_t* lbl_zoom = NULL;
static lv_obj_t* lbl_dist_info = NULL;
static lv_timer_t* poll_timer = NULL;

// Zoom levels in km radius
static const double zoom_levels[] = {0.5, 1.0, 5.0, 20.0, 50.0};
static const int n_zoom = 5;
static int zoom_idx = 2;  // default 5km

// Map area dimensions
static const int MAP_X = 5;
static const int MAP_Y = 130;
static const int MAP_W = 530;
static const int MAP_H = 720;
static const int MAP_CX = MAP_W / 2;
static const int MAP_CY = MAP_H / 2;

struct MapContact {
    char name[32];
    double lat, lon;
    lv_obj_t* dot;
    lv_obj_t* name_lbl;
};
static MapContact contacts[32];
static int contact_count = 0;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void rebuild_map();

static void on_zoom_in(lv_event_t* e) {
    if (zoom_idx > 0) zoom_idx--;
    rebuild_map();
}

static void on_zoom_out(lv_event_t* e) {
    if (zoom_idx < n_zoom - 1) zoom_idx++;
    rebuild_map();
}

static void on_contact_tap(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= contact_count) return;
    if (!model::gps.has_fix) return;

    double dist = ui::geo::distance_km(model::gps.lat, model::gps.lng,
                                        contacts[idx].lat, contacts[idx].lon);
    double bear = ui::geo::bearing(model::gps.lat, model::gps.lng,
                                    contacts[idx].lat, contacts[idx].lon);
    static char buf[64];
    if (dist < 1.0) {
        snprintf(buf, sizeof(buf), "%s: %dm %s", contacts[idx].name,
                 (int)(dist * 1000), ui::geo::bearing_to_cardinal(bear));
    } else {
        snprintf(buf, sizeof(buf), "%s: %.1fkm %s", contacts[idx].name,
                 dist, ui::geo::bearing_to_cardinal(bear));
    }
    if (lbl_dist_info) lv_label_set_text(lbl_dist_info, buf);
}

static void load_contacts() {
    contact_count = 0;
    mesh::task::push_all_contacts();
    mesh::bridge::ContactUpdate cu;
    while (mesh::bridge::pop_contact(cu) && contact_count < 64) {
        if (cu.gps_lat == 0 && cu.gps_lon == 0) continue;  // skip contacts without GPS
        strncpy(contacts[contact_count].name, cu.name, 31);
        contacts[contact_count].name[31] = 0;
        ui::text::strip_emoji(contacts[contact_count].name);
        contacts[contact_count].lat = cu.gps_lat / 1e6;
        contacts[contact_count].lon = cu.gps_lon / 1e6;
        contacts[contact_count].dot = NULL;
        contacts[contact_count].name_lbl = NULL;
        contact_count++;
    }
}

static void rebuild_map() {
    if (!map_area) return;
    lv_obj_clean(map_area);

    double my_lat = model::gps.lat;
    double my_lon = model::gps.lng;
    double zoom_km = zoom_levels[zoom_idx];

    // Update zoom label
    if (lbl_zoom) {
        static char zbuf[16];
        if (zoom_km < 1.0)
            snprintf(zbuf, sizeof(zbuf), "%dm", (int)(zoom_km * 1000));
        else
            snprintf(zbuf, sizeof(zbuf), "%.0fkm", zoom_km);
        lv_label_set_text(lbl_zoom, zbuf);
    }

    // Scale: pixels per km
    double scale_y = MAP_H / (zoom_km * 2.0);
    double cos_lat = cos(my_lat * ui::geo::DEG_TO_RAD);
    if (cos_lat < 0.01) cos_lat = 0.01;
    double scale_x = MAP_W / (zoom_km * 2.0 / cos_lat);

    // Center crosshair
    static lv_point_precise_t h_pts[2];
    h_pts[0] = {0, (lv_value_precise_t)MAP_CY};
    h_pts[1] = {(lv_value_precise_t)MAP_W, (lv_value_precise_t)MAP_CY};
    lv_obj_t* h_line = lv_line_create(map_area);
    lv_line_set_points(h_line, h_pts, 2);
    lv_obj_set_style_line_width(h_line, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(h_line, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);

    static lv_point_precise_t v_pts[2];
    v_pts[0] = {(lv_value_precise_t)MAP_CX, 0};
    v_pts[1] = {(lv_value_precise_t)MAP_CX, (lv_value_precise_t)MAP_H};
    lv_obj_t* v_line = lv_line_create(map_area);
    lv_line_set_points(v_line, v_pts, 2);
    lv_obj_set_style_line_width(v_line, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(v_line, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);

    // "Me" marker at center
    lv_obj_t* me = lv_label_create(map_area);
    lv_obj_set_style_text_font(me, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(me, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(me, LV_SYMBOL_GPS);
    lv_obj_set_pos(me, MAP_CX - 15, MAP_CY - 15);

    if (!model::gps.has_fix) {
        lv_obj_t* no_fix = lv_label_create(map_area);
        lv_obj_set_style_text_font(no_fix, &lv_font_noto_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(no_fix, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_align(no_fix, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(no_fix, MAP_W);
        lv_label_set_text(no_fix, "Waiting for GPS fix...");
        lv_obj_set_pos(no_fix, 0, MAP_CY + 30);
        return;
    }

    // Plot contacts
    for (int i = 0; i < contact_count; i++) {
        double dlat = contacts[i].lat - my_lat;
        double dlon = contacts[i].lon - my_lon;

        // Convert to km offset
        double dy_km = dlat * ui::geo::KM_PER_DEG_LAT;
        double dx_km = dlon * ui::geo::KM_PER_DEG_LAT * cos_lat;

        // Check if within zoom range
        double dist = sqrt(dx_km * dx_km + dy_km * dy_km);
        if (dist > zoom_km) continue;

        // Convert to pixels (y inverted: north = up)
        int px = MAP_CX + (int)(dx_km * scale_x / ui::geo::KM_PER_DEG_LAT * cos_lat);
        int py = MAP_CY - (int)(dy_km * scale_y / ui::geo::KM_PER_DEG_LAT);

        // Clamp to map area
        if (px < 10 || px > MAP_W - 10 || py < 10 || py > MAP_H - 30) continue;

        // Dot
        lv_obj_t* dot = lv_obj_create(map_area);
        lv_obj_set_size(dot, 16, 16);
        lv_obj_set_pos(dot, px - 8, py - 8);
        lv_obj_set_style_bg_color(dot, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(dot, 20);
        lv_obj_add_event_cb(dot, on_contact_tap, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        contacts[i].dot = dot;

        // Name label below dot
        lv_obj_t* nlbl = lv_label_create(map_area);
        lv_obj_set_style_text_font(nlbl, &lv_font_noto_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(nlbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        // Truncate name to ~8 chars for map
        char short_name[10];
        strncpy(short_name, contacts[i].name, 9);
        short_name[9] = 0;
        lv_label_set_text(nlbl, short_name);
        lv_obj_set_pos(nlbl, px - 30, py + 10);
        contacts[i].name_lbl = nlbl;
    }
}

static void poll_update(lv_timer_t* t) {
    load_contacts();
    rebuild_map();
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Map", on_back);

    // Map container
    map_area = lv_obj_create(parent);
    lv_obj_set_size(map_area, MAP_W, MAP_H);
    lv_obj_set_pos(map_area, MAP_X, MAP_Y);
    lv_obj_set_style_bg_color(map_area, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(map_area, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(map_area, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_pad_all(map_area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(map_area, LV_OBJ_FLAG_SCROLLABLE);

    // Zoom controls at bottom
    lv_obj_t* btn_in = ui::nav::text_button(parent, "+", on_zoom_in, NULL);
    lv_obj_set_size(btn_in, 80, 60);
    lv_obj_align(btn_in, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    lbl_zoom = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_zoom, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_zoom, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_zoom, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_label_set_text(lbl_zoom, "5km");

    lv_obj_t* btn_out = ui::nav::text_button(parent, "-", on_zoom_out, NULL);
    lv_obj_set_size(btn_out, 80, 60);
    lv_obj_align(btn_out, LV_ALIGN_BOTTOM_RIGHT, -20, -20);

    // Distance info label (shown on contact tap)
    lbl_dist_info = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_dist_info, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_dist_info, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_dist_info, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl_dist_info, lv_pct(90));
    lv_obj_align(lbl_dist_info, LV_ALIGN_BOTTOM_MID, 0, -85);
    lv_label_set_text(lbl_dist_info, "Tap a contact for distance");

    // Initial load
    load_contacts();
    rebuild_map();
}

static void entry() {
    load_contacts();
    rebuild_map();
    poll_timer = lv_timer_create(poll_update, 5000, NULL);
}

static void exit_fn() {
    if (poll_timer) { lv_timer_del(poll_timer); poll_timer = NULL; }
}

static void destroy() {
    scr = NULL;
    map_area = NULL;
    lbl_zoom = NULL;
    lbl_dist_info = NULL;
    contact_count = 0;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::map
