#include <Arduino.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "lvgl.h"

#include "ui_task.h"
#include "ui_port.h"
#include "ui_screen_mgr.h"
#include "ui_theme.h"
#include "components/statusbar.h"
#include "../board.h"
#include "../model.h"
#include "screens/home.h"
#include "screens/contacts.h"
#include "screens/chat.h"
#include "screens/settings.h"
#include "screens/gps.h"
#include "screens/battery.h"
#include "screens/mesh_settings.h"
#include "screens/status.h"
#include "screens/set_display.h"
#include "screens/set_gps.h"
#include "screens/set_mesh.h"
#include "screens/discovery.h"
#include "screens/lock.h"

namespace ui::task {

static unsigned long next_model_update = 0;
static unsigned long next_lock_update = 0;
static unsigned long backlight_off_at = 0;

static void ui_task_fn(void* param) {
    while (1) {
        if (board::home_button_pressed) {
            board::home_button_pressed = false;
            model::touch_activity();
            ui::screen_mgr::switch_to(SCREEN_HOME, false);
        }

        // Check sleep timeout — enter lock screen after inactivity
        if (model::should_sleep() && ui::screen_mgr::top_id() != SCREEN_LOCK) {
            ui::screen::lock::show();
        }

        // Wake from lock screen on any touch
        if (ui::screen_mgr::top_id() == SCREEN_LOCK && board::touch.isPressed()) {
            model::touch_activity();
            model::sleep_cfg.unread_messages = 0;
            ui::statusbar::show();
            ui::screen_mgr::switch_to(SCREEN_HOME, false);
        }

        // Update model + labels BEFORE lv_timer_handler so flush uses fresh data.
        // No full-screen invalidation — LVGL tracks dirty areas automatically.
        // Only widgets whose text/state changes get redrawn + partial e-paper update.
        if (millis() > next_model_update) {
            model::update_clock();
            model::update_gps();
            model::update_battery();
            model::update_mesh();
            ui::statusbar::update_now();
            ui::screen::home::update();
            next_model_update = millis() + 2000;
        }

        // Lock screen: update every 60 seconds
        if (ui::screen_mgr::top_id() == SCREEN_LOCK && millis() > next_lock_update) {
            model::update_clock();
            model::update_battery();
            model::update_mesh();
            ui::screen::lock::update();
            next_lock_update = millis() + 60000;
        }

        // Reset activity on any touch + auto backlight at night
        if (board::touch.isPressed()) {
            model::touch_activity();
            // Auto-backlight: turn on at night if in Auto mode
            if (ui::port::is_backlight_auto()) {
                uint8_t h = model::clock.hour;
                if (h < 7 || h >= 19) {
                    analogWrite(BOARD_BL_EN, 100); // mid PWM directly
                    backlight_off_at = millis() + 10000;
                }
            }
        }
        // Auto turn off backlight after timeout
        if (backlight_off_at > 0 && millis() > backlight_off_at) {
            ui::port::set_backlight(0);
            backlight_off_at = 0;
        }

        // Process LVGL timers, input, and flush — AFTER labels are updated
        lv_timer_handler();

        // On lock screen: slow loop to save power.
        // Normal: 20ms matches e-ink refresh period (no point polling faster).
        if (ui::screen_mgr::top_id() == SCREEN_LOCK) {
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

void start(int core) {
    model::touch_activity(); // init sleep timer
    Serial.println("UI: init port...");
    ui::port::init();

    // Splash screen
    {
        lv_obj_t *splash = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(splash, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_pad_all(splash, 0, LV_PART_MAIN);
        lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);
        lv_screen_load(splash);

        lv_obj_t *title = lv_label_create(splash);
        lv_obj_set_style_text_font(title, &Font_Mono_Bold_90, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_label_set_text(title, "Mesh\nCore");
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);

        lv_obj_t *sub = lv_label_create(splash);
        lv_obj_set_style_text_font(sub, &Font_Mono_Bold_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_label_set_text(sub, "T5-ePaper");
        lv_obj_align(sub, LV_ALIGN_CENTER, 0, 60);

        lv_obj_t *ver = lv_label_create(splash);
        lv_obj_set_style_text_font(ver, &Font_Mono_Bold_25, LV_PART_MAIN);
        lv_obj_set_style_text_color(ver, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_label_set_text(ver, T_PAPER_SW_VERSION);
        lv_obj_align(ver, LV_ALIGN_CENTER, 0, 100);

        lv_timer_handler();
        delay(2000);
        lv_obj_delete(splash);
    }

    // Create global statusbar on top layer — persists across all screens
    ui::statusbar::create(NULL);

    Serial.println("UI: init screen mgr...");
    ui::screen_mgr::init();

    Serial.println("UI: register screens...");
    ui::screen_mgr::register_screen(0, &ui::screen::home::lifecycle);
    ui::screen_mgr::register_screen(1, &ui::screen::contacts::lifecycle);
    ui::screen_mgr::register_screen(2, &ui::screen::chat::lifecycle);
    ui::screen_mgr::register_screen(3, &ui::screen::settings::lifecycle);
    ui::screen_mgr::register_screen(4, &ui::screen::gps::lifecycle);
    ui::screen_mgr::register_screen(5, &ui::screen::battery::lifecycle);
    ui::screen_mgr::register_screen(6, &ui::screen::mesh_settings::lifecycle);
    ui::screen_mgr::register_screen(7, &ui::screen::status::lifecycle);
    ui::screen_mgr::register_screen(8, &ui::screen::set_display::lifecycle);
    ui::screen_mgr::register_screen(9, &ui::screen::set_gps::lifecycle);
    ui::screen_mgr::register_screen(10, &ui::screen::set_mesh::lifecycle);
    ui::screen_mgr::register_screen(11, &ui::screen::discovery::lifecycle);
    ui::screen_mgr::register_screen(12, &ui::screen::lock::lifecycle);

    Serial.println("UI: switch to home...");
    ui::screen_mgr::switch_to(0, false);

    Serial.println("UI: first lv_task_handler...");
    lv_task_handler();
    Serial.println("UI: starting task...");

    // Start task pinned to core
    xTaskCreatePinnedToCore(ui_task_fn, "ui", 1024 * 16, NULL, 5, NULL, core);
}

} // namespace ui::task
