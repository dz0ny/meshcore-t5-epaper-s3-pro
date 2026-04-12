#include <cstring>
#include <cstdio>
#include <cmath>
#include <esp_heap_caps.h>
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
static lv_obj_t* canvas_obj = NULL;
static lv_obj_t* tap_layer = NULL;
static lv_obj_t* lbl_zoom = NULL;
static lv_obj_t* lbl_dist_info = NULL;
static lv_timer_t* poll_timer = NULL;
static lv_obj_t* grid_label = NULL;
static lv_obj_t* no_fix_label = NULL;
static lv_obj_t* contact_taps[32] = {};
static lv_obj_t* contact_name_labels[32] = {};

static const uint8_t MAP_GRID_COLOR = 0x90;
static const uint8_t MAP_AXIS_COLOR = 0x50;
static const uint8_t MAP_MARKER_COLOR = 0x00;

// Zoom levels in km radius
static const double zoom_levels[] = {0.5, 1.0, 5.0, 20.0, 50.0};
static const int n_zoom = 5;
static int zoom_idx = 2;  // default 5km

// Map area dimensions
static const int MAP_W = 530;
static const int MAP_H = 720;
static const int MAP_CX = MAP_W / 2;
static const int MAP_CY = MAP_H / 2;

// Canvas buffer in PSRAM (L8 = 1 byte per pixel)
static uint8_t* canvas_buf = NULL;

struct MapContact {
    char name[32];
    double lat, lon;
    int px, py;
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
    while (mesh::bridge::pop_contact(cu) && contact_count < 32) {
        if (cu.gps_lat == 0 && cu.gps_lon == 0) continue;
        strncpy(contacts[contact_count].name, cu.name, 31);
        contacts[contact_count].name[31] = 0;
        ui::text::strip_emoji(contacts[contact_count].name);
        contacts[contact_count].lat = cu.gps_lat / 1e6;
        contacts[contact_count].lon = cu.gps_lon / 1e6;
        contacts[contact_count].px = -1;
        contacts[contact_count].py = -1;
        contact_count++;
    }
}

static void draw_filled_circle(int cx, int cy, int r, uint8_t color) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                int x = cx + dx;
                int y = cy + dy;
                if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) {
                    canvas_buf[y * MAP_W + x] = color;
                }
            }
        }
    }
}

static void draw_hline_dashed(int y, uint8_t color) {
    if (y < 0 || y >= MAP_H) return;
    for (int x = 0; x < MAP_W; x++) {
        if ((x / 4) % 2 == 0) canvas_buf[y * MAP_W + x] = color;
    }
}

static void draw_vline_dashed(int x, uint8_t color) {
    if (x < 0 || x >= MAP_W) return;
    for (int y = 0; y < MAP_H; y++) {
        if ((y / 4) % 2 == 0) canvas_buf[y * MAP_W + x] = color;
    }
}

static void draw_hline(int y, uint8_t color) {
    if (y < 0 || y >= MAP_H) return;
    memset(&canvas_buf[y * MAP_W], color, MAP_W);
}

static void draw_vline(int x, uint8_t color) {
    if (x < 0 || x >= MAP_W) return;
    for (int y = 0; y < MAP_H; y++) {
        canvas_buf[y * MAP_W + x] = color;
    }
}

