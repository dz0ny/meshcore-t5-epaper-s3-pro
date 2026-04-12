#include "sd_log.h"
#include "board.h"
#include <esp_heap_caps.h>
#include <SD.h>
#include <cstring>
#include <time.h>

static const char* MSG_FILE = "/messages.bin";
static const char* TELEMETRY_FILE = "/telemetry.bin";
static const int MAX_TELEMETRY_RECORDS = 32;

struct TelemetryRecord {
    uint8_t pub_key_prefix[7];
    uint8_t data[96];
    uint8_t len;
    uint32_t timestamp;
};

// Track how many messages have been written to SD so we only append new ones.
static int flushed_count = 0;
static bool dirty = false;
static bool telemetry_dirty = false;
static bool sd_available = false;
static TelemetryRecord* telemetry_records = nullptr;
static int telemetry_count = 0;

static void ensure_telemetry_cache() {
    if (!telemetry_records) {
        telemetry_records = (TelemetryRecord*)heap_caps_calloc(
            MAX_TELEMETRY_RECORDS, sizeof(TelemetryRecord), MALLOC_CAP_SPIRAM);
    }
}

namespace sd_log {

void load() {
    ensure_telemetry_cache();

    // Deselect LoRa, select SD
    digitalWrite(BOARD_LORA_CS, HIGH);
    sd_available = true;
    int count = 0;
    if (SD.exists(MSG_FILE)) {
        File f = SD.open(MSG_FILE, FILE_READ);
        if (f) {
            model::StoredMessage tmp;
            while (count < MAX_STORED_MESSAGES && f.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp)) {
                model::messages[count] = tmp;
                count++;
            }
            f.close();
        }
    }

    model::message_count = count;
    flushed_count = count;
    dirty = false;

    telemetry_count = 0;
    if (telemetry_records && SD.exists(TELEMETRY_FILE)) {
        File tf = SD.open(TELEMETRY_FILE, FILE_READ);
        if (tf) {
            TelemetryRecord record;
            while (telemetry_count < MAX_TELEMETRY_RECORDS &&
                   tf.read((uint8_t*)&record, sizeof(record)) == sizeof(record)) {
                telemetry_records[telemetry_count] = record;
                telemetry_count++;
            }
            tf.close();
        }
    }
    telemetry_dirty = false;

    // Deselect SD when done
    digitalWrite(BOARD_SD_CS, HIGH);

    Serial.printf("SD_LOG: loaded %d messages and %d telemetry snapshots from SD\n", count, telemetry_count);
}

void mark_dirty() {
    dirty = true;
}

bool is_dirty() {
    return dirty || telemetry_dirty;
}

void flush() {
    if ((!dirty && !telemetry_dirty) || !sd_available) return;

    // Deselect LoRa before touching SD
    digitalWrite(BOARD_LORA_CS, HIGH);

    int written = 0;
    if (dirty) {
        // Deletion happened — rewrite entire file
        bool full_rewrite = (model::message_count < flushed_count);

        File f;
        if (flushed_count == 0 || full_rewrite) {
            f = SD.open(MSG_FILE, FILE_WRITE);  // truncate + write
        } else if (model::message_count > flushed_count) {
            f = SD.open(MSG_FILE, FILE_APPEND);
        } else {
            dirty = false;
        }

        if (dirty) {
            if (!f) {
                sd_available = false;
                digitalWrite(BOARD_SD_CS, HIGH);
                Serial.println("SD_LOG: flush failed — SD not writable");
                return;
            }

            int start = full_rewrite ? 0 : flushed_count;
            for (int i = start; i < model::message_count; i++) {
                size_t n = f.write((const uint8_t*)&model::messages[i], sizeof(model::StoredMessage));
                if (n != sizeof(model::StoredMessage)) {
                    Serial.println("SD_LOG: short write, stopping");
                    break;
                }
                written++;
            }
            f.close();

            flushed_count = full_rewrite ? written : (flushed_count + written);
            if (flushed_count >= model::message_count) {
                dirty = false;
            }
        }
    }

    if (telemetry_dirty) {
        SD.remove(TELEMETRY_FILE);
        File tf = SD.open(TELEMETRY_FILE, FILE_WRITE);
        if (!tf) {
            sd_available = false;
            digitalWrite(BOARD_SD_CS, HIGH);
            Serial.println("SD_LOG: telemetry flush failed — SD not writable");
            return;
        }

        for (int i = 0; i < telemetry_count; i++) {
            size_t n = tf.write((const uint8_t*)&telemetry_records[i], sizeof(TelemetryRecord));
            if (n != sizeof(TelemetryRecord)) {
                Serial.println("SD_LOG: telemetry short write, stopping");
                break;
            }
        }
        tf.close();
        telemetry_dirty = false;
    }

    // Deselect SD, bus is free for LoRa again
    digitalWrite(BOARD_SD_CS, HIGH);

    Serial.printf("SD_LOG: flushed %d messages to SD (total %d)\n", written, flushed_count);
}

