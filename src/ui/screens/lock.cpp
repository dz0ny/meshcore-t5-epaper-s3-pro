#include <Arduino.h>
#include "lock.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../ui_port.h"
#include "../components/statusbar.h"
#include "../../model.h"

namespace ui::screen::lock {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_time = NULL;
static lv_obj_t* lbl_date = NULL;
static lv_obj_t* lbl_unread = NULL;
static lv_obj_t* lbl_info = NULL;

static void create(lv_obj_t* parent) {
    scr = parent;

    lbl_time = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_time, &Font_Mono_Bold_90, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, 100);

    lbl_date = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_date, &Font_Mono_Bold_25, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 210);

    lbl_unread = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_unread, &Font_Mono_Bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_unread, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_unread, LV_ALIGN_CENTER, 0, 30);

    lbl_info = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_info, &Font_Mono_Bold_25, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(lbl_info, LV_ALIGN_BOTTOM_MID, 0, -30);
}

void update() {
    if (!lbl_time) return;

    lv_label_set_text_fmt(lbl_time, "%02d:%02d", model::clock.hour, model::clock.minute);
    lv_label_set_text_fmt(lbl_date, "%02d/%02d/20%02d",
        model::clock.day, model::clock.month, model::clock.year);

    if (model::sleep_cfg.unread_messages > 0) {
        if (model::sleep_cfg.last_sender[0]) {
            lv_label_set_text_fmt(lbl_unread, "%d new: %s",
                model::sleep_cfg.unread_messages, model::sleep_cfg.last_sender);
        } else {
            lv_label_set_text_fmt(lbl_unread, "%d new messages", model::sleep_cfg.unread_messages);
        }
    } else {
        lv_label_set_text(lbl_unread, "");
    }

    lv_label_set_text_fmt(lbl_info, "BAT %d%%", model::battery.percent);
}

static void entry() {
    ui::port::set_backlight(0);
    ui::statusbar::hide();
    update();
}

static void exit_fn() {
}

static void destroy() {
    scr = NULL;
    lbl_time = lbl_date = lbl_unread = lbl_info = NULL;
}

void show() {
    model::update_clock();
    model::update_battery();
    model::update_mesh();
    ui::screen_mgr::switch_to(SCREEN_LOCK, false);
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::lock
