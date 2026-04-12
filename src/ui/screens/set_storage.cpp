#include <Arduino.h>
#include "set_storage.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../ui_port.h"
#include "../components/nav_button.h"
#include "../components/toast.h"
#include "../../model.h"
#include "../../board.h"
#include "../../mesh/mesh_task.h"
#include "../../sd_log.h"
#include <SPIFFS.h>
#include <SD.h>
#ifdef BOARD_EPAPER
#include <epdiy.h>
#endif

namespace ui::screen::set_storage {

static lv_obj_t* scr = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_clear_messages(lv_event_t* e) {
    model::message_count = 0;
    SD.remove("/messages.bin");
    ui::toast::show("Messages cleared");
}

static void on_clear_contacts(lv_event_t* e) {
    mesh::task::clear_contacts();
    ui::toast::show("Contacts cleared");
}

static void on_clear_channels(lv_event_t* e) {
    mesh::task::clear_channels();
    ui::toast::show("Channels cleared");
}

static void on_factory_reset(lv_event_t* e) {
    mesh::task::flush_storage();

    // Shut down peripherals
    mesh::task::ble_disable();
    ui::port::set_backlight(2);  // Off
#ifdef BOARD_EPAPER
    board::touch.sleep();
    digitalWrite(BOARD_TOUCH_RST, LOW);
    digitalWrite(BOARD_LORA_RST, LOW);
    epd_poweroff();
#endif

    // Wipe storage
    SPIFFS.format();
    SD.remove("/messages.bin");
    SD.remove("/telemetry.bin");
    SD.remove("/contacts3");
    SD.remove("/channels2");

    Serial.println("Factory reset — rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Storage", on_back);

    lv_obj_t* list = ui::nav::scroll_list(parent);

    ui::nav::toggle_item(list, "Messages", "Clear", on_clear_messages, NULL);
    ui::nav::toggle_item(list, "Contacts", "Clear", on_clear_contacts, NULL);
    ui::nav::toggle_item(list, "Channels", "Clear", on_clear_channels, NULL);
    ui::nav::toggle_item(list, "Factory Reset", "Reset", on_factory_reset, NULL);
}

static void entry() {}
static void exit_fn() {}
static void destroy() { scr = NULL; }

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::set_storage
