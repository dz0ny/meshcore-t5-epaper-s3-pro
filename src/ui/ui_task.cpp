#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <OneButton.h>
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
#include "screens/map.h"

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
    vTaskDelay(pdMS_TO_TICKS(3000));
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
    vTaskDelay(pdMS_TO_TICKS(100));
    shutdown_battery();

    Serial.println("SHUTDOWN: battery shutdown sent, waiting 1s...");
    Serial.flush();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // If we get here, USB is connected — battery shutdown had no effect
    Serial.println("SHUTDOWN: still alive — USB powered, entering deep sleep");
    Serial.flush();
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_BOOT_BTN, 0);
    esp_deep_sleep_start();
}

namespace ui::task {

static unsigned long next_model_update = 0;
static unsigned long next_lock_update = 0;
static unsigned long backlight_off_at = 0;

static OneButton boot_btn(BOARD_BOOT_BTN, true, true);  // active-low, internal pull-up
static OneButton pca_btn;  // PCA9535 IO expander button (PC12)

static void on_boot_click() {
    model::touch_activity();
    if (ui::screen_mgr::top_id() == SCREEN_LOCK) {
        ui::statusbar::show();
        ui::screen_mgr::switch_to(SCREEN_HOME, false);
    } else {
        ui::screen::lock::show();
    }
}

static void on_boot_long_press() {
    do_power_off();
}

static void on_pca_click() {
    model::touch_activity();
    if (ui::screen_mgr::top_id() == SCREEN_LOCK) {
        ui::statusbar::show();
    }
    ui::screen_mgr::switch_to(SCREEN_HOME, false);
}

static void ui_task_fn(void* param) {
    // BOOT button (GPIO 0): short press toggles lock, long press powers off
    boot_btn.setPressTicks(2000);
    boot_btn.attachClick(on_boot_click);
    boot_btn.attachLongPressStart(on_boot_long_press);

    // PCA9535 IO expander button (PC12): acts as home button
    pca_btn.setup(0, true, false);  // pin unused, active-low, no internal pull-up
    pca_btn.attachClick(on_pca_click);

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

        // Tick button state machines
        boot_btn.tick();
        pca_btn.tick(!button_read());  // button_read() returns true when pressed, OneButton expects pin level


        // Check sleep timeout — enter lock screen after inactivity
        if (model::should_sleep() && ui::screen_mgr::top_id() != SCREEN_LOCK) {
            ui::screen::lock::show();
        }

        // Single I2C touch read per loop — cache to avoid hammering the bus
        bool touch_pressed = board::peri_status[E_PERI_TOUCH] && board::touch.isPressed();

        bool was_locked = (ui::screen_mgr::top_id() == SCREEN_LOCK);
        bool lock_touched = was_locked && touch_pressed;
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
            next_model_update = millis() + 10000;  // 10s — e-ink doesn't need fast updates
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
        if (touch_pressed) {
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
            vTaskDelay(pdMS_TO_TICKS(50));
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
        vTaskDelay(pdMS_TO_TICKS(500));

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
    ui::screen_mgr::register_screen(18, &ui::screen::map::lifecycle);

    Serial.println("UI: switch to home...");
    ui::screen_mgr::switch_to(0, false);

    Serial.println("UI: first lv_task_handler...");
    lv_task_handler();
    Serial.println("UI: starting task...");

    // Pin UI to core 1
    xTaskCreatePinnedToCore(ui_task_fn, "ui", 1024 * 16, NULL, 5, NULL, 1);
}

} // namespace ui::task
