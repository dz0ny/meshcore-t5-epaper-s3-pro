#include <Arduino.h>
#include <esp_partition.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPIFFS.h>
#include "board.h"
#include "mesh/mesh_task.h"
#include "ui/ui_task.h"

static bool erase_spiffs_partition() {
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if (!partition) {
        Serial.println("BOOT: SPIFFS partition not found");
        return false;
    }

    uint32_t start = millis();
    Serial.printf("BOOT: erasing SPIFFS partition at 0x%lx (%lu bytes)\n", partition->address, partition->size);
    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    Serial.printf("BOOT: SPIFFS erase finished in %lu ms (err=%d)\n", millis() - start, err);
    return err == ESP_OK;
}

static void init_spiffs() {
    uint32_t start = millis();
    bool spiffs_ok = SPIFFS.begin(false);
    Serial.printf("BOOT: SPIFFS initial mount=%d in %lu ms\n", spiffs_ok, millis() - start);
    if (!spiffs_ok) {
        if (erase_spiffs_partition()) {
            start = millis();
            spiffs_ok = SPIFFS.begin(true);
            Serial.printf("BOOT: SPIFFS remount-after-erase=%d in %lu ms\n", spiffs_ok, millis() - start);
        }
    }
    if (!spiffs_ok) {
        Serial.println("BOOT: SPIFFS unavailable");
    }
}

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
    init_spiffs();

    // Start mesh init in background (no core pinning — let FreeRTOS decide)
    xTaskCreate(mesh_init_task, "mesh_init", 1024 * 8, NULL, 5, NULL);

    // Start LVGL UI — splash polls mesh::task::is_ready()
    ui::task::start(-1);

    Serial.printf("t-paper ready %s\n", T_PAPER_FW_VERSION);
}

void loop() {
    // Everything runs in FreeRTOS tasks.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
