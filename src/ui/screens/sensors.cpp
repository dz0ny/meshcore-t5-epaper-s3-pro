#include <Arduino.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <time.h>
#include "sensors.h"
#include "../ui_theme.h"
#include "../ui_port.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../components/text_utils.h"
#include "../components/toast.h"
#include "../../sd_log.h"
#include "../../mesh/mesh_bridge.h"
#include "../../mesh/mesh_task.h"
#include <helpers/AdvertDataHelpers.h>
#include <helpers/sensors/LPPDataHelpers.h>

namespace ui::screen::sensors {

static lv_obj_t* scr = NULL;
static lv_obj_t* sensor_list = NULL;
static lv_obj_t* empty_label = NULL;
static lv_timer_t* poll_timer = NULL;
static const uint32_t TELEMETRY_TIMEOUT_MS = 15000;
static int saved_refresh_mode = UI_REFRESH_MODE_NORMAL;
static bool refresh_mode_overridden = false;

struct SensorCard {
    char name[32];
    uint8_t pub_key[32];
    uint8_t type;
    bool pending;
    uint32_t pending_since_ms;
    uint32_t telemetry_timestamp;
    char last_telemetry[192];
    char telemetry[192];
};

static SensorCard cards[MAX_CONTACTS] = {};
static int card_count = 0;
static lv_obj_t* card_rows[MAX_CONTACTS] = {};
static lv_obj_t* card_name_labels[MAX_CONTACTS] = {};
static lv_obj_t* card_body_labels[MAX_CONTACTS] = {};
static lv_obj_t* card_state_pills[MAX_CONTACTS] = {};
static lv_obj_t* card_state_labels[MAX_CONTACTS] = {};
static bool row_visible[MAX_CONTACTS] = {};

static void decode_telemetry(SensorCard& card, const uint8_t* data, uint8_t len, uint32_t timestamp = 0);

static const uint8_t LPP_BINARY_BOOL = 143;
static const uint8_t LPP_BINARY_POWER_SWITCH = 144;
static const uint8_t LPP_BINARY_OPEN = 145;
static const uint8_t LPP_BINARY_BATTERY_LOW = 146;
static const uint8_t LPP_BINARY_CHARGING = 147;
static const uint8_t LPP_BINARY_CARBON_MONOXIDE = 148;
static const uint8_t LPP_BINARY_COLD = 149;
static const uint8_t LPP_BINARY_CONNECTIVITY = 150;
static const uint8_t LPP_BINARY_DOOR = 151;
static const uint8_t LPP_BINARY_GARAGE_DOOR = 152;
static const uint8_t LPP_BINARY_GAS = 153;
static const uint8_t LPP_BINARY_HEAT = 154;
static const uint8_t LPP_BINARY_LIGHT = 155;
static const uint8_t LPP_BINARY_LOCK = 156;
static const uint8_t LPP_BINARY_MOISTURE = 157;
static const uint8_t LPP_BINARY_MOTION = 158;
static const uint8_t LPP_BINARY_MOVING = 159;
static const uint8_t LPP_BINARY_OCCUPANCY = 160;
static const uint8_t LPP_BINARY_PLUG = 161;
static const uint8_t LPP_BINARY_PRESENCE = 162;
static const uint8_t LPP_BINARY_PROBLEM = 163;
static const uint8_t LPP_BINARY_RUNNING = 164;
static const uint8_t LPP_BINARY_SAFETY = 165;
static const uint8_t LPP_BINARY_SMOKE = 166;
static const uint8_t LPP_BINARY_SOUND = 167;
static const uint8_t LPP_BINARY_TAMPER = 168;
static const uint8_t LPP_BINARY_VIBRATION = 169;
static const uint8_t LPP_BINARY_WINDOW = 170;
static const uint8_t LPP_BUTTON_EVENT = 171;
static const uint8_t LPP_DIMMER = 172;
static const uint8_t LPP_SPEED = 129;
static const uint8_t LPP_UV = 173;
static const uint8_t LPP_LIGHT_LEVEL = 174;
static const uint8_t LPP_PM25 = 175;
static const uint8_t LPP_PM10 = 176;
static const uint8_t LPP_CO2 = 177;
static const uint8_t LPP_TVOC = 178;
static const uint8_t LPP_RPM = 179;
static const uint8_t LPP_CONDUCTIVITY = 180;
static const uint8_t LPP_ROTATION = 181;
static const uint8_t LPP_DURATION = 182;
static const uint8_t LPP_ACCELERATION = 183;
static const uint8_t LPP_GYRO_RATE = 184;
static const uint8_t LPP_VOLUME = 185;
static const uint8_t LPP_FLOW_RATE = 186;
static const uint8_t LPP_VOLUME_STORAGE = 187;
static const uint8_t LPP_WATER = 188;
static const uint8_t LPP_GAS_VOLUME = 189;
static const uint8_t LPP_MASS = 190;
static const uint8_t LPP_SIGNED_SPEED = 191;
static const uint8_t LPP_SIGNED_POWER = 192;
static const uint8_t LPP_SIGNED_CURRENT = 193;
static const uint8_t LPP_GUST = 137;
static const uint8_t LPP_DEW_POINT = 138;
static const uint8_t LPP_RAIN = 139;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static uint32_t read_u32_be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int32_t read_i24_be(const uint8_t* p) {
    int32_t v = ((int32_t)p[0] << 16) | ((int32_t)p[1] << 8) | (int32_t)p[2];
    if (v & 0x800000) v -= 0x1000000;
    return v;
}

static int32_t read_i32_be(const uint8_t* p) {
    uint32_t v = read_u32_be(p);
    return (v & 0x80000000U) ? (int32_t)(v - 0x100000000ULL) : (int32_t)v;
}

static int8_t read_i8(const uint8_t* p) {
    return (int8_t)p[0];
}

static const char* binary_label(uint8_t type) {
    switch (type) {
        case LPP_BINARY_POWER_SWITCH: return "Power switch";
        case LPP_BINARY_OPEN: return "Open";
        case LPP_BINARY_BATTERY_LOW: return "Battery low";
        case LPP_BINARY_CHARGING: return "Charging";
        case LPP_BINARY_CARBON_MONOXIDE: return "Carbon monoxide";
        case LPP_BINARY_COLD: return "Cold";
        case LPP_BINARY_CONNECTIVITY: return "Connectivity";
        case LPP_BINARY_DOOR: return "Door";
        case LPP_BINARY_GARAGE_DOOR: return "Garage door";
        case LPP_BINARY_GAS: return "Gas";
        case LPP_BINARY_HEAT: return "Heat";
        case LPP_BINARY_LIGHT: return "Light";
        case LPP_BINARY_LOCK: return "Lock";
        case LPP_BINARY_MOISTURE: return "Moisture";
        case LPP_BINARY_MOTION: return "Motion";
        case LPP_BINARY_MOVING: return "Moving";
        case LPP_BINARY_OCCUPANCY: return "Occupied";
        case LPP_BINARY_PLUG: return "Plug";
        case LPP_BINARY_PRESENCE: return "Presence";
        case LPP_BINARY_PROBLEM: return "Problem";
        case LPP_BINARY_RUNNING: return "Running";
        case LPP_BINARY_SAFETY: return "Safety";
        case LPP_BINARY_SMOKE: return "Smoke";
        case LPP_BINARY_SOUND: return "Sound";
        case LPP_BINARY_TAMPER: return "Tamper";
        case LPP_BINARY_VIBRATION: return "Vibration";
        case LPP_BINARY_WINDOW: return "Window";
        case LPP_BINARY_BOOL:
        default: return "Binary";
    }
}

static bool has_zero_tail(const uint8_t* data, uint8_t len, uint8_t pos) {
    if (!data || pos >= len) return false;
    for (uint8_t i = pos; i < len; i++) {
        if (data[i] != 0) return false;
    }
    return true;
}

static bool is_sensor_contact(uint8_t type, uint8_t flags) {
    (void)type;
    return (flags & 0x01) != 0;
}

static SensorCard* find_card(const uint8_t* pub_key_prefix) {
    if (!pub_key_prefix) return NULL;
    for (int i = 0; i < card_count; i++) {
        if (memcmp(cards[i].pub_key, pub_key_prefix, 7) == 0) {
            return &cards[i];
        }
    }
    return NULL;
}

static void format_default_body(SensorCard& card) {
    card.telemetry_timestamp = 0;
    card.pending_since_ms = 0;
    card.last_telemetry[0] = 0;
    snprintf(card.telemetry, sizeof(card.telemetry), "Hold to request telemetry");
}

static bool load_persisted_telemetry(SensorCard& card) {
    uint8_t data[96];
    uint8_t len = 0;
    uint32_t timestamp = 0;
    if (!sd_log::get_telemetry(card.pub_key, data, &len, sizeof(data), &timestamp)) return false;
    decode_telemetry(card, data, len, timestamp);
    return true;
}

static void update_card_row(int idx) {
    if (idx < 0 || idx >= card_count || !card_rows[idx]) return;

    const char* state_text = "READY";
    bool filled_pill = false;

    if (cards[idx].pending) {
        state_text = "WAITING";
        filled_pill = true;
    } else if (cards[idx].telemetry_timestamp != 0) {
        state_text = "CACHED";
    }

    lv_label_set_text(card_name_labels[idx], cards[idx].name);
    lv_label_set_text(card_body_labels[idx], cards[idx].telemetry);
    lv_label_set_text(card_state_labels[idx], state_text);
    lv_obj_set_style_bg_opa(card_state_pills[idx], filled_pill ? LV_OPA_COVER : LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_text_color(card_state_labels[idx],
                                lv_color_hex(filled_pill ? EPD_COLOR_BG : EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_width(card_rows[idx], cards[idx].pending ? 4 : 3, LV_PART_MAIN);
    if (!row_visible[idx]) {
        lv_obj_clear_flag(card_rows[idx], LV_OBJ_FLAG_HIDDEN);
        row_visible[idx] = true;
    }
}

static void rebuild_list() {
    int shown = 0;

    for (int i = 0; i < card_count; i++) {
        update_card_row(i);
        shown++;
    }

    for (int i = shown; i < MAX_CONTACTS; i++) {
        if (card_rows[i] && row_visible[i]) {
            lv_obj_add_flag(card_rows[i], LV_OBJ_FLAG_HIDDEN);
            row_visible[i] = false;
        }
    }

    if (empty_label) {
        if (shown == 0) {
            lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

}

static void append_timestamp(char* out, size_t out_size, int& used, uint32_t timestamp) {
    if (timestamp == 0 || used >= (int)out_size - 1) return;

    time_t raw = (time_t)timestamp;
    struct tm local_tm;
    localtime_r(&raw, &local_tm);

    char line[48];
    snprintf(line, sizeof(line), "Updated %02d/%02d %02d:%02d",
             local_tm.tm_mday, local_tm.tm_mon + 1, local_tm.tm_hour, local_tm.tm_min);
    used += snprintf(out + used, out_size - used, used == 0 ? "%s" : "\n%s", line);
}

static void decode_telemetry(SensorCard& card, const uint8_t* data, uint8_t len, uint32_t timestamp) {
    card.telemetry_timestamp = timestamp;
    card.pending_since_ms = 0;
    if (!data || len == 0) {
        int used = snprintf(card.telemetry, sizeof(card.telemetry), "No telemetry data");
        append_timestamp(card.telemetry, sizeof(card.telemetry), used, timestamp);
        strncpy(card.last_telemetry, card.telemetry, sizeof(card.last_telemetry) - 1);
        card.last_telemetry[sizeof(card.last_telemetry) - 1] = 0;
        return;
    }

    char out[sizeof(card.telemetry)] = {};
    int used = 0;
    int lines = 0;
    bool saw_known = false;
    bool saw_non_self_channel = false;
    LPPReader reader(data, len);
    uint8_t channel;
    uint8_t type;
    uint8_t pos = 0;

    while (reader.readHeader(channel, type) && used < (int)sizeof(out) - 1 && lines < 8) {
        if (channel != 1) saw_non_self_channel = true;
        pos += 2;
        if (lines > 0 && has_zero_tail(data, len, pos - 2)) break;
        char line[48] = {};
        bool handled = true;

        switch (type) {
            case LPP_DIGITAL_INPUT: {
                if (pos + 1 > len) { handled = false; break; }
                uint8_t value = data[pos++];
                snprintf(line, sizeof(line), "Digital input %u: %u", channel, value);
                break;
            }
            case LPP_DIGITAL_OUTPUT: {
                if (pos + 1 > len) { handled = false; break; }
                uint8_t value = data[pos++];
                snprintf(line, sizeof(line), "Digital output %u: %u", channel, value);
                break;
            }
            case LPP_ANALOG_INPUT: {
                if (pos + 2 > len) { handled = false; break; }
                float value = (int16_t)(((uint16_t)data[pos] << 8) | data[pos + 1]) / 100.0f;
                pos += 2;
                if (channel == 0) {
                    snprintf(line, sizeof(line), "Battery %.2f V", value);
                } else if (channel == 1 && !saw_non_self_channel) {
                    snprintf(line, sizeof(line), "Voltage %.2f V", value);
                } else {
                    snprintf(line, sizeof(line), "Analog input %u: %.2f", channel, value);
                }
                break;
            }
            case LPP_ANALOG_OUTPUT: {
                if (pos + 2 > len) { handled = false; break; }
                float value = (int16_t)(((uint16_t)data[pos] << 8) | data[pos + 1]) / 100.0f;
                pos += 2;
                snprintf(line, sizeof(line), "Analog output %u: %.2f", channel, value);
                break;
            }
            case LPP_VOLTAGE: {
                float value;
                handled = reader.readVoltage(value);
                pos += 2;
                if (handled) {
                    if (channel == 0) {
                        snprintf(line, sizeof(line), "Battery %.2f V", value);
                    } else if (channel == 1 && !saw_non_self_channel) {
                        snprintf(line, sizeof(line), "Voltage %.2f V", value);
                    } else {
                        snprintf(line, sizeof(line), "Voltage %u: %.2f V", channel, value);
                    }
                }
                break;
            }
            case LPP_TEMPERATURE: {
                float value;
                handled = reader.readTemperature(value);
                pos += 2;
                if (handled) {
                    if (channel == 1 || !saw_non_self_channel) {
                        snprintf(line, sizeof(line), "Temperature %.1f C", value);
                    } else {
                        snprintf(line, sizeof(line), "Temperature %u: %.1f C", channel, value);
                    }
                }
                break;
            }
            case LPP_RELATIVE_HUMIDITY: {
                float value;
                handled = reader.readRelativeHumidity(value);
                pos += 1;
                if (handled) {
                    if (channel == 1 || !saw_non_self_channel) {
                        snprintf(line, sizeof(line), "Humidity %.0f%%", value);
                    } else {
                        snprintf(line, sizeof(line), "Humidity %u: %.0f%%", channel, value);
                    }
                }
                break;
            }
            case LPP_BAROMETRIC_PRESSURE: {
                float value;
                handled = reader.readPressure(value);
                pos += 2;
                if (handled) {
                    if (channel == 1 || !saw_non_self_channel) {
                        snprintf(line, sizeof(line), "Pressure %.1f hPa", value);
                    } else {
                        snprintf(line, sizeof(line), "Pressure %u: %.1f hPa", channel, value);
                    }
                }
                break;
            }
            case LPP_ALTITUDE: {
                float value;
                handled = reader.readAltitude(value);
                pos += 2;
                if (handled) snprintf(line, sizeof(line), "Altitude %.0f m", value);
                break;
            }
            case LPP_CURRENT: {
                float value;
                handled = reader.readCurrent(value);
                pos += 2;
                if (handled) snprintf(line, sizeof(line), "Current %.3f A", value);
                break;
            }
            case LPP_POWER: {
                float value;
                handled = reader.readPower(value);
                pos += 2;
                if (handled) snprintf(line, sizeof(line), "Power %.0f W", value);
                break;
            }
            case LPP_GPS: {
                if (pos + 9 > len) { handled = false; break; }
                float lat = read_i24_be(&data[pos]) / 10000.0f;
                float lon = read_i24_be(&data[pos + 3]) / 10000.0f;
                pos += 9;
                snprintf(line, sizeof(line), "GPS %.4f %.4f", lat, lon);
                break;
            }
            case LPP_PERCENTAGE: {
                if (pos + 1 > len) { handled = false; break; }
                float value = data[pos];
                pos += 1;
                if (channel == 0 || (channel == 1 && !saw_non_self_channel)) {
                    snprintf(line, sizeof(line), "Battery %.0f%%", value);
                } else {
                    snprintf(line, sizeof(line), "Percentage %u: %.0f%%", channel, value);
                }
                break;
            }
            case LPP_LUMINOSITY: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "Light %.0f lx", value);
                break;
            }
            case LPP_GENERIC_SENSOR: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]);
                pos += 4;
                snprintf(line, sizeof(line), "Generic sensor %.0f", value);
                break;
            }
            case LPP_PRESENCE: {
                if (pos + 1 > len) { handled = false; break; }
                uint8_t value = data[pos++];
                snprintf(line, sizeof(line), "Presence %s", value ? "Yes" : "No");
                break;
            }
            case LPP_ACCELEROMETER: {
                if (pos + 6 > len) { handled = false; break; }
                float x = (int16_t)(((uint16_t)data[pos] << 8) | data[pos + 1]) / 1000.0f;
                float y = (int16_t)(((uint16_t)data[pos + 2] << 8) | data[pos + 3]) / 1000.0f;
                float z = (int16_t)(((uint16_t)data[pos + 4] << 8) | data[pos + 5]) / 1000.0f;
                pos += 6;
                snprintf(line, sizeof(line), "Accelerometer %.2f %.2f %.2f", x, y, z);
                break;
            }
            case LPP_CONCENTRATION: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "Concentration %.0f ppm", value);
                break;
            }
            case LPP_DISTANCE: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Distance %.2f m", value);
                break;
            }
            case LPP_SPEED: {
                if (pos + 2 > len) { handled = false; break; }
                float value = (((uint16_t)data[pos] << 8) | data[pos + 1]) / 100.0f;
                pos += 2;
                snprintf(line, sizeof(line), "Wind %.2f m/s", value);
                break;
            }
            case LPP_ENERGY: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Energy %.2f kWh", value);
                break;
            }
            case LPP_DIRECTION: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "Direction %.0f deg", value);
                break;
            }
            case LPP_FREQUENCY: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]);
                pos += 4;
                snprintf(line, sizeof(line), "Frequency %.0f Hz", value);
                break;
            }
            case LPP_UNIXTIME: {
                if (pos + 4 > len) { handled = false; break; }
                uint32_t value = read_u32_be(&data[pos]);
                pos += 4;
                snprintf(line, sizeof(line), "Unix time %lu", (unsigned long)value);
                break;
            }
            case LPP_GYROMETER: {
                if (pos + 6 > len) { handled = false; break; }
                float x = (int16_t)(((uint16_t)data[pos] << 8) | data[pos + 1]) / 100.0f;
                float y = (int16_t)(((uint16_t)data[pos + 2] << 8) | data[pos + 3]) / 100.0f;
                float z = (int16_t)(((uint16_t)data[pos + 4] << 8) | data[pos + 5]) / 100.0f;
                pos += 6;
                snprintf(line, sizeof(line), "Gyrometer %.1f %.1f %.1f", x, y, z);
                break;
            }
            case LPP_COLOUR: {
                if (pos + 3 > len) { handled = false; break; }
                uint8_t r = data[pos];
                uint8_t g = data[pos + 1];
                uint8_t b = data[pos + 2];
                pos += 3;
                snprintf(line, sizeof(line), "Color R%u G%u B%u", r, g, b);
                break;
            }
            case LPP_SWITCH:
            case LPP_BINARY_BOOL: {
                if (pos + 1 > len) { handled = false; break; }
                uint8_t value = data[pos++];
                snprintf(line, sizeof(line), "Binary %s", value ? "On" : "Off");
                break;
            }
            case LPP_BINARY_POWER_SWITCH:
            case LPP_BINARY_OPEN:
            case LPP_BINARY_BATTERY_LOW:
            case LPP_BINARY_CHARGING:
            case LPP_BINARY_CARBON_MONOXIDE:
            case LPP_BINARY_COLD:
            case LPP_BINARY_CONNECTIVITY:
            case LPP_BINARY_DOOR:
            case LPP_BINARY_GARAGE_DOOR:
            case LPP_BINARY_GAS:
            case LPP_BINARY_HEAT:
            case LPP_BINARY_LIGHT:
            case LPP_BINARY_LOCK:
            case LPP_BINARY_MOISTURE:
            case LPP_BINARY_MOTION:
            case LPP_BINARY_MOVING:
            case LPP_BINARY_OCCUPANCY:
            case LPP_BINARY_PLUG:
            case LPP_BINARY_PRESENCE:
            case LPP_BINARY_PROBLEM:
            case LPP_BINARY_RUNNING:
            case LPP_BINARY_SAFETY:
            case LPP_BINARY_SMOKE:
            case LPP_BINARY_SOUND:
            case LPP_BINARY_TAMPER:
            case LPP_BINARY_VIBRATION:
            case LPP_BINARY_WINDOW: {
                if (pos + 1 > len) { handled = false; break; }
                uint8_t value = data[pos++];
                snprintf(line, sizeof(line), "%s %s", binary_label(type), value ? "On" : "Off");
                break;
            }
            case LPP_BUTTON_EVENT: {
                if (pos + 1 > len) { handled = false; break; }
                uint8_t value = data[pos++];
                snprintf(line, sizeof(line), "Button event %u", value);
                break;
            }
            case LPP_DIMMER: {
                if (pos + 1 > len) { handled = false; break; }
                int value = read_i8(&data[pos]);
                pos += 1;
                snprintf(line, sizeof(line), "Dimmer %d%%", value);
                break;
            }
            case LPP_UV: {
                if (pos + 1 > len) { handled = false; break; }
                float value = data[pos++] / 10.0f;
                snprintf(line, sizeof(line), "UV %.1f", value);
                break;
            }
            case LPP_GUST: {
                if (pos + 2 > len) { handled = false; break; }
                float value = (((uint16_t)data[pos] << 8) | data[pos + 1]) / 100.0f;
                pos += 2;
                snprintf(line, sizeof(line), "Gust %.2f m/s", value);
                break;
            }
            case LPP_DEW_POINT: {
                if (pos + 2 > len) { handled = false; break; }
                float value = (int16_t)(((uint16_t)data[pos] << 8) | data[pos + 1]) / 10.0f;
                pos += 2;
                snprintf(line, sizeof(line), "Dew point %.1f C", value);
                break;
            }
            case LPP_RAIN: {
                if (pos + 2 > len) { handled = false; break; }
                float value = (((uint16_t)data[pos] << 8) | data[pos + 1]) / 10.0f;
                pos += 2;
                snprintf(line, sizeof(line), "Rain %.1f mm", value);
                break;
            }
            case LPP_LIGHT_LEVEL: {
                if (pos + 1 > len) { handled = false; break; }
                uint8_t value = data[pos++];
                snprintf(line, sizeof(line), "Light %u", value);
                break;
            }
            case LPP_PM25: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "PM2.5 %.0f", value);
                break;
            }
            case LPP_PM10: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "PM10 %.0f", value);
                break;
            }
            case LPP_CO2: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "CO2 %.0f ppm", value);
                break;
            }
            case LPP_TVOC: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "TVOC %.0f ppb", value);
                break;
            }
            case LPP_RPM: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "RPM %.0f", value);
                break;
            }
            case LPP_CONDUCTIVITY: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "Conductivity %.0f uS/cm", value);
                break;
            }
            case LPP_ROTATION: {
                if (pos + 2 > len) { handled = false; break; }
                float value = (int16_t)(((uint16_t)data[pos] << 8) | data[pos + 1]) / 10.0f;
                pos += 2;
                snprintf(line, sizeof(line), "Rotation %.1f deg", value);
                break;
            }
            case LPP_DURATION: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Duration %.1f s", value);
                break;
            }
            case LPP_ACCELERATION: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_i32_be(&data[pos]) / 1000000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Acceleration %.3f m/s2", value);
                break;
            }
            case LPP_GYRO_RATE: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_i32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Gyro rate %.2f deg/s", value);
                break;
            }
            case LPP_FLOW_RATE: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Flow rate %.2f", value);
                break;
            }
            case LPP_VOLUME: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Volume %.2f L", value);
                break;
            }
            case LPP_VOLUME_STORAGE: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Storage volume %.2f L", value);
                break;
            }
            case LPP_WATER: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Water %.2f L", value);
                break;
            }
            case LPP_GAS_VOLUME: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Gas volume %.2f L", value);
                break;
            }
            case LPP_MASS: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Mass %.2f kg", value);
                break;
            }
            case LPP_SIGNED_SPEED: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_i32_be(&data[pos]) / 1000000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Signed speed %.3f m/s", value);
                break;
            }
            case LPP_SIGNED_POWER: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_i32_be(&data[pos]) / 100.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Signed power %.2f W", value);
                break;
            }
            case LPP_SIGNED_CURRENT: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_i32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Signed current %.3f A", value);
                break;
            }
            default:
                reader.skipData(type);
                if (type == LPP_FREQUENCY || type == LPP_DISTANCE || type == LPP_ENERGY || type == LPP_UNIXTIME ||
                    type == LPP_DURATION || type == LPP_ACCELERATION || type == LPP_GYRO_RATE || type == LPP_FLOW_RATE ||
                    type == LPP_GENERIC_SENSOR || type == LPP_VOLUME || type == LPP_VOLUME_STORAGE || type == LPP_WATER ||
                    type == LPP_GAS_VOLUME || type == LPP_MASS || type == LPP_SIGNED_SPEED || type == LPP_SIGNED_POWER ||
                    type == LPP_SIGNED_CURRENT) {
                    pos += 4;
                } else if (type == LPP_SWITCH || type == LPP_BINARY_BOOL || type == LPP_UV || type == LPP_LIGHT_LEVEL ||
                           type == LPP_PERCENTAGE || type == LPP_DIGITAL_INPUT || type == LPP_DIGITAL_OUTPUT || type == LPP_PRESENCE ||
                           type == LPP_BUTTON_EVENT || type == LPP_DIMMER ||
                           (type >= LPP_BINARY_BOOL && type <= LPP_BINARY_WINDOW)) {
                    pos += 1;
                } else if (type == LPP_LUMINOSITY || type == LPP_CONCENTRATION || type == LPP_DIRECTION || type == LPP_PM25 ||
                           type == LPP_PM10 || type == LPP_CO2 || type == LPP_TVOC || type == LPP_RPM || type == LPP_CONDUCTIVITY ||
                           type == LPP_SPEED || type == LPP_GUST || type == LPP_DEW_POINT || type == LPP_RAIN ||
                           type == LPP_ANALOG_INPUT || type == LPP_ANALOG_OUTPUT || type == LPP_ALTITUDE || type == LPP_POWER ||
                           type == LPP_CURRENT || type == LPP_ROTATION || type == LPP_TEMPERATURE || type == LPP_BAROMETRIC_PRESSURE ||
                           type == LPP_VOLTAGE) {
                    pos += 2;
                } else if (type == LPP_ACCELEROMETER || type == LPP_GYROMETER) {
                    pos += 6;
                } else if (type == LPP_GPS) {
                    pos += 9;
                } else if (type == LPP_COLOUR) {
                    pos += 3;
                }
                handled = false;
                break;
        }

        if (handled && line[0] != 0) {
            saw_known = true;
            used += snprintf(out + used, sizeof(out) - used, used == 0 ? "%s" : "\n%s", line);
            lines++;
        }
    }

    if (!saw_known) {
        int used = snprintf(card.telemetry, sizeof(card.telemetry), "Telemetry updated");
        append_timestamp(card.telemetry, sizeof(card.telemetry), used, timestamp);
        return;
    }

    append_timestamp(out, sizeof(out), used, timestamp);
    strncpy(card.telemetry, out, sizeof(card.telemetry) - 1);
    card.telemetry[sizeof(card.telemetry) - 1] = 0;
    strncpy(card.last_telemetry, card.telemetry, sizeof(card.last_telemetry) - 1);
    card.last_telemetry[sizeof(card.last_telemetry) - 1] = 0;
}

