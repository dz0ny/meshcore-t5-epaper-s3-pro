#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <time.h>
#include "sensors.h"
#include "../ui_theme.h"
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
static bool row_visible[MAX_CONTACTS] = {};

static void decode_telemetry(SensorCard& card, const uint8_t* data, uint8_t len, uint32_t timestamp = 0);

static const uint8_t LPP_BINARY_BOOL = 143;
static const uint8_t LPP_SPEED = 129;
static const uint8_t LPP_UV = 173;
static const uint8_t LPP_LIGHT_LEVEL = 174;
static const uint8_t LPP_PM25 = 175;
static const uint8_t LPP_PM10 = 176;
static const uint8_t LPP_CO2 = 177;
static const uint8_t LPP_TVOC = 178;
static const uint8_t LPP_RPM = 179;
static const uint8_t LPP_CONDUCTIVITY = 180;
static const uint8_t LPP_DURATION = 182;
static const uint8_t LPP_ACCELERATION = 183;
static const uint8_t LPP_GYRO_RATE = 184;
static const uint8_t LPP_FLOW_RATE = 186;
static const uint8_t LPP_GUST = 137;
static const uint8_t LPP_DEW_POINT = 138;
static const uint8_t LPP_RAIN = 139;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static uint32_t read_u32_be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int32_t read_i32_be(const uint8_t* p) {
    uint32_t v = read_u32_be(p);
    return (v & 0x80000000U) ? (int32_t)(v - 0x100000000ULL) : (int32_t)v;
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
    snprintf(card.telemetry, sizeof(card.telemetry), "Tap to request telemetry");
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
    lv_label_set_text(card_name_labels[idx], cards[idx].name);
    lv_label_set_text(card_body_labels[idx], cards[idx].telemetry);
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
    LPPReader reader(data, len);
    uint8_t channel;
    uint8_t type;
    uint8_t pos = 0;

    while (reader.readHeader(channel, type) && used < (int)sizeof(out) - 1 && lines < 5) {
        pos += 2;
        if (lines > 0 && has_zero_tail(data, len, pos - 2)) break;
        char line[48] = {};
        bool handled = true;

        switch (type) {
            case LPP_VOLTAGE: {
                float value;
                handled = reader.readVoltage(value);
                pos += 2;
                if (handled) snprintf(line, sizeof(line), "Battery %.2f V", value);
                break;
            }
            case LPP_TEMPERATURE: {
                float value;
                handled = reader.readTemperature(value);
                pos += 2;
                if (handled) snprintf(line, sizeof(line), "Temp %.1f C", value);
                break;
            }
            case LPP_RELATIVE_HUMIDITY: {
                float value;
                handled = reader.readRelativeHumidity(value);
                pos += 1;
                if (handled) snprintf(line, sizeof(line), "Humidity %.0f%%", value);
                break;
            }
            case LPP_BAROMETRIC_PRESSURE: {
                float value;
                handled = reader.readPressure(value);
                pos += 2;
                if (handled) snprintf(line, sizeof(line), "Pressure %.1f hPa", value);
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
                float lat;
                float lon;
                float alt;
                handled = reader.readGPS(lat, lon, alt);
                pos += 9;
                if (handled) snprintf(line, sizeof(line), "GPS %.4f %.4f", lat, lon);
                break;
            }
            case LPP_PERCENTAGE: {
                if (pos + 1 > len) { handled = false; break; }
                float value = data[pos];
                pos += 1;
                snprintf(line, sizeof(line), "Level %.0f%%", value);
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
                snprintf(line, sizeof(line), "Sensor %.0f", value);
                break;
            }
            case LPP_CONCENTRATION: {
                if (pos + 2 > len) { handled = false; break; }
                float value = ((uint16_t)data[pos] << 8) | data[pos + 1];
                pos += 2;
                snprintf(line, sizeof(line), "Conc %.0f ppm", value);
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
            case LPP_UNIXTIME: {
                if (pos + 4 > len) { handled = false; break; }
                uint32_t value = read_u32_be(&data[pos]);
                pos += 4;
                snprintf(line, sizeof(line), "Unix %lu", (unsigned long)value);
                break;
            }
            case LPP_SWITCH:
            case LPP_BINARY_BOOL: {
                if (pos + 1 > len) { handled = false; break; }
                uint8_t value = data[pos++];
                snprintf(line, sizeof(line), "State %s", value ? "On" : "Off");
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
                snprintf(line, sizeof(line), "Dew %.1f C", value);
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
                snprintf(line, sizeof(line), "TVOC %.0f", value);
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
                snprintf(line, sizeof(line), "Cond %.0f", value);
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
                snprintf(line, sizeof(line), "Accel %.3f", value);
                break;
            }
            case LPP_GYRO_RATE: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_i32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Gyro %.2f", value);
                break;
            }
            case LPP_FLOW_RATE: {
                if (pos + 4 > len) { handled = false; break; }
                float value = read_u32_be(&data[pos]) / 1000.0f;
                pos += 4;
                snprintf(line, sizeof(line), "Flow %.2f", value);
                break;
            }
            default:
                reader.skipData(type);
                if (type == LPP_FREQUENCY || type == 131 || type == 130 || type == 133 || type == 183 || type == 184 || type == 186 || type == LPP_GENERIC_SENSOR) {
                    pos += 4;
                } else if (type == LPP_SWITCH || type == LPP_BINARY_BOOL || type == LPP_UV || type == LPP_LIGHT_LEVEL || type == LPP_PERCENTAGE) {
                    pos += 1;
                } else if (type == LPP_LUMINOSITY || type == LPP_CONCENTRATION || type == LPP_DIRECTION || type == LPP_PM25 || type == LPP_PM10 || type == LPP_CO2 || type == LPP_TVOC || type == LPP_RPM || type == LPP_CONDUCTIVITY || type == LPP_SPEED || type == LPP_GUST || type == LPP_DEW_POINT || type == LPP_RAIN) {
                    pos += 2;
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
    lv_obj_set_style_radius(row, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(row, 10, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, on_card_click, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
    lv_obj_set_ext_click_area(row, 10);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* name = lv_label_create(row);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(name, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_width(name, lv_pct(100));

    lv_obj_t* body = lv_label_create(row);
    lv_obj_set_style_text_font(body, &lv_font_noto_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(body, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_width(body, lv_pct(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);

    card_rows[idx] = row;
    card_name_labels[idx] = name;
    card_body_labels[idx] = body;
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

    for (int i = 0; i < MAX_CONTACTS; i++) {
        ensure_row(i);
    }

    empty_label = lv_label_create(sensor_list);
    lv_obj_set_width(empty_label, lv_pct(100));
    lv_obj_set_flex_grow(empty_label, 1);
    lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_bold_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(empty_label, lv_color_hex(EPD_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(empty_label, "\n\n\nNo favorite nodes");
    lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
}

static void entry() {
    card_count = 0;
    mesh::task::push_all_contacts();
    rebuild_list();
    poll_timer = lv_timer_create(poll_updates, 500, NULL);
    poll_updates(NULL);
}

static void exit_fn() {
    if (poll_timer) { lv_timer_del(poll_timer); poll_timer = NULL; }
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
        row_visible[i] = false;
    }
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::sensors