void store_telemetry(const uint8_t* pub_key_prefix, const uint8_t* data, uint8_t len) {
    if (!pub_key_prefix || !data || len == 0) return;
    ensure_telemetry_cache();
    if (!telemetry_records) return;
    uint32_t now = (uint32_t)time(nullptr);

    for (int i = 0; i < telemetry_count; i++) {
        if (memcmp(telemetry_records[i].pub_key_prefix, pub_key_prefix, sizeof(telemetry_records[i].pub_key_prefix)) == 0) {
            telemetry_records[i].len = len > sizeof(telemetry_records[i].data) ? sizeof(telemetry_records[i].data) : len;
            memcpy(telemetry_records[i].data, data, telemetry_records[i].len);
            telemetry_records[i].timestamp = now;
            telemetry_dirty = true;
            return;
        }
    }

    int idx = telemetry_count < MAX_TELEMETRY_RECORDS ? telemetry_count : (MAX_TELEMETRY_RECORDS - 1);
    if (telemetry_count < MAX_TELEMETRY_RECORDS) {
        telemetry_count++;
    }
    memcpy(telemetry_records[idx].pub_key_prefix, pub_key_prefix, sizeof(telemetry_records[idx].pub_key_prefix));
    telemetry_records[idx].len = len > sizeof(telemetry_records[idx].data) ? sizeof(telemetry_records[idx].data) : len;
    memcpy(telemetry_records[idx].data, data, telemetry_records[idx].len);
    telemetry_records[idx].timestamp = now;
    telemetry_dirty = true;
}

bool get_telemetry(const uint8_t* pub_key_prefix, uint8_t* out, uint8_t* len, size_t out_size, uint32_t* timestamp) {
    if (!pub_key_prefix || !out || !len || out_size == 0) return false;
    ensure_telemetry_cache();
    if (!telemetry_records) {
        *len = 0;
        if (timestamp) *timestamp = 0;
        return false;
    }

    for (int i = 0; i < telemetry_count; i++) {
        if (memcmp(telemetry_records[i].pub_key_prefix, pub_key_prefix, sizeof(telemetry_records[i].pub_key_prefix)) == 0) {
            *len = telemetry_records[i].len > out_size ? out_size : telemetry_records[i].len;
            memcpy(out, telemetry_records[i].data, *len);
            if (timestamp) *timestamp = telemetry_records[i].timestamp;
            return true;
        }
    }

    *len = 0;
    if (timestamp) *timestamp = 0;
    return false;
}

void clear_telemetry() {
    ensure_telemetry_cache();
    telemetry_count = 0;
    telemetry_dirty = false;
    if (telemetry_records) {
        memset(telemetry_records, 0, sizeof(TelemetryRecord) * MAX_TELEMETRY_RECORDS);
    }
    if (sd_available) {
        digitalWrite(BOARD_LORA_CS, HIGH);
        SD.remove(TELEMETRY_FILE);
        digitalWrite(BOARD_SD_CS, HIGH);
    }
}

} // namespace sd_log
