#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "board.h"
#include "mesh/mesh_task.h"
#include "ui/ui_task.h"

static void mesh_init_task(void* param) {
    mesh::task::start((int)(intptr_t)param);
    vTaskDelete(NULL);
}

void setup() {
    // Disable WiFi completely to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    // Initialize all hardware (serial, SPI, I2C, screen, touch, PMU, GPS, SD)
    board::init();

    // Start mesh init in background on core 0 (identity/PKI generation)
    xTaskCreatePinnedToCore(mesh_init_task, "mesh_init", 1024 * 8, (void*)0, 5, NULL, 0);

    // Start LVGL UI on core 1 — splash polls mesh::task::is_ready()
    ui::task::start(1);

    Serial.println("t-paper ready");
}

void loop() {
    // Everything runs in FreeRTOS tasks.
    delay(1000);
}
