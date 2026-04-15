#ifdef BOARD_TDECK

#include "board.h"
#include "model.h"
#include "nvs_param.h"
#include "mesh/companion/target.h"

namespace board {

SemaphoreHandle_t i2c_mutex = NULL;

bool peri_status[E_PERI_MAX] = {0};
TouchDrvGT911 touch;

volatile bool home_button_pressed = false;

// ---------- Trackball ISR state ----------
// Debounce: ignore edges within TB_DEBOUNCE_US of the last edge (per axis).
// Meshtastic uses TB_THRESHOLD=3; we debounce in time instead for smoother feel.

static constexpr uint32_t TB_DEBOUNCE_US = 800;  // microseconds

static volatile int8_t tb_dx = 0;
static volatile int8_t tb_dy = 0;
static volatile bool tb_clicked = false;
volatile uint32_t tb_isr_up_total = 0;
volatile uint32_t tb_isr_down_total = 0;
volatile uint32_t tb_isr_left_total = 0;
volatile uint32_t tb_isr_right_total = 0;
static volatile uint32_t tb_last_up = 0;
static volatile uint32_t tb_last_down = 0;
static volatile uint32_t tb_last_left = 0;
static volatile uint32_t tb_last_right = 0;

static void IRAM_ATTR tb_up_isr() {
    uint32_t now = micros();
    if (now - tb_last_up > TB_DEBOUNCE_US) { tb_dy--; tb_last_up = now; tb_isr_up_total++; }
}
static void IRAM_ATTR tb_down_isr() {
    uint32_t now = micros();
    if (now - tb_last_down > TB_DEBOUNCE_US) { tb_dy++; tb_last_down = now; tb_isr_down_total++; }
}
static void IRAM_ATTR tb_left_isr() {
    uint32_t now = micros();
    if (now - tb_last_left > TB_DEBOUNCE_US) { tb_dx--; tb_last_left = now; tb_isr_left_total++; }
}
static void IRAM_ATTR tb_right_isr() {
    uint32_t now = micros();
    if (now - tb_last_right > TB_DEBOUNCE_US) { tb_dx++; tb_last_right = now; tb_isr_right_total++; }
}
static void IRAM_ATTR tb_click_isr() { tb_clicked = true; }

// ---------- Detail init functions ----------

namespace detail {

bool display_init() {
    // ST7789 init is done in ui_port_tft.cpp via SPI
    // Here we just enable backlight
    pinMode(TDECK_DISP_BL, OUTPUT);
    digitalWrite(TDECK_DISP_BL, HIGH);
    return true;
}

bool touch_init() {
    touch.setPins(-1, BOARD_TOUCH_INT);  // no reset pin on T-Deck
    bool ok = touch.begin(Wire, GT911_SLAVE_ADDRESS_H, BOARD_SDA, BOARD_SCL);
    if (!ok) {
        ok = touch.begin(Wire, GT911_SLAVE_ADDRESS_L, BOARD_SDA, BOARD_SCL);
    }
    if (!ok) {
        Serial.println("GT911 touch init failed");
        return false;
    }
    // Align touch coordinates with landscape display rotation (from trail-mate)
    touch.setMaxCoordinates(SCREEN_WIDTH, SCREEN_HEIGHT);
    touch.setSwapXY(true);
    touch.setMirrorXY(false, true);
    Serial.println("GT911 touch init OK");
    return true;
}

bool keyboard_init() {
    // Keyboard is on I2C at TDECK_KB_ADDR (0x55)
    Wire.beginTransmission(TDECK_KB_ADDR);
    bool ok = (Wire.endTransmission() == 0);
    if (ok) {
        Serial.println("T-Deck keyboard found");
    } else {
        Serial.println("T-Deck keyboard not found");
    }
    return ok;
}

bool trackball_init() {
    pinMode(TDECK_TB_UP, INPUT_PULLUP);
    pinMode(TDECK_TB_DOWN, INPUT_PULLUP);
    pinMode(TDECK_TB_LEFT, INPUT_PULLUP);
    pinMode(TDECK_TB_RIGHT, INPUT_PULLUP);
    pinMode(TDECK_TB_CLICK, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(TDECK_TB_UP), tb_up_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(TDECK_TB_DOWN), tb_down_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(TDECK_TB_LEFT), tb_left_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(TDECK_TB_RIGHT), tb_right_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(TDECK_TB_CLICK), tb_click_isr, FALLING);

    Serial.println("Trackball init OK");
    return true;
}

bool sd_init() {
    // 75MHz SD SPI — matches Meshtastic T-Deck config for fast file I/O
    if (!SD.begin(BOARD_SD_CS, SPI, 75000000U)) {
        Serial.println("SD card mount failed");
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        Serial.println("No SD card");
        return false;
    }
    Serial.println("SD card OK");
    return true;
}

bool gps_init() {
    // GPS initialized by MeshCore's EnvironmentSensorManager
    return true;
}

} // namespace detail

void seed_clock_from_rtc() {
    // No hardware RTC on T-Deck — system clock comes from GPS or defaults
}

// ---------- Main init ----------

void init() {
    // T-Deck requires powering on peripherals first
    pinMode(TDECK_POWER_EN, OUTPUT);
    digitalWrite(TDECK_POWER_EN, HIGH);
    delay(100);  // let peripherals stabilize

    Serial.begin(115200);

    // SPI bus
    SPI.begin(BOARD_SPI_SCLK, BOARD_SPI_MISO, BOARD_SPI_MOSI);

    // Pull CS lines high
    pinMode(P_LORA_NSS, OUTPUT);
    digitalWrite(P_LORA_NSS, HIGH);
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(TDECK_DISP_CS, OUTPUT);
    digitalWrite(TDECK_DISP_CS, HIGH);

    nsv_param_init();
    model::init_messages();

    // I2C for touch + keyboard
    Wire.begin(BOARD_SDA, BOARD_SCL);
    Wire.setTimeOut(50);

    peri_status[E_PERI_DISPLAY] = detail::display_init();
    peri_status[E_PERI_TOUCH] = detail::touch_init();
    peri_status[E_PERI_KEYBOARD] = detail::keyboard_init();
    peri_status[E_PERI_TRACKBALL] = detail::trackball_init();
    peri_status[E_PERI_SD_CARD] = detail::sd_init();
    peri_status[E_PERI_GPS] = detail::gps_init();

    // No BQ27220, BQ25896, RTC, or PCA9535 on T-Deck
    peri_status[E_PERI_BQ25896] = false;
    peri_status[E_PERI_BQ27220] = false;
    peri_status[E_PERI_RTC] = false;
    peri_status[E_PERI_INK_POWER] = false;

    i2c_mutex = xSemaphoreCreateMutex();

    Serial.println("Board init complete (T-Deck)");
    for (int i = 0; i < E_PERI_MAX; i++) {
        Serial.printf("  %s = %s\n", peri_name(i), peri_status[i] ? "OK" : "N/A");
    }
    Serial.printf("Free DRAM: %u, Free PSRAM: %u\n",
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// ---------- Battery (ADC-based) ----------

// ADC multiplier 2.11 — Meshtastic uses this to correct for voltage divider
// tolerance (nominal 2.0 from 100k/100k, but real-world ~2.11).
static constexpr float ADC_MULTIPLIER = 2.11f;

static int read_battery_mv() {
    analogSetPinAttenuation(TDECK_BAT_ADC, ADC_11db);
    int mv = analogReadMilliVolts(TDECK_BAT_ADC);
    if (mv <= 0) return 0;
    return (int)(mv * ADC_MULTIPLIER);
}

static uint16_t mv_to_percent(int mv) {
    if (mv <= 0) return 0;
    int pct = (int)(((mv - 3300) / 900.0f) * 100.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint16_t)pct;
}

bool battery_is_charging() { return false; }  // no charger IC to query

uint16_t battery_percent() {
    return mv_to_percent(read_battery_mv());
}

uint16_t battery_voltage_mv() {
    return (uint16_t)read_battery_mv();
}

int16_t battery_current_ma() { return 0; }
uint16_t battery_temperature() { return 0; }
uint16_t battery_full_capacity() { return 0; }
uint16_t battery_design_capacity() { return 0; }
uint16_t battery_remain_capacity() { return 0; }
uint16_t battery_health() { return 0; }

// Charger stubs (no BQ25896)
bool     charger_is_valid() { return false; }
bool     charger_vbus_in() { return false; }
const char* charger_status_str() { return "N/A"; }
const char* charger_bus_status_str() { return "N/A"; }
const char* charger_ntc_status_str() { return "N/A"; }
float    charger_vbus_v() { return 0; }
float    charger_vsys_v() { return 0; }
float    charger_vbat_v() { return 0; }
float    charger_target_v() { return 0; }
float    charger_current_ma() { return 0; }
float    charger_prechrg_ma() { return 0; }

// ---------- GPS ----------

void gps_get_coord(double* lat, double* lng) {
    *lat = 0; *lng = 0;
}

uint32_t gps_satellites() { return 0; }

// ---------- RTC stubs ----------

void rtc_get_time(uint8_t* h, uint8_t* m, uint8_t* s) {
    *h = 0; *m = 0; *s = 0;
}

void rtc_get_date(uint8_t* year, uint8_t* month, uint8_t* day, uint8_t* week) {
    if (year) *year = 0;
    if (month) *month = 0;
    if (day) *day = 0;
    if (week) *week = 0;
}

// ---------- Keyboard ----------

// Keyboard I2C commands (from Meshtastic/LilyGo reference)
static constexpr uint8_t KB_CMD_BRIGHTNESS = 0x01;
static uint8_t kb_backlight_level = 0;

int keyboard_read_char() {
    if (!peri_status[E_PERI_KEYBOARD]) return -1;
    Wire.requestFrom((uint8_t)TDECK_KB_ADDR, (uint8_t)1);
    if (Wire.available()) {
        uint8_t c = Wire.read();
        if (c != 0) return (int)c;
    }
    return -1;
}

void keyboard_set_backlight(uint8_t level) {
    if (!peri_status[E_PERI_KEYBOARD]) return;
    kb_backlight_level = level;
    Wire.beginTransmission(TDECK_KB_ADDR);
    Wire.write(KB_CMD_BRIGHTNESS);
    Wire.write(level);
    Wire.endTransmission();
}

uint8_t keyboard_get_backlight() {
    return kb_backlight_level;
}

// ---------- Trackball ----------

TrackballState trackball_read() {
    TrackballState s;
    noInterrupts();
    s.dx = tb_dx;
    s.dy = tb_dy;
    s.clicked = tb_clicked;
    tb_dx = 0;
    tb_dy = 0;
    tb_clicked = false;
    interrupts();
    return s;
}

} // namespace board

#endif // BOARD_TDECK
