#include <Arduino.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "board.h"
#include "mesh/mesh_task.h"
#include "ui/ui_task.h"

static void mesh_init_task(void* param) {
    mesh::task::start(-1);
    vTaskDelete(NULL);
}

void setup() {
    // Disable WiFi at driver level to free DRAM (~40KB)
    esp_wifi_stop();
    esp_wifi_deinit();
    // Initialize all hardware (serial, SPI, I2C, screen, touch, PMU, GPS, SD)
    board::init();

    // Start mesh init in background (no core pinning — let FreeRTOS decide)
    xTaskCreate(mesh_init_task, "mesh_init", 1024 * 8, NULL, 5, NULL);

    // Start LVGL UI — splash polls mesh::task::is_ready()
    ui::task::start(-1);

    Serial.println("t-paper ready");
}

void loop() {
    // Everything runs in FreeRTOS tasks.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
