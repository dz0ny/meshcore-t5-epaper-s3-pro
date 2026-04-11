#include <cmath>
#include <cstring>
#include <cstdio>
#include "contact_detail.h"
#include "compose.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../model.h"
#include "../../mesh/mesh_task.h"
#include <helpers/AdvertDataHelpers.h>
#include "../components/toast.h"

namespace ui::screen::contact_detail {

static lv_obj_t* scr = NULL;

// Contact data (set before pushing screen)
static char contact_name[32];
static int32_t contact_lat = 0;  // scaled 1e6
static int32_t contact_lon = 0;
static uint8_t contact_type = 0;
static bool contact_has_path = false;
static uint8_t contact_pubkey[7] = {};
static bool has_pubkey = false;

// UI widgets
static lv_obj_t* lbl_coords = NULL;
static lv_obj_t* lbl_distance = NULL;
static lv_obj_t* lbl_bearing_text = NULL;
static lv_obj_t* compass_canvas = NULL;

static constexpr double DEG_TO_RAD = M_PI / 180.0;
static constexpr double R_EARTH_KM = 6371.0;

void set_contact(const char* name, int32_t gps_lat, int32_t gps_lon, uint8_t type, bool has_path,
                 const uint8_t* pubkey_prefix) {
    strncpy(contact_name, name, sizeof(contact_name) - 1);
    contact_name[31] = 0;
    contact_lat = gps_lat;
    contact_lon = gps_lon;
    contact_type = type;
    contact_has_path = has_path;
    if (pubkey_prefix) {
        memcpy(contact_pubkey, pubkey_prefix, 7);
        has_pubkey = true;
    } else {
        has_pubkey = false;
    }
}

// Haversine distance in km
static double calc_distance_km(double lat1, double lon1, double lat2, double lon2) {
    double dLat = (lat2 - lat1) * DEG_TO_RAD;
    double dLon = (lon2 - lon1) * DEG_TO_RAD;
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
               sin(dLon / 2) * sin(dLon / 2);
    return R_EARTH_KM * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

// Bearing from point 1 to point 2 in degrees (0=N, 90=E)
static double calc_bearing(double lat1, double lon1, double lat2, double lon2) {
    double dLon = (lon2 - lon1) * DEG_TO_RAD;
    double y = sin(dLon) * cos(lat2 * DEG_TO_RAD);
    double x = cos(lat1 * DEG_TO_RAD) * sin(lat2 * DEG_TO_RAD) -
               sin(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) * cos(dLon);
    double bearing = atan2(y, x) * (180.0 / M_PI);
    return fmod(bearing + 360.0, 360.0);
}

static const char* bearing_to_cardinal(double bearing) {
    if (bearing >= 337.5 || bearing < 22.5)  return "N";
    if (bearing < 67.5)  return "NE";
    if (bearing < 112.5) return "E";
    if (bearing < 157.5) return "SE";
    if (bearing < 202.5) return "S";
    if (bearing < 247.5) return "SW";
    if (bearing < 292.5) return "W";
    return "NW";
}

// Draw compass rose with direction arrow using LVGL line objects
static void draw_compass(lv_obj_t* parent, double bearing_deg) {
    // Compass container
    lv_obj_t* compass = lv_obj_create(parent);
    lv_obj_set_size(compass, 280, 280);
    lv_obj_align(compass, LV_ALIGN_CENTER, 0, 60);
    lv_obj_set_style_bg_color(compass, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(compass, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(compass, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(compass, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(compass, 0, LV_PART_MAIN);
    lv_obj_clear_flag(compass, LV_OBJ_FLAG_SCROLLABLE);

    int cx = 140, cy = 140;  // center
    int r = 120;              // radius for labels

    // Cardinal direction labels
    const char* dirs[] = {"N", "E", "S", "W"};
    int dx[] = {0, 1, 0, -1};
    int dy[] = {-1, 0, 1, 0};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* lbl = lv_label_create(compass);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_bold_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(lbl, dirs[i]);
        lv_obj_align(lbl, LV_ALIGN_CENTER, dx[i] * (r - 15), dy[i] * (r - 15));
    }

    // Direction arrow — thick line from center toward the bearing
    double rad = bearing_deg * DEG_TO_RAD;
    int arrow_len = 90;
    int tip_x = cx + (int)(sin(rad) * arrow_len);
    int tip_y = cy - (int)(cos(rad) * arrow_len);

    // Arrow shaft
    static lv_point_precise_t shaft_pts[2];
    shaft_pts[0] = {(lv_value_precise_t)cx, (lv_value_precise_t)cy};
    shaft_pts[1] = {(lv_value_precise_t)tip_x, (lv_value_precise_t)tip_y};

    lv_obj_t* shaft = lv_line_create(compass);
    lv_line_set_points(shaft, shaft_pts, 2);
    lv_obj_set_style_line_width(shaft, 6, LV_PART_MAIN);
    lv_obj_set_style_line_color(shaft, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);

    // Arrowhead — two short lines from tip
    int head_len = 20;
    double head_angle = 0.5;  // ~30 degrees
    static lv_point_precise_t head1_pts[2];
    static lv_point_precise_t head2_pts[2];

    head1_pts[0] = {(lv_value_precise_t)tip_x, (lv_value_precise_t)tip_y};
    head1_pts[1] = {
        (lv_value_precise_t)(tip_x - (int)(sin(rad - head_angle) * head_len)),
        (lv_value_precise_t)(tip_y + (int)(cos(rad - head_angle) * head_len))
    };
    head2_pts[0] = {(lv_value_precise_t)tip_x, (lv_value_precise_t)tip_y};
    head2_pts[1] = {
        (lv_value_precise_t)(tip_x - (int)(sin(rad + head_angle) * head_len)),
        (lv_value_precise_t)(tip_y + (int)(cos(rad + head_angle) * head_len))
    };

    lv_obj_t* h1 = lv_line_create(compass);
    lv_line_set_points(h1, head1_pts, 2);
    lv_obj_set_style_line_width(h1, 5, LV_PART_MAIN);
    lv_obj_set_style_line_color(h1, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);

    lv_obj_t* h2 = lv_line_create(compass);
    lv_line_set_points(h2, head2_pts, 2);
    lv_obj_set_style_line_width(h2, 5, LV_PART_MAIN);
    lv_obj_set_style_line_color(h2, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);

    // Small dot at center
    lv_obj_t* dot = lv_obj_create(compass);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);

    compass_canvas = compass;
}

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_add(lv_event_t* e) {
    if (has_pubkey) {
        bool ok = mesh::task::add_contact_by_prefix(contact_pubkey);
        ui::toast::show(ok ? "Contact added" : "Add failed");
    }
    ui::screen_mgr::pop(true);
}

static void on_remove(lv_event_t* e) {
    if (has_pubkey) {
        mesh::task::remove_contact_by_prefix(contact_pubkey);
        ui::toast::show("Contact removed");
    }
    ui::screen_mgr::pop(true);
}

static void on_favorite(lv_event_t* e) {
    if (has_pubkey) {
        bool is_fav = mesh::task::toggle_favorite(contact_pubkey);
        ui::toast::show(is_fav ? "Added to favorites" : "Removed from favorites");
    }
}

static void on_send_message(lv_event_t* e) {
    ui::screen::compose::set_recipient(contact_name);
    ui::screen_mgr::push(SCREEN_COMPOSE, true);
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Contacts", on_back);

    double c_lat = contact_lat / 1e6;
    double c_lon = contact_lon / 1e6;
    bool has_contact_gps = (contact_lat != 0 || contact_lon != 0);
    bool has_own_gps = model::gps.has_fix;

    // Name
    lv_obj_t* lbl_name = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_name, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_name, LV_ALIGN_TOP_MID, 0, 130);
    lv_label_set_text(lbl_name, contact_name);

    // Contact coords
    lbl_coords = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_coords, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_coords, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_coords, LV_ALIGN_TOP_MID, 0, 165);

    if (has_contact_gps) {
        static char coord_buf[48];
        snprintf(coord_buf, sizeof(coord_buf), "%.4f, %.4f", c_lat, c_lon);
        lv_label_set_text(lbl_coords, coord_buf);
    } else {
        lv_label_set_text(lbl_coords, "No GPS position");
    }

    // Info line: type + path
    lv_obj_t* lbl_info = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_info, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_info, LV_ALIGN_TOP_MID, 0, 195);
    static char info_buf[48];
    snprintf(info_buf, sizeof(info_buf), "Route: %s", contact_has_path ? "Direct" : "Unknown");
    lv_label_set_text(lbl_info, info_buf);

    // Distance + bearing
    lbl_distance = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_distance, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_distance, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_distance, LV_ALIGN_TOP_MID, 0, 230);

