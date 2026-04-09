#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "lvgl.h"

#include "ui_task.h"
#include "ui_port.h"
#include "ui_screen_mgr.h"
#include "ui_theme.h"
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

namespace ui::task {

static unsigned long next_model_update = 0;

static void ui_task_fn(void* param) {
    while (1) {
        // Check home button (GT911 center button)
        if (board::home_button_pressed) {
            board::home_button_pressed = false;
            ui::screen_mgr::switch_to(SCREEN_HOME, false);
        }

        lv_task_handler();

        // Refresh model + force e-paper redraw every 5 seconds
        if (millis() > next_model_update) {
            model::update_clock();
            model::update_gps();
            model::update_battery();
            model::update_mesh();
            lv_obj_invalidate(lv_scr_act());
            next_model_update = millis() + 5000;
        }

        delay(5);
    }
}

void start(int core) {
    // Init LVGL display + touch drivers
    ui::port::init();

    // Init screen manager
    ui::screen_mgr::init();

    // Register screens
    ui::screen_mgr::register_screen(0, &ui::screen::home::lifecycle);
    ui::screen_mgr::register_screen(1, &ui::screen::contacts::lifecycle);
    ui::screen_mgr::register_screen(2, &ui::screen::chat::lifecycle);
    ui::screen_mgr::register_screen(3, &ui::screen::settings::lifecycle);
    ui::screen_mgr::register_screen(4, &ui::screen::gps::lifecycle);
    ui::screen_mgr::register_screen(5, &ui::screen::battery::lifecycle);
    ui::screen_mgr::register_screen(6, &ui::screen::mesh_settings::lifecycle);
    ui::screen_mgr::register_screen(7, &ui::screen::status::lifecycle);

    // Switch to home screen
    ui::screen_mgr::switch_to(0, false);

    // Force first render before handing off to the task —
    // LVGL needs at least one lv_task_handler() to flush the initial screen.
    lv_task_handler();

    // Start task pinned to core
    xTaskCreatePinnedToCore(ui_task_fn, "ui", 1024 * 8, NULL, 5, NULL, core);
}

} // namespace ui::task
