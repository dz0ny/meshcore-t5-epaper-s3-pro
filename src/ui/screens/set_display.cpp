#include "set_display.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../ui_port.h"
#include "../components/nav_button.h"
#include "../components/statusbar.h"
#include "../components/toast.h"
#include "../../nvs_param.h"
#include "../../model.h"

namespace ui::screen::set_display {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_refresh_val = NULL;
static lv_obj_t* lbl_backlight_val = NULL;
static lv_obj_t* lbl_brightness_val = NULL;
static lv_obj_t* lbl_sleep_val = NULL;
static lv_obj_t* lbl_theme_val = NULL;
static const char* mode_names[] = {"Normal", "Fast"};
static const char* sleep_names[] = {"Off", "1 min", "2 min", "5 min", "15 min", "30 min"};
static const uint32_t sleep_ms[] = {0, 60000, 120000, 300000, 900000, 1800000};

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }
static void apply_theme_async(void* data) {
    (void)data;
    ui::statusbar::recreate();
    ui::screen_mgr::reload_stack();
    ui::toast::show(ui::theme::current_name());
}

static void on_sleep_cycle(lv_event_t* e) {
    uint8_t idx = (model::sleep_cfg.timeout_idx + 1) % 6;
    model::sleep_cfg.timeout_idx = idx;
    model::sleep_cfg.timeout_ms = sleep_ms[idx];
    nvs_param_set_u8(NVS_ID_SLEEP_TIMEOUT, idx);
    model::touch_activity();
    if (lbl_sleep_val) lv_label_set_text(lbl_sleep_val, sleep_names[idx]);
}

static void on_refresh_mode(lv_event_t* e) {
    int mode = ui::port::get_refresh_mode();
    mode = (mode + 1) % 2;
    ui::port::set_refresh_mode(mode);
    nvs_param_set_u8(NVS_ID_REFRESH_MODE, mode);
    if (lbl_refresh_val) lv_label_set_text(lbl_refresh_val, mode_names[mode]);
}

static void on_backlight_cycle(lv_event_t* e) {
    int mode = (ui::port::get_backlight() + 1) % 3;
    ui::port::set_backlight(mode);
    nvs_param_set_u8(NVS_ID_BACKLIGHT, mode);
    if (lbl_backlight_val) lv_label_set_text(lbl_backlight_val, ui::port::get_backlight_name());
}

static void on_brightness_cycle(lv_event_t* e) {
    int level = (ui::port::get_brightness() + 1) % 3;
    ui::port::set_brightness(level);
    nvs_param_set_u8(NVS_ID_BRIGHTNESS, level);
    // Re-apply if backlight is currently on
    if (ui::port::get_backlight() == 1) ui::port::apply_backlight();
    if (lbl_brightness_val) lv_label_set_text(lbl_brightness_val, ui::port::get_brightness_name());
}

static void on_theme_cycle(lv_event_t* e) {
    ui::theme::theme_id next_theme = ui::theme::next();
    if (!ui::theme::set(next_theme)) return;

    nvs_param_set_u8(NVS_ID_UI_THEME, static_cast<uint8_t>(next_theme));
    lv_async_call(apply_theme_async, NULL);
}

static void create(lv_obj_t* parent) {
    scr = parent;

    lv_obj_t* list = ui::nav::scroll_list(parent);

#ifdef BOARD_EPAPER
    lbl_refresh_val = ui::nav::toggle_item(list, "Refresh", mode_names[ui::port::get_refresh_mode()], on_refresh_mode, NULL);
#endif
#ifdef BOARD_TDECK
    lbl_theme_val = ui::nav::toggle_item(list, "Theme", ui::theme::current_name(), on_theme_cycle, NULL);
#endif
#ifdef BOARD_EPAPER
    lbl_backlight_val = ui::nav::toggle_item(list, "Light", ui::port::get_backlight_name(), on_backlight_cycle, NULL);
#endif
    lbl_brightness_val = ui::nav::toggle_item(list, "Brightness", ui::port::get_brightness_name(), on_brightness_cycle, NULL);
    lbl_sleep_val = ui::nav::toggle_item(list, "Sleep", sleep_names[model::sleep_cfg.timeout_idx], on_sleep_cycle, NULL);
}

static void entry() {}
static void exit_fn() {}
static void destroy() {
    scr = NULL;
    lbl_refresh_val = NULL;
    lbl_backlight_val = NULL;
    lbl_brightness_val = NULL;
    lbl_sleep_val = NULL;
    lbl_theme_val = NULL;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::set_display
