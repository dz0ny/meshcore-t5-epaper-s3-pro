#include <Arduino.h>
#include <Mesh.h>
#include <SPIFFS.h>
#include <SD.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/SimpleMeshTables.h>

#include "mesh_task.h"
#include "mesh_bridge.h"
#include "companion/target.h"
#include "companion/DataStore.h"
#include "companion/NodePrefs.h"
#include "companion/MyMesh.h"
#include <helpers/esp32/SerialBLEInterface.h>
#include "companion/BridgeUITask.h"
#include "../board.h"

// ---------- Globals ----------

static SerialBLEInterface ble_serial;
static BridgeUITask bridge_ui(&mc_board, &ble_serial);

static StdRNG mc_rng;
static SimpleMeshTables mc_tables;

// DataStore: SPIFFS for identity/prefs, SD card for contacts/channels/blobs
static DataStore* store = NULL;
static MyMesh* the_mesh_ptr = NULL;
static SemaphoreHandle_t mesh_mutex = NULL;

// ---------- FreeRTOS task ----------

static void mesh_task_fn(void* param) {
    while (1) {
        if (xSemaphoreTake(mesh_mutex, pdMS_TO_TICKS(50))) {
            the_mesh_ptr->loop();

            // Update bridge status
            mesh::bridge::MeshStatus s = {};
            s.peer_count = the_mesh_ptr->getNumContacts();
            s.last_rssi = radio_driver.getLastRSSI();
            s.last_snr = radio_driver.getLastSNR();
            s.radio_ok = true;
            mesh::bridge::update_status(s);

            rtc_clock.tick();
            xSemaphoreGive(mesh_mutex);
        }
        delay(1);
    }
}

// ---------- Public API ----------

namespace mesh::task {

void start(int core) {
    mesh::bridge::init();

    // board::init() already set up SPI, Wire, GPIO.
    // Ensure SD CS is deselected before LoRa init (shared SPI bus)
    digitalWrite(BOARD_SD_CS, HIGH);

    if (!radio_init()) {
        Serial.println("MESH: radio init FAILED — continuing without mesh");
        // Don't return — still start the task so UI doesn't crash on null mesh
    }
    mc_rng.begin(radio_get_rng_seed());

    // Init filesystems
    SPIFFS.begin(true);

    // Always use SPIFFS for mesh data (reliable, doesn't conflict with SD on shared SPI)
    store = new DataStore(SPIFFS, rtc_clock);
    store->begin();

    // Create mesh
    the_mesh_ptr = new MyMesh(radio_driver, mc_rng, rtc_clock, mc_tables, *store, &bridge_ui);
    the_mesh_ptr->begin(false); // no display (we handle UI separately)

    // Start BLE serial interface for companion app
    ble_serial.begin(BLE_NAME_PREFIX, the_mesh_ptr->getNodePrefs()->node_name, the_mesh_ptr->getBLEPin());
    the_mesh_ptr->startInterface(ble_serial);

    // Apply radio params from prefs
    NodePrefs* prefs = the_mesh_ptr->getNodePrefs();
    radio_set_params(prefs->freq, prefs->bw, prefs->sf, prefs->cr);
    radio_set_tx_power(prefs->tx_power_dbm);

    sensors.begin();

    mesh_mutex = xSemaphoreCreateMutex();

    Serial.printf("MESH: node '%s' started on %.3f MHz\n", the_mesh_ptr->getNodeName(), prefs->freq);

    // Send initial advert
#if ENABLE_ADVERT_ON_BOOT == 1
    the_mesh_ptr->advert();
#endif

    xTaskCreatePinnedToCore(mesh_task_fn, "mesh", 1024 * 12, NULL, 5, NULL, core);
}

bool send_message(const char* recipient_prefix, const char* text) {
    if (!the_mesh_ptr || !mesh_mutex) return false;
    bool ok = false;
    if (xSemaphoreTake(mesh_mutex, pdMS_TO_TICKS(500))) {
        ContactInfo* r = the_mesh_ptr->searchContactsByPrefix(recipient_prefix);
        if (r) {
            uint32_t ack, timeout;
            int result = the_mesh_ptr->sendMessage(*r, rtc_clock.getCurrentTime(), 0, text, ack, timeout);
            ok = (result != MSG_SEND_FAILED);
        }
        xSemaphoreGive(mesh_mutex);
    }
    return ok;
}

bool send_public(const char* text) {
    // TODO: implement via channel
    return false;
}

const char* node_name() {
    if (!the_mesh_ptr) return "T-Paper";
    return the_mesh_ptr->getNodeName();
}

float get_freq() {
    if (!the_mesh_ptr) return LORA_FREQ;
    return the_mesh_ptr->getNodePrefs()->freq;
}

float get_bw() {
    if (!the_mesh_ptr) return LORA_BW;
    return the_mesh_ptr->getNodePrefs()->bw;
}

uint8_t get_sf() {
    if (!the_mesh_ptr) return LORA_SF;
    return the_mesh_ptr->getNodePrefs()->sf;
}

uint8_t get_cr() {
    if (!the_mesh_ptr) return LORA_CR;
    return the_mesh_ptr->getNodePrefs()->cr;
}

int8_t get_tx_power() {
    if (!the_mesh_ptr) return LORA_TX_POWER;
    return the_mesh_ptr->getNodePrefs()->tx_power_dbm;
}

uint32_t get_packets_recv() {
    return radio_driver.getPacketsRecv();
}

uint32_t get_packets_sent() {
    return radio_driver.getPacketsSent();
}

void push_all_contacts() {
    if (!the_mesh_ptr || !mesh_mutex) return;
    if (xSemaphoreTake(mesh_mutex, pdMS_TO_TICKS(500))) {
        ContactsIterator iter = the_mesh_ptr->startContactsIterator();
        ContactInfo c;
        while (iter.hasNext(the_mesh_ptr, c)) {
            mesh::bridge::ContactUpdate cu = {};
            strncpy(cu.name, c.name, sizeof(cu.name) - 1);
            memcpy(cu.pub_key, c.id.pub_key, 32);
            cu.type = c.type;
            cu.gps_lat = c.gps_lat;
            cu.gps_lon = c.gps_lon;
            cu.path_len = c.out_path_len;
            cu.is_new = false;
            mesh::bridge::push_contact(cu);
        }
        xSemaphoreGive(mesh_mutex);
    }
}

} // namespace mesh::task
