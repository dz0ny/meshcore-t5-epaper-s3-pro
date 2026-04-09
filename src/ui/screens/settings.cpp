#include "settings.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../ui_port.h"
#include "../components/nav_button.h"
#include "../../board.h"
#include "../../nvs_param.h"

namespace ui::screen::settings {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_refresh_val = NULL;
static lv_obj_t* lbl_backlight_val = NULL;
static const char* mode_names[] = {"Normal", "Fast", "Neat"};

// ---------- Handlers ----------

static void on_back(lv_event_t* e) {
    ui::screen_mgr::pop(true);
}

static void on_refresh_mode(lv_event_t* e) {
    int mode = ui::port::get_refresh_mode();
    mode = (mode + 1) % 3;
    ui::port::set_refresh_mode(mode);
    nvs_param_set_u8(NVS_ID_REFRESH_MODE, mode);

    // Update the label to show new mode
    if (lbl_refresh_val) {
        lv_label_set_text(lbl_refresh_val, mode_names[mode]);
    }
}

static void on_backlight_cycle(lv_event_t* e) {
    int level = ui::port::get_backlight();
    level = (level + 1) % 4;
    ui::port::set_backlight(level);
    nvs_param_set_u8(NVS_ID_BACKLIGHT, level);
    if (lbl_backlight_val) {
        lv_label_set_text(lbl_backlight_val, ui::port::get_backlight_name());
    }
}

// ---------- Lifecycle ----------

static void create(lv_obj_t* parent) {
    scr = parent;

    ui::nav::back_button(parent, "Settings", on_back);

    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(95), lv_pct(85));
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(list, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    // Refresh mode
    {
        int mode = ui::port::get_refresh_mode();

        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), 70);
        lv_obj_set_style_bg_color(row, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 10, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_refresh_mode, LV_EVENT_CLICKED, NULL);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, &Font_Mono_Bold_30, LV_PART_MAIN);
        lv_label_set_text(lbl, "Refresh");

        lbl_refresh_val = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_refresh_val, &Font_Mono_Bold_25, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_refresh_val, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(lbl_refresh_val, mode_names[mode]);
    }

    // Backlight
    {
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), 70);
        lv_obj_set_style_bg_color(row, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(EPD_COLOR_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 10, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_backlight_cycle, LV_EVENT_CLICKED, NULL);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, &Font_Mono_Bold_30, LV_PART_MAIN);
        lv_label_set_text(lbl, "Light");

        lbl_backlight_val = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_backlight_val, &Font_Mono_Bold_25, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_backlight_val, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text(lbl_backlight_val, ui::port::get_backlight_name());
    }

    // Version info
    {
        lv_obj_t* ver = lv_label_create(list);
        lv_obj_set_style_text_color(ver, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
        lv_label_set_text_fmt(ver, "FW: %s  HW: %s", T_PAPER_SW_VERSION, T_PAPER_HW_VERSION);
        lv_obj_set_style_pad_top(ver, 20, LV_PART_MAIN);
    }
}

static void entry() {}

static void exit_fn() {}

static void destroy() {
    scr = NULL;
    lbl_refresh_val = NULL;
    lbl_backlight_val = NULL;
}

screen_lifecycle_t lifecycle = {
    .create  = create,
    .entry   = entry,
    .exit    = exit_fn,
    .destroy = destroy,
};

} // namespace ui::screen::settings
