#include <Arduino.h>
#include "lock.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../ui_port.h"
#include "../components/statusbar.h"
#include "../../model.h"
#include "../../board.h"
#include "../../mesh/mesh_task.h"

namespace ui::screen::lock {

static lv_obj_t* scr = NULL;

static void create(lv_obj_t* parent) {
    scr = parent;

    // Big clock
    lv_obj_t* lbl_time = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_time, &Font_Mono_Bold_90, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl_time, "%02d:%02d", model::clock.hour, model::clock.minute);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, 100);

    // Date
    lv_obj_t* lbl_date = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_date, &Font_Mono_Bold_25, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl_date, "%02d/%02d/20%02d",
        model::clock.day, model::clock.month, model::clock.year);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 210);

    // Unread messages
    if (model::sleep_cfg.unread_messages > 0) {
        lv_obj_t* lbl_unread = lv_label_create(parent);
        lv_obj_set_style_text_font(lbl_unread, &Font_Mono_Bold_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_unread, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text_fmt(lbl_unread, "%d new messages", model::sleep_cfg.unread_messages);
        lv_obj_align(lbl_unread, LV_ALIGN_CENTER, 0, 30);

        if (model::sleep_cfg.last_sender[0]) {
            lv_obj_t* lbl_last = lv_label_create(parent);
            lv_obj_set_style_text_font(lbl_last, &lv_font_montserrat_24, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_last, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
            lv_obj_set_width(lbl_last, lv_pct(90));
            lv_label_set_long_mode(lbl_last, LV_LABEL_LONG_WRAP);
            lv_label_set_text_fmt(lbl_last, "%s: %s",
                model::sleep_cfg.last_sender, model::sleep_cfg.last_message);
            lv_obj_align(lbl_last, LV_ALIGN_CENTER, 0, 80);
        }
    }

    // Battery + mesh at bottom
    lv_obj_t* lbl_info = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_info, &Font_Mono_Bold_25, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl_info, "BAT %d%%  Peers %d",
        model::battery.percent, model::mesh.peer_count);
    lv_obj_align(lbl_info, LV_ALIGN_BOTTOM_MID, 0, -30);
}

static void entry() {
    ui::port::set_backlight(0);
    ui::statusbar::hide();
}

static void exit_fn() {}
static void destroy() { scr = NULL; }

void show() {
    model::update_clock();
    model::update_battery();
    model::update_mesh();
    ui::screen_mgr::switch_to(SCREEN_LOCK, false);
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::lock
