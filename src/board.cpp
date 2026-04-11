#include "board.h"
#include "nvs_param.h"
#include "mesh/companion/target.h"

namespace board {

SemaphoreHandle_t i2c_mutex = NULL;

// ---------- Global peripheral instances ----------

bool peri_status[E_PERI_MAX] = {0};
XPowersPPM ppm;
BQ27220 bq27220;
TouchDrvGT911 touch;
SensorPCF8563 rtc;

// E-paper
#define WAVEFORM EPD_BUILTIN_WAVEFORM
#define DEMO_BOARD epd_board_v7
EpdiyHighlevelState hl;

// Home button
volatile bool home_button_pressed = false;

// GPS is now handled by MeshCore's EnvironmentSensorManager + MicroNMEALocationProvider
// (configured in target.cpp, polled by sensors.loop() in mesh_task.cpp)

// ---------- Detail init functions ----------

namespace detail {

bool screen_init() {
    epd_init(&DEMO_BOARD, &ED047TC1, EPD_LUT_64K);
    epd_set_vcom(nvs_param_get_u16(NVS_ID_EPD_VCOM));
    hl = epd_hl_init(WAVEFORM);
    epd_set_rotation(EPD_ROT_INVERTED_PORTRAIT);

    Serial.printf("Display: %d x %d\n", epd_rotated_display_width(), epd_rotated_display_height());

    epd_set_lcd_pixel_clock_MHz(17);
    epd_poweron();
    epd_clear();
    epd_poweroff();
    return true;
}

bool bq25896_init() {
    bool result = ppm.init(Wire, BOARD_SDA, BOARD_SCL, BQ25896_SLAVE_ADDRESS);
    if (!result) return false;

    ppm.setSysPowerDownVoltage(3300);
    ppm.setInputCurrentLimit(3250);
    ppm.disableCurrentLimitPin();
    ppm.setChargeTargetVoltage(4208);
    ppm.setPrechargeCurr(64);
    ppm.setChargerConstantCurr(1024);
    ppm.enableMeasure();
    ppm.enableCharge();
    ppm.enableOTG();
    ppm.disableOTG();
    return true;
}

bool bq27220_init() {
    return bq27220.init();
}

bool rtc_init() {
    pinMode(BOARD_RTC_IRQ, INPUT_PULLUP);
    if (!rtc.begin(Wire, PCF8563_SLAVE_ADDRESS, BOARD_RTC_SDA, BOARD_RTC_SCL)) {
        Serial.println("Failed to find PCF8563");
        return false;
    }
    return true;
}

bool touch_init() {
    touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_INT);
    if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L, BOARD_SDA, BOARD_SCL)) {
        Serial.println("Failed to find GT911");
        return false;
    }
    Serial.println("GT911 touch init OK");

    // Home button (center touch button on GT911) — sets flag for UI task
    touch.setHomeButtonCallback([](void* user_data) {
        Serial.println("Home button pressed");
        home_button_pressed = true;
    }, NULL);

    touch.setInterruptMode(LOW_LEVEL_QUERY);
    return true;
}

bool sd_init() {
    if (!SD.begin(BOARD_SD_CS)) {
        Serial.println("SD card mount failed");
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        Serial.println("No SD card");
        return false;
    }
    return true;
}

bool gps_init() {
    // GPS is now initialized by MeshCore's EnvironmentSensorManager (sensors.begin())
    // via MicroNMEALocationProvider which uses Serial1 with PIN_GPS_TX/RX
    // Just return true — GPS detection happens in sensors.begin()
    return true;
}

} // namespace detail

// ---------- Main init ----------