static void hide_contact_overlays() {
    for (int i = 0; i < 32; i++) {
        if (contact_taps[i]) lv_obj_add_flag(contact_taps[i], LV_OBJ_FLAG_HIDDEN);
        if (contact_name_labels[i]) lv_obj_add_flag(contact_name_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void rebuild_map() {
    if (!canvas_obj || !canvas_buf) return;

    double my_lat = model::gps.lat;
    double my_lon = model::gps.lng;
    double zoom_km = zoom_levels[zoom_idx];

    if (lbl_zoom) {
        static char zbuf[16];
        if (zoom_km < 1.0)
            snprintf(zbuf, sizeof(zbuf), "%dm", (int)(zoom_km * 1000));
        else
            snprintf(zbuf, sizeof(zbuf), "%.0fkm", zoom_km);
        lv_label_set_text(lbl_zoom, zbuf);
    }

    memset(canvas_buf, 0xFF, MAP_W * MAP_H);

    double scale_y = MAP_H / (zoom_km * 2.0);
    double scale_x = MAP_W / (zoom_km * 2.0);
    double cos_lat = cos(my_lat * ui::geo::DEG_TO_RAD);
    if (cos_lat < 0.01) cos_lat = 0.01;

    double grid_km;
    if (zoom_km <= 0.5)      grid_km = 0.1;
    else if (zoom_km <= 1.0) grid_km = 0.25;
    else if (zoom_km <= 5.0) grid_km = 1.0;
    else if (zoom_km <= 20.0) grid_km = 5.0;
    else                     grid_km = 10.0;

    int grid_px_y = (int)(grid_km * scale_y);
    int grid_px_x = (int)(grid_km * scale_x);
    if (grid_px_y > 20 && grid_px_x > 20) {
        for (int gy = grid_px_y; MAP_CY + gy < MAP_H; gy += grid_px_y) {
            draw_hline_dashed(MAP_CY + gy, MAP_GRID_COLOR);
            draw_hline_dashed(MAP_CY - gy, MAP_GRID_COLOR);
        }
        for (int gx = grid_px_x; MAP_CX + gx < MAP_W; gx += grid_px_x) {
            draw_vline_dashed(MAP_CX + gx, MAP_GRID_COLOR);
            draw_vline_dashed(MAP_CX - gx, MAP_GRID_COLOR);
        }
    }

    draw_hline(MAP_CY, MAP_AXIS_COLOR);
    draw_vline(MAP_CX, MAP_AXIS_COLOR);
    draw_filled_circle(MAP_CX, MAP_CY, 6, MAP_MARKER_COLOR);

    hide_contact_overlays();

    static char grid_str[16];
    if (grid_km < 1.0)
        snprintf(grid_str, sizeof(grid_str), "grid: %dm", (int)(grid_km * 1000));
    else
        snprintf(grid_str, sizeof(grid_str), "grid: %.0fkm", grid_km);
    if (grid_label) {
        lv_label_set_text(grid_label, grid_str);
        lv_obj_clear_flag(grid_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (!model::gps.has_fix) {
        if (no_fix_label) {
            lv_obj_clear_flag(no_fix_label, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_invalidate(canvas_obj);
        return;
    }

    if (no_fix_label) {
        lv_obj_add_flag(no_fix_label, LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < contact_count; i++) {
        double dlat = contacts[i].lat - my_lat;
        double dlon = contacts[i].lon - my_lon;

        double dy_km = dlat * ui::geo::KM_PER_DEG_LAT;
        double dx_km = dlon * ui::geo::KM_PER_DEG_LAT * cos_lat;

        double dist = sqrt(dx_km * dx_km + dy_km * dy_km);
        if (dist > zoom_km) {
            contacts[i].px = -1;
            continue;
        }

        int px = MAP_CX + (int)(dx_km * scale_x);
        int py = MAP_CY - (int)(dy_km * scale_y);

        if (px < 10 || px > MAP_W - 10 || py < 10 || py > MAP_H - 30) {
            contacts[i].px = -1;
            continue;
        }

        contacts[i].px = px;
        contacts[i].py = py;

        draw_filled_circle(px, py, 8, MAP_MARKER_COLOR);

        char short_name[10];
        strncpy(short_name, contacts[i].name, 9);
        short_name[9] = 0;

        if (contact_taps[i]) {
            lv_obj_set_pos(contact_taps[i], px - 25, py - 25);
            lv_obj_clear_flag(contact_taps[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (contact_name_labels[i]) {
            lv_label_set_text(contact_name_labels[i], short_name);
            lv_obj_set_pos(contact_name_labels[i], px - 30, py + 12);
            lv_obj_clear_flag(contact_name_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_invalidate(canvas_obj);
}

static void poll_update(lv_timer_t* t) {
    load_contacts();
    rebuild_map();
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Map", on_back);

    if (!canvas_buf) {
        canvas_buf = (uint8_t*)heap_caps_malloc(MAP_W * MAP_H, MALLOC_CAP_SPIRAM);
    }

    canvas_obj = lv_canvas_create(parent);
    lv_obj_set_pos(canvas_obj, 5, 130);
    lv_canvas_set_buffer(canvas_obj, canvas_buf, MAP_W, MAP_H, LV_COLOR_FORMAT_L8);

    tap_layer = lv_obj_create(parent);
    lv_obj_set_size(tap_layer, MAP_W, MAP_H);
    lv_obj_set_pos(tap_layer, 5, 130);
    lv_obj_set_style_bg_opa(tap_layer, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(tap_layer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tap_layer, 0, LV_PART_MAIN);
    lv_obj_clear_flag(tap_layer, LV_OBJ_FLAG_SCROLLABLE);

    grid_label = lv_label_create(tap_layer);
    lv_obj_set_style_text_font(grid_label, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(grid_label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(grid_label, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(grid_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(grid_label, 3, LV_PART_MAIN);
    lv_obj_set_pos(grid_label, 5, 5);

    no_fix_label = lv_label_create(tap_layer);
    lv_obj_set_style_text_font(no_fix_label, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(no_fix_label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(no_fix_label, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(no_fix_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(no_fix_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(no_fix_label, MAP_W);
    lv_label_set_text(no_fix_label, "Waiting for GPS fix...");
    lv_obj_set_pos(no_fix_label, 0, MAP_CY + 30);
    lv_obj_add_flag(no_fix_label, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < 32; i++) {
        contact_taps[i] = lv_obj_create(tap_layer);
        lv_obj_set_size(contact_taps[i], 50, 50);
        lv_obj_set_style_bg_opa(contact_taps[i], LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_border_width(contact_taps[i], 0, LV_PART_MAIN);
        lv_obj_add_flag(contact_taps[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(contact_taps[i], 15);
        lv_obj_add_event_cb(contact_taps[i], on_contact_tap, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_flag(contact_taps[i], LV_OBJ_FLAG_HIDDEN);

        contact_name_labels[i] = lv_label_create(tap_layer);
        lv_obj_set_style_text_font(contact_name_labels[i], &lv_font_noto_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(contact_name_labels[i], lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_bg_color(contact_name_labels[i], lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(contact_name_labels[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(contact_name_labels[i], 2, LV_PART_MAIN);
        lv_obj_add_flag(contact_name_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

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

    lbl_dist_info = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_dist_info, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_dist_info, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(lbl_dist_info, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl_dist_info, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lbl_dist_info, 4, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_dist_info, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl_dist_info, lv_pct(90));
    lv_obj_align(lbl_dist_info, LV_ALIGN_BOTTOM_MID, 0, -85);
    lv_label_set_text(lbl_dist_info, "Tap a contact for distance");

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
    canvas_obj = NULL;
    tap_layer = NULL;
    lbl_zoom = NULL;
    lbl_dist_info = NULL;
    grid_label = NULL;
    no_fix_label = NULL;
    contact_count = 0;
    for (int i = 0; i < 32; i++) {
        contact_taps[i] = NULL;
        contact_name_labels[i] = NULL;
    }
    // canvas_buf persists — reused on next map open, freed only on reboot
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::map
