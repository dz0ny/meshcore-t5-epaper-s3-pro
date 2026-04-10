#include "sd_log.h"
#include "board.h"
#include <SD.h>

static const char* MSG_FILE = "/messages.bin";

// Track how many messages have been written to SD so we only append new ones.
static int flushed_count = 0;
static bool dirty = false;
static bool sd_available = false;

namespace sd_log {

void load() {
    // Deselect LoRa, select SD
    digitalWrite(BOARD_LORA_CS, HIGH);

    if (!SD.exists(MSG_FILE)) {
        sd_available = true;
        return;
    }

    File f = SD.open(MSG_FILE, FILE_READ);
    if (!f) return;

    sd_available = true;
    int count = 0;
    model::StoredMessage tmp;

    while (count < MAX_STORED_MESSAGES && f.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp)) {
        model::messages[count] = tmp;
        count++;
    }
    f.close();

    model::message_count = count;
    flushed_count = count;
    dirty = false;

    // Deselect SD when done
    digitalWrite(BOARD_SD_CS, HIGH);

    Serial.printf("SD_LOG: loaded %d messages from SD\n", count);
}

void mark_dirty() {
    dirty = true;
}

bool is_dirty() {
    return dirty;
}

void flush() {
    if (!dirty || !sd_available) return;

    // Deselect LoRa before touching SD
    digitalWrite(BOARD_LORA_CS, HIGH);

    // Deletion happened — rewrite entire file
    bool full_rewrite = (model::message_count < flushed_count);

    File f;
    if (flushed_count == 0 || full_rewrite) {
        f = SD.open(MSG_FILE, FILE_WRITE);  // truncate + write
    } else if (model::message_count > flushed_count) {
        f = SD.open(MSG_FILE, FILE_APPEND);
    } else {
        dirty = false;
        return;
    }

    if (!f) {
        sd_available = false;
        digitalWrite(BOARD_SD_CS, HIGH);
        Serial.println("SD_LOG: flush failed — SD not writable");
        return;
    }

    int start = full_rewrite ? 0 : flushed_count;
    int written = 0;
    for (int i = start; i < model::message_count; i++) {
        size_t n = f.write((const uint8_t*)&model::messages[i], sizeof(model::StoredMessage));
        if (n != sizeof(model::StoredMessage)) {
            Serial.println("SD_LOG: short write, stopping");
            break;
        }
        written++;
    }
    f.close();

    // Deselect SD, bus is free for LoRa again
    digitalWrite(BOARD_SD_CS, HIGH);

    flushed_count = full_rewrite ? written : (flushed_count + written);
    if (flushed_count >= model::message_count) {
        dirty = false;
    }

    Serial.printf("SD_LOG: flushed %d messages to SD (total %d)\n", written, flushed_count);
}

} // namespace sd_log