static void on_card_click(lv_event_t* e) {
    int row_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (row_idx < 0 || row_idx >= card_count) return;

    if (!mesh::task::request_telemetry(cards[row_idx].pub_key)) {
        ui::toast::show("Telemetry request failed");
        return;
    }

    cards[row_idx].pending = true;
    cards[row_idx].pending_since_ms = millis();
    if (cards[row_idx].last_telemetry[0] != 0) {
        snprintf(cards[row_idx].telemetry, sizeof(cards[row_idx].telemetry), "%s\nRefreshing...", cards[row_idx].last_telemetry);
    } else {
        snprintf(cards[row_idx].telemetry, sizeof(cards[row_idx].telemetry), "Requesting telemetry...");
    }
    update_card_row(row_idx);
    ui::toast::show("Telemetry requested");
}

static void ensure_row(int idx) {
    if (!sensor_list || idx < 0 || idx >= MAX_CONTACTS || card_rows[idx]) return;

    lv_obj_t* row = lv_obj_create(sensor_list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(row, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_row(row, 14, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, on_card_click, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)idx);
    lv_obj_set_ext_click_area(row, 10);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* header = lv_obj_create(row);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(header, 10, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* name = lv_label_create(header);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(name, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_width(name, lv_pct(72));
    lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);

    lv_obj_t* pill = lv_obj_create(header);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(pill, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(pill, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_width(pill, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(pill, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(pill, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(pill, 6, LV_PART_MAIN);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* state = lv_label_create(pill);
    lv_obj_set_style_text_font(state, &lv_font_noto_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(state, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_label_set_text(state, "READY");

    lv_obj_t* body = lv_label_create(row);
    lv_obj_set_style_text_font(body, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(body, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_width(body, lv_pct(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);

    card_rows[idx] = row;
    card_name_labels[idx] = name;
    card_body_labels[idx] = body;
    card_state_pills[idx] = pill;
    card_state_labels[idx] = state;
    row_visible[idx] = false;
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
}

static void upsert_contact(const mesh::bridge::ContactUpdate& cu) {
    if (!is_sensor_contact(cu.type, cu.flags)) return;

    SensorCard* existing = find_card(cu.pub_key);
    if (existing) {
        strncpy(existing->name, cu.name, sizeof(existing->name) - 1);
        existing->name[sizeof(existing->name) - 1] = 0;
        ui::text::strip_emoji(existing->name);
        existing->type = cu.type;
        if (!existing->pending && !load_persisted_telemetry(*existing)) {
            format_default_body(*existing);
        }
        return;
    }

    if (card_count >= MAX_CONTACTS) return;

    SensorCard& card = cards[card_count];
    memset(&card, 0, sizeof(card));
    strncpy(card.name, cu.name, sizeof(card.name) - 1);
    card.name[sizeof(card.name) - 1] = 0;
    ui::text::strip_emoji(card.name);
    memcpy(card.pub_key, cu.pub_key, sizeof(card.pub_key));
    card.type = cu.type;
    if (!load_persisted_telemetry(card)) {
        format_default_body(card);
    }
    card_count++;
}

static void poll_updates(lv_timer_t* t) {
    bool changed = false;
    mesh::bridge::ContactUpdate cu;
    while (mesh::bridge::pop_contact(cu)) {
        upsert_contact(cu);
        changed = true;
    }

    mesh::bridge::TelemetryResponse tr;
    while (mesh::bridge::pop_telemetry(tr)) {
        SensorCard* card = find_card(tr.pub_key_prefix);
        if (!card) continue;
        card->pending = false;
        uint32_t timestamp = (uint32_t)time(nullptr);
        decode_telemetry(*card, tr.data, tr.len, timestamp);
        sd_log::store_telemetry(tr.pub_key_prefix, tr.data, tr.len);
        changed = true;
    }

    uint32_t now = millis();
    for (int i = 0; i < card_count; i++) {
        if (!cards[i].pending || cards[i].pending_since_ms == 0) continue;
        if ((uint32_t)(now - cards[i].pending_since_ms) < TELEMETRY_TIMEOUT_MS) continue;
        cards[i].pending = false;
        cards[i].pending_since_ms = 0;
        if (cards[i].last_telemetry[0] != 0) {
            snprintf(cards[i].telemetry, sizeof(cards[i].telemetry), "%s\nRequest timed out", cards[i].last_telemetry);
        } else {
            snprintf(cards[i].telemetry, sizeof(cards[i].telemetry), "Telemetry request timed out");
        }
        changed = true;
    }

    if (changed) rebuild_list();
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Sensors", on_back);

    sensor_list = ui::nav::scroll_list(parent);
    lv_obj_set_height(sensor_list, lv_pct(82));

    for (int i = 0; i < MAX_CONTACTS; i++) {
        ensure_row(i);
    }

    empty_label = lv_label_create(sensor_list);
    lv_obj_set_width(empty_label, lv_pct(100));
    lv_obj_set_flex_grow(empty_label, 1);
    lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(empty_label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(empty_label, "\n\nNo favorite nodes yet");
    lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
}

static void entry() {
    if (!refresh_mode_overridden) {
        saved_refresh_mode = ui::port::get_refresh_mode();
        refresh_mode_overridden = true;
    }
    ui::port::set_refresh_mode(UI_REFRESH_MODE_NORMAL);
    card_count = 0;
    mesh::task::push_all_contacts();
    rebuild_list();
    poll_timer = lv_timer_create(poll_updates, 500, NULL);
    poll_updates(NULL);
}

static void exit_fn() {
    if (poll_timer) { lv_timer_del(poll_timer); poll_timer = NULL; }
    if (refresh_mode_overridden) {
        ui::port::set_refresh_mode(saved_refresh_mode);
        refresh_mode_overridden = false;
    }
}

static void destroy() {
    scr = NULL;
    sensor_list = NULL;
    empty_label = NULL;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        memset(&cards[i], 0, sizeof(cards[i]));
        card_rows[i] = NULL;
        card_name_labels[i] = NULL;
        card_body_labels[i] = NULL;
        card_state_pills[i] = NULL;
        card_state_labels[i] = NULL;
        row_visible[i] = false;
    }
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::sensors
