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
#include "../sd_log.h"
#include "../mesh/mesh_task.h"
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
#include "screens/contact_detail.h"
#include "screens/msg_detail.h"
#include "screens/set_ble.h"
#include "screens/set_storage.h"
#include "screens/compose.h"

static void show_power_off_splash() {
    ui::statusbar::hide();
    lv_obj_t* splash = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_pad_all(splash, 0, LV_PART_MAIN);
    lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);
    lv_screen_load(splash);

    lv_obj_t* title = lv_label_create(splash);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_bold_80, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_label_set_text(title, "MeshCore");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);

    lv_obj_t* sub = lv_label_create(splash);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(sub, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_label_set_text(sub, "Power Off");
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t* hint = lv_label_create(splash);
    lv_obj_set_width(hint, lv_pct(80));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(hint, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    if (board::charger_vbus_in()) {
        lv_label_set_text(hint, "USB connected - press BOOT to wake");
    } else {
        lv_label_set_text(hint, "Press PWR to wake");
    }
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 120);

    lv_timer_handler();
    delay(3000);
}

// Cut battery power via BQ25896 BATFET_DIS (PPM.shutdown)
// Only works on battery — no effect when USB connected
// Wake by pressing PWR/QON button or connecting USB
static void shutdown_battery() {
    board::ppm.shutdown();
}

void do_power_off() {
    Serial.println("SHUTDOWN: starting power off sequence");
    Serial.flush();

    show_power_off_splash();

    Serial.println("SHUTDOWN: flushing SD");
    Serial.flush();
    sd_log::flush();

    Serial.println("SHUTDOWN: sleeping touch");
    Serial.flush();
    board::touch.sleep();

    Serial.println("SHUTDOWN: pulling RST pins low");
    Serial.flush();
    digitalWrite(BOARD_TOUCH_RST, LOW);
    digitalWrite(BOARD_LORA_RST, LOW);
    gpio_hold_en((gpio_num_t)BOARD_TOUCH_RST);
    gpio_hold_en((gpio_num_t)BOARD_LORA_RST);
    gpio_deep_sleep_hold_en();

    Serial.println("SHUTDOWN: disabling GPS/LoRa power");
    Serial.flush();
    io_extend_lora_gps_power_on(false);

    Serial.println("SHUTDOWN: backlight off, epd poweroff");
    Serial.flush();
    ui::port::set_backlight(0);
    epd_poweroff();

    Serial.println("SHUTDOWN: attempting battery FET shutdown (PPM.shutdown)");
    Serial.flush();
    delay(100);
    shutdown_battery();

    Serial.println("SHUTDOWN: battery shutdown sent, waiting 1s...");
    Serial.flush();
    delay(1000);

    // If we get here, USB is connected — battery shutdown had no effect
    Serial.println("SHUTDOWN: still alive — USB powered, entering deep sleep");
    Serial.flush();
    delay(100);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_BOOT_BTN, 0);
    esp_deep_sleep_start();
}

namespace ui::task {

static unsigned long next_model_update = 0;
static unsigned long next_lock_update = 0;
static unsigned long backlight_off_at = 0;

static bool boot_btn_last = true;   // GPIO 0 is active-low, idle=high
static uint32_t boot_press_start = 0;
static bool io48_btn_last = false;   // PCA9555 button (IO48) via button_read()
static const uint32_t LONG_PRESS_MS = 2000;

static void ui_task_fn(void* param) {
    // Setup BOOT button as input (GPIO 0)
    pinMode(BOARD_BOOT_BTN, INPUT_PULLUP);

    while (1) {
        // Home button (GT911 touch center)
        if (board::home_button_pressed) {
            board::home_button_pressed = false;
            model::touch_activity();
            if (ui::screen_mgr::top_id() == SCREEN_LOCK) {
                ui::statusbar::show();
            }
            ui::screen_mgr::switch_to(SCREEN_HOME, false);
        }

        // BOOT button (GPIO 0)
        // Short press: toggle lock screen
        // Long press (>2s): power off
        bool boot_now = digitalRead(BOARD_BOOT_BTN) == LOW;
        if (boot_now && !boot_btn_last) {
            boot_press_start = millis();
        } else if (boot_now && boot_btn_last && boot_press_start > 0) {
            if ((millis() - boot_press_start) >= LONG_PRESS_MS) {
                do_power_off();
            }
        } else if (!boot_now && boot_btn_last && boot_press_start > 0) {
            if ((millis() - boot_press_start) < LONG_PRESS_MS) {
                model::touch_activity();
                if (ui::screen_mgr::top_id() == SCREEN_LOCK) {
                    ui::statusbar::show();
                    ui::screen_mgr::switch_to(SCREEN_HOME, false);
                } else {
                    ui::screen::lock::show();
                }
            }
            boot_press_start = 0;
        }
        boot_btn_last = boot_now;

        // IO48 button (PCA9555 IO expander) — compose / wake from lock
        bool io48_now = button_read();
        if (io48_now && !io48_btn_last) {
            model::touch_activity();
            if (ui::screen_mgr::top_id() == SCREEN_LOCK) {
                ui::statusbar::show();
                ui::screen_mgr::switch_to(SCREEN_HOME, false);
            } else {
                ui::screen::compose::set_recipient(NULL);
                ui::screen_mgr::push(SCREEN_COMPOSE, true);
            }
        }
        io48_btn_last = io48_now;

        // Check sleep timeout — enter lock screen after inactivity
        if (model::should_sleep() && ui::screen_mgr::top_id() != SCREEN_LOCK) {
            ui::screen::lock::show();
        }

        // Track if we're on lock screen before LVGL processes touch events
        bool was_locked = (ui::screen_mgr::top_id() == SCREEN_LOCK);
        bool lock_touched = was_locked && board::touch.isPressed();
        if (lock_touched) {
            model::touch_activity();
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
                    ui::port::apply_backlight();
                    backlight_off_at = millis() + 10000;
                }
            }
        }
        // Auto turn off backlight after timeout
        if (backlight_off_at > 0 && millis() > backlight_off_at) {
            ui::port::set_backlight(0);
            backlight_off_at = 0;
        }

