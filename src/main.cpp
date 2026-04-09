#include <Arduino.h>
#include "board.h"
#include "mesh/mesh_task.h"
#include "ui/ui_task.h"

void setup() {
    // Initialize all hardware (serial, SPI, I2C, screen, touch, PMU, GPS, SD)
    board::init();

    // Start MeshCore networking on core 0
    mesh::task::start(0);

    // Start LVGL UI on core 1
    ui::task::start(1);

    Serial.println("t-paper ready");
}

void loop() {
    // Everything runs in FreeRTOS tasks.
    delay(1000);
}