void init() {
    gpio_hold_dis((gpio_num_t)BOARD_TOUCH_RST);
    gpio_hold_dis((gpio_num_t)BOARD_LORA_RST);
    gpio_deep_sleep_hold_dis();

    // Pull all SPI CS lines high before init
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);

    if (BOARD_PCA9535_INT > 0) {
        pinMode(BOARD_PCA9535_INT, INPUT_PULLUP);
    }

    Serial.begin(115200);
    SPI.begin(BOARD_SPI_SCLK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
    Wire.begin(BOARD_SDA, BOARD_SCL);
    Wire.setTimeOut(50);  // 50ms timeout (default 50ms, but explicit)
    pinMode(BOARD_BL_EN, OUTPUT);

    nsv_param_init();

    peri_status[E_PERI_BQ27220] = detail::bq27220_init();
    peri_status[E_PERI_INK_POWER] = detail::screen_init();
    io_extend_lora_gps_power_on(true);
    peri_status[E_PERI_BQ25896] = detail::bq25896_init();
    peri_status[E_PERI_RTC] = detail::rtc_init();
    peri_status[E_PERI_TOUCH] = detail::touch_init();
    peri_status[E_PERI_SD_CARD] = detail::sd_init();
    peri_status[E_PERI_GPS] = detail::gps_init();

    // I2C mutex for cross-core safety (epdiy on Core 1, mesh/RTC on Core 0)
    i2c_mutex = xSemaphoreCreateMutex();

    // Init MeshCore RTC clock before tasks start (avoids I2C race with epdiy)
    rtc_init();

    Serial.println("Board init complete");
    for (int i = 0; i < E_PERI_MAX; i++) {
        Serial.printf("  peri[%d] = %s\n", i, peri_status[i] ? "OK" : "FAIL");
    }
}

// ---------- Battery ----------

bool battery_is_charging() {
    return ppm.isCharging();
}

uint16_t battery_percent() {
    if (!peri_status[E_PERI_BQ27220]) return 0;
    uint16_t pct = bq27220.getStateOfCharge();
    if (pct > 100) return 0;
    return pct;
}

uint16_t battery_voltage_mv() {
    if (!peri_status[E_PERI_BQ27220]) return 0;
    return bq27220.getVoltage();
}

int16_t battery_current_ma() {
    if (!peri_status[E_PERI_BQ27220]) return 0;
    return bq27220.getCurrent();
}

uint16_t battery_temperature() {
    if (!peri_status[E_PERI_BQ27220]) return 0;
    return bq27220.getTemperature();
}

uint16_t battery_full_capacity() {
    if (!peri_status[E_PERI_BQ27220]) return 0;
    return bq27220.getFullChargeCapacity();
}

uint16_t battery_design_capacity() {
    if (!peri_status[E_PERI_BQ27220]) return 0;
    return bq27220.getDesignCapacity();
}

uint16_t battery_remain_capacity() {
    if (!peri_status[E_PERI_BQ27220]) return 0;
    return bq27220.getRemainingCapacity();
}

uint16_t battery_health() {
    if (!peri_status[E_PERI_BQ27220]) return 0;
    return bq27220.getStateOfHealth();
}

// BQ25896 charger
bool charger_is_valid() { return peri_status[E_PERI_BQ25896]; }
bool charger_vbus_in() { return ppm.isVbusIn(); }
const char* charger_status_str() { return ppm.getChargeStatusString(); }
const char* charger_bus_status_str() { return ppm.getBusStatusString(); }
const char* charger_ntc_status_str() { return ppm.getNTCStatusString(); }
float charger_vbus_v() { return ppm.getVbusVoltage() / 1000.0f; }
float charger_vsys_v() { return ppm.getSystemVoltage() / 1000.0f; }
float charger_vbat_v() { return ppm.getBattVoltage() / 1000.0f; }
float charger_target_v() { return ppm.getChargeTargetVoltage() / 1000.0f; }
float charger_current_ma() { return ppm.getChargeCurrent(); }
float charger_prechrg_ma() { return ppm.getPrechargeCurr(); }

// ---------- GPS ----------

void gps_get_coord(double* lat, double* lng) {
    *lat = 0; *lng = 0; // GPS now via MeshCore sensors
}

uint32_t gps_satellites() {
    return 0; // GPS now via MeshCore sensors
}

// ---------- RTC ----------

void rtc_get_time(uint8_t* h, uint8_t* m, uint8_t* s) {
    RTC_DateTime dt = rtc.getDateTime();
    *h = dt.hour;
    *m = dt.minute;
    *s = dt.second;
}

void rtc_get_date(uint8_t* year, uint8_t* month, uint8_t* day, uint8_t* week) {
    RTC_DateTime dt = rtc.getDateTime();
    if (year) *year = dt.year % 100;
    if (month) *month = dt.month;
    if (day) *day = dt.day;
    if (week) *week = 0;
}

} // namespace board