    if (has_contact_gps && has_own_gps) {
        double dist = calc_distance_km(model::gps.lat, model::gps.lng, c_lat, c_lon);
        double bearing = calc_bearing(model::gps.lat, model::gps.lng, c_lat, c_lon);
        static char dist_buf[48];
        if (dist < 1.0) {
            snprintf(dist_buf, sizeof(dist_buf), "%dm %s", (int)(dist * 1000), bearing_to_cardinal(bearing));
        } else {
            snprintf(dist_buf, sizeof(dist_buf), "%.1fkm %s", dist, bearing_to_cardinal(bearing));
        }
        lv_label_set_text(lbl_distance, dist_buf);

        // Draw compass rose with arrow pointing to contact
        draw_compass(parent, bearing);
    } else if (has_contact_gps) {
        lv_label_set_text(lbl_distance, "Waiting for GPS fix...");
    } else {
        lv_label_set_text(lbl_distance, "");
    }

    // Action buttons at bottom
    if (has_pubkey) {
        bool is_existing = mesh::task::is_contact(contact_pubkey);

        if (is_existing) {
            bool can_chat = (contact_type == ADV_TYPE_CHAT || contact_type == ADV_TYPE_ROOM);
            int next_y = -20;

            // Favorite toggle (bottom-most)
            lv_obj_t* fav_btn = ui::nav::text_button(parent, LV_SYMBOL_OK " Favorite", on_favorite, NULL);
            lv_obj_align(fav_btn, LV_ALIGN_BOTTOM_MID, 0, next_y);
            next_y -= 90;

            // Remove Contact
            lv_obj_t* rm_btn = ui::nav::text_button(parent, "Remove Contact", on_remove, NULL);
            lv_obj_align(rm_btn, LV_ALIGN_BOTTOM_MID, 0, next_y);
            next_y -= 90;

            // Send Message (only for chat/room types)
            if (can_chat) {
                lv_obj_t* msg_btn = ui::nav::text_button(parent, "Send Message", on_send_message, NULL);
                lv_obj_align(msg_btn, LV_ALIGN_BOTTOM_MID, 0, next_y);
            }
        } else {
            lv_obj_t* add_btn = ui::nav::text_button(parent, "Add Contact", on_add, NULL);
            lv_obj_align(add_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
        }
    }
}

static void entry() {}
static void exit_fn() {}
static void destroy() {
    scr = NULL;
    lbl_coords = lbl_distance = lbl_bearing_text = NULL;
    compass_canvas = NULL;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::contact_detail