        // Let LVGL handle its own timing — returns immediately if period hasn't elapsed.
        // On lock screen: slow poll to save power.
        if (ui::screen_mgr::top_id() == SCREEN_LOCK) {
            lv_timer_handler_run_in_period(500);
        } else {
            lv_timer_handler_run_in_period(20);
        }

        // After LVGL processed events: if lock screen was touched but LVGL
        // didn't navigate away (user tapped outside the unread label), wake to home.
        if (lock_touched && ui::screen_mgr::top_id() == SCREEN_LOCK) {
            model::sleep_cfg.unread_messages = 0;
            ui::statusbar::show();
            ui::screen_mgr::switch_to(SCREEN_HOME, false);
        }

        vTaskDelay(pdMS_TO_TICKS(5));  // yield to other tasks
    }
}

void start(int core) {
    model::touch_activity(); // init sleep timer
    Serial.println("UI: init port...");
    ui::port::init();

    // Splash screen with progress
    {
        lv_obj_t *splash = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(splash, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_pad_all(splash, 0, LV_PART_MAIN);
        lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);
        lv_screen_load(splash);

        lv_obj_t *title = lv_label_create(splash);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_bold_80, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_label_set_text(title, "MeshCore");
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);

        lv_obj_t *sub = lv_label_create(splash);
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_bold_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_label_set_text(sub, "T5-ePaper");
        lv_obj_align(sub, LV_ALIGN_CENTER, 0, 60);

        lv_obj_t *ver = lv_label_create(splash);
        lv_obj_set_style_text_font(ver, &lv_font_noto_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(ver, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_label_set_text(ver, T_PAPER_SW_VERSION);
        lv_obj_align(ver, LV_ALIGN_CENTER, 0, 100);

        lv_obj_t *status = lv_label_create(splash);
        lv_obj_set_style_text_font(status, &lv_font_noto_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(status, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(status, lv_pct(80));
        lv_obj_align(status, LV_ALIGN_CENTER, 0, 160);
        lv_label_set_text(status, "Starting...");

        lv_timer_handler();

        // Wait for model data to initialize
        lv_label_set_text(status, "Reading battery...");
        lv_timer_handler();
        model::update_battery();

        lv_label_set_text(status, "Loading identity...");
        lv_timer_handler();

        // Wait for mesh task to finish init (identity/PKI, radio, contacts)
        while (!mesh::task::is_ready()) {
            delay(50);
        }

        lv_label_set_text(status, "Starting mesh...");
        lv_timer_handler();
        model::update_mesh();

        lv_label_set_text(status, "Checking GPS...");
        lv_timer_handler();
        model::update_gps();

        lv_label_set_text(status, "Setting clock...");
        lv_timer_handler();
        model::update_clock();

        lv_label_set_text(status, "Ready");
        lv_timer_handler();
        delay(500);

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
    ui::screen_mgr::register_screen(13, &ui::screen::contact_detail::lifecycle);
    ui::screen_mgr::register_screen(14, &ui::screen::msg_detail::lifecycle);
    ui::screen_mgr::register_screen(15, &ui::screen::set_ble::lifecycle);
    ui::screen_mgr::register_screen(16, &ui::screen::set_storage::lifecycle);
    ui::screen_mgr::register_screen(17, &ui::screen::compose::lifecycle);

    Serial.println("UI: switch to home...");
    ui::screen_mgr::switch_to(0, false);

    Serial.println("UI: first lv_task_handler...");
    lv_task_handler();
    Serial.println("UI: starting task...");

    // Start task pinned to core
    xTaskCreatePinnedToCore(ui_task_fn, "ui", 1024 * 32, NULL, 5, NULL, core);
}

} // namespace ui::task
