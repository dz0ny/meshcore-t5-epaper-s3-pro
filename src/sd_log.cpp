#include "sd_log.h"
#include "board.h"
#include <esp_heap_caps.h>
#include <SD.h>
#include <cstring>
#include <time.h>

static const char* MSG_FILE = "/messages.bin";
static const char* TELEMETRY_FILE = "/telemetry.bin";
static const char* PING_FILE = "/ping.bin";
static const int MAX_TELEMETRY_RECORDS = 32;
static const int MAX_PING_RECORDS = 32;
static const int MAX_PING_HISTORY = 24;

struct TelemetryRecord {
    uint8_t pub_key_prefix[7];
    uint8_t data[96];
    uint8_t len;
    uint32_t timestamp;
};

struct PingRecord {
    uint8_t pub_key_prefix[7];
    uint8_t count;
    sd_log::PingHistoryEntry entries[MAX_PING_HISTORY];
};

// Track how many messages have been written to SD so we only append new ones.
static int flushed_count = 0;
static bool dirty = false;
static bool telemetry_dirty = false;
static bool ping_dirty = false;
static bool sd_available = false;
static TelemetryRecord* telemetry_records = nullptr;
static int telemetry_count = 0;
static PingRecord* ping_records = nullptr;
static int ping_count = 0;

static void ensure_telemetry_cache() {
    if (!telemetry_records) {
        telemetry_records = (TelemetryRecord*)heap_caps_calloc(
            MAX_TELEMETRY_RECORDS, sizeof(TelemetryRecord), MALLOC_CAP_SPIRAM);
    }
}

static void ensure_ping_cache() {
    if (!ping_records) {
        ping_records = (PingRecord*)heap_caps_calloc(
            MAX_PING_RECORDS, sizeof(PingRecord), MALLOC_CAP_SPIRAM);
    }
}

namespace sd_log {

void load() {
    ensure_telemetry_cache();
    ensure_ping_cache();

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

    ping_count = 0;
    if (ping_records && SD.exists(PING_FILE)) {
        File pf = SD.open(PING_FILE, FILE_READ);
        if (pf) {
            PingRecord record;
            while (ping_count < MAX_PING_RECORDS &&
                   pf.read((uint8_t*)&record, sizeof(record)) == sizeof(record)) {
                ping_records[ping_count] = record;
                if (ping_records[ping_count].count > MAX_PING_HISTORY) {
                    ping_records[ping_count].count = MAX_PING_HISTORY;
                }
                ping_count++;
            }
            pf.close();
        }
    }
    ping_dirty = false;

    // Deselect SD when done
    digitalWrite(BOARD_SD_CS, HIGH);

    Serial.printf("SD_LOG: loaded %d messages, %d telemetry snapshots and %d ping histories from SD\n",
                  count, telemetry_count, ping_count);
}

void mark_dirty() {
    dirty = true;
}

bool is_dirty() {
    return dirty || telemetry_dirty || ping_dirty;
}

void flush() {
    if ((!dirty && !telemetry_dirty && !ping_dirty) || !sd_available) return;

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

    if (ping_dirty) {
        SD.remove(PING_FILE);
        File pf = SD.open(PING_FILE, FILE_WRITE);
        if (!pf) {
            sd_available = false;
            digitalWrite(BOARD_SD_CS, HIGH);
            Serial.println("SD_LOG: ping flush failed — SD not writable");
            return;
        }

        for (int i = 0; i < ping_count; i++) {
            size_t n = pf.write((const uint8_t*)&ping_records[i], sizeof(PingRecord));
            if (n != sizeof(PingRecord)) {
                Serial.println("SD_LOG: ping short write, stopping");
                break;
            }
        }
        pf.close();
        ping_dirty = false;
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

void store_ping_history(const uint8_t* pub_key_prefix, const PingHistoryEntry* entries, uint8_t count) {
    if (!pub_key_prefix) return;
    ensure_ping_cache();
    if (!ping_records) return;

    uint8_t clamped_count = count > MAX_PING_HISTORY ? MAX_PING_HISTORY : count;
    for (int i = 0; i < ping_count; i++) {
        if (memcmp(ping_records[i].pub_key_prefix, pub_key_prefix, sizeof(ping_records[i].pub_key_prefix)) == 0) {
            ping_records[i].count = clamped_count;
            if (clamped_count > 0 && entries) {
                memcpy(ping_records[i].entries, entries, sizeof(PingHistoryEntry) * clamped_count);
            }
            if (clamped_count < MAX_PING_HISTORY) {
                memset(&ping_records[i].entries[clamped_count], 0,
                       sizeof(PingHistoryEntry) * (MAX_PING_HISTORY - clamped_count));
            }
            ping_dirty = true;
            return;
        }
    }

    int idx = ping_count < MAX_PING_RECORDS ? ping_count : (MAX_PING_RECORDS - 1);
    if (ping_count < MAX_PING_RECORDS) {
        ping_count++;
    }
    memcpy(ping_records[idx].pub_key_prefix, pub_key_prefix, sizeof(ping_records[idx].pub_key_prefix));
    ping_records[idx].count = clamped_count;
    memset(ping_records[idx].entries, 0, sizeof(ping_records[idx].entries));
    if (clamped_count > 0 && entries) {
        memcpy(ping_records[idx].entries, entries, sizeof(PingHistoryEntry) * clamped_count);
    }
    ping_dirty = true;
}

bool get_ping_history(const uint8_t* pub_key_prefix, PingHistoryEntry* out, uint8_t* count, size_t max_entries) {
    if (!pub_key_prefix || !out || !count || max_entries == 0) return false;
    ensure_ping_cache();
    if (!ping_records) {
        *count = 0;
        return false;
    }

    for (int i = 0; i < ping_count; i++) {
        if (memcmp(ping_records[i].pub_key_prefix, pub_key_prefix, sizeof(ping_records[i].pub_key_prefix)) == 0) {
            *count = ping_records[i].count > max_entries ? max_entries : ping_records[i].count;
            memcpy(out, ping_records[i].entries, sizeof(PingHistoryEntry) * (*count));
            return true;
        }
    }

    *count = 0;
    return false;
}

void clear_ping_history() {
    ensure_ping_cache();
    ping_count = 0;
    ping_dirty = false;
    if (ping_records) {
        memset(ping_records, 0, sizeof(PingRecord) * MAX_PING_RECORDS);
    }
    if (sd_available) {
        digitalWrite(BOARD_LORA_CS, HIGH);
        SD.remove(PING_FILE);
        digitalWrite(BOARD_SD_CS, HIGH);
    }
}

} // namespace sd_log
