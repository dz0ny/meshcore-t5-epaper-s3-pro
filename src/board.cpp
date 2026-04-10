#include "board.h"
#include "nvs_param.h"

namespace board {

// ---------- Global peripheral instances ----------

bool peri_status[E_PERI_MAX] = {0};
XPowersPPM ppm;
BQ27220 bq27220;
TouchDrvGT911 touch;
SensorPCF8563 rtc;
TinyGPSPlus gps;

// E-paper
#define WAVEFORM EPD_BUILTIN_WAVEFORM
#define DEMO_BOARD epd_board_v7
EpdiyHighlevelState hl;

// Home button
volatile bool home_button_pressed = false;

// GPS state
static TaskHandle_t gps_handle = NULL;
static double gps_lat = 0, gps_lng = 0;
static uint32_t gps_vsat = 0;
static uint8_t gps_hour = 0, gps_minute = 0, gps_second = 0;
static bool gps_is_ublox = false;
static bool gps_rtc_synced = false;
SFE_UBLOX_GNSS ublox;

// Sync RTC from GPS time when we get a valid fix
static void sync_rtc_from_gps() {
    if (gps_rtc_synced) return;
    if (!peri_status[E_PERI_RTC]) return;
    if (!gps.date.isValid() || !gps.time.isValid()) return;
    if (gps.date.year() < 2024) return;

    rtc.setDateTime(gps.date.year(), gps.date.month(), gps.date.day(),
                    gps.time.hour(), gps.time.minute(), gps.time.second());
    gps_rtc_synced = true;
    Serial.printf("RTC synced from GPS: %04d-%02d-%02d %02d:%02d:%02d\n",
        gps.date.year(), gps.date.month(), gps.date.day(),
        gps.time.hour(), gps.time.minute(), gps.time.second());
}

// ---------- GPS: L76K detection (Quectel, NMEA at 9600 baud) ----------

static bool detect_L76K() {
    SerialGPS.begin(9600, SERIAL_8N1, BOARD_GPS_RXD, BOARD_GPS_TXD);
    for (int attempt = 0; attempt < 3; attempt++) {
        // Silence all NMEA sentences first
        SerialGPS.write("$PCAS03,0,0,0,0,0,0,0,0,0,0,,,0,0*02\r\n");
        delay(5);
        uint32_t timeout = millis() + 3000;
        while (SerialGPS.available()) {
            SerialGPS.readString();
            if (millis() > timeout) return false;
        }
        SerialGPS.flush();
        delay(200);

        // Request firmware version
        SerialGPS.write("$PCAS06,0*1B\r\n");
        timeout = millis() + 500;
        while (!SerialGPS.available()) {
            if (millis() > timeout) return false;
        }
        SerialGPS.setTimeout(10);
        String ver = SerialGPS.readStringUntil('\n');
        if (ver.startsWith("$GPTXT,01,01,02")) {
            // Configure: GPS + GLONASS, all NMEA sentences, vehicle mode
            SerialGPS.write("$PCAS04,5*1C\r\n");
            delay(250);
            SerialGPS.write("$PCAS03,1,1,1,1,1,1,1,1,1,1,,,0,0*02\r\n");
            delay(250);
            SerialGPS.write("$PCAS11,3*1E\r\n");
            Serial.println("GPS: L76K detected at 9600 baud");
            return true;
        }
        delay(500);
    }
    return false;
}

// ---------- GPS: u-blox detection (M10Q, UBX at 38400 baud) ----------

static bool detect_ublox() {
    // Try 38400 first (factory default for M10Q on this board)
    SerialGPS.begin(38400, SERIAL_8N1, BOARD_GPS_RXD, BOARD_GPS_TXD);
    if (ublox.begin(SerialGPS)) {
        Serial.println("GPS: u-blox detected at 38400 baud");
        return true;
    }

    // Fallback: try 9600
    SerialGPS.updateBaudRate(9600);
    if (ublox.begin(SerialGPS)) {
        // Set to 38400 for better throughput
        ublox.setSerialRate(38400);
        delay(100);
        SerialGPS.updateBaudRate(38400);
        if (ublox.begin(SerialGPS)) {
            Serial.println("GPS: u-blox detected at 9600, switched to 38400");
            return true;
        }
    }
    return false;
}

// ---------- GPS task: NMEA parsing (L76K) ----------

static void gps_task_nmea(void* param) {
    while (1) {
        while (SerialGPS.available()) {
            int c = SerialGPS.read();
            if (gps.encode(c)) {
                if (gps.location.isValid()) {
                    gps_lat = gps.location.lat();
                    gps_lng = gps.location.lng();
                }
                if (gps.time.isValid()) {
                    gps_hour = gps.time.hour();
                    gps_minute = gps.time.minute();
                    gps_second = gps.time.second();
                }
                if (gps.satellites.isValid()) {
                    gps_vsat = gps.satellites.value();
                }
                // Sync RTC as soon as we have valid time — no need to wait for fix
                sync_rtc_from_gps();
            }
        }
        delay(1);
    }
}

// ---------- GPS task: UBX polling (u-blox) ----------

static void gps_task_ublox(void* param) {
    // Enable NMEA output on u-blox so TinyGPSPlus can also parse
    ublox.setUART1Output(COM_TYPE_UBX | COM_TYPE_NMEA);
    ublox.setNavigationFrequency(1); // 1Hz
    ublox.setAutoPVT(true);          // automatic PVT polling
    ublox.saveConfiguration();

    while (1) {
        ublox.checkUblox();

        if (ublox.getPVT()) {
            gps_lat = ublox.getLatitude() / 1e7;
            gps_lng = ublox.getLongitude() / 1e7;
            gps_hour = ublox.getHour();
            gps_minute = ublox.getMinute();
            gps_second = ublox.getSecond();
            gps_vsat = ublox.getSIV();

            // Sync RTC from u-blox time
            if (!gps_rtc_synced && ublox.getDateValid() && ublox.getTimeValid() &&
                ublox.getYear() >= 2024 && peri_status[E_PERI_RTC]) {
                rtc.setDateTime(ublox.getYear(), ublox.getMonth(), ublox.getDay(),
                                ublox.getHour(), ublox.getMinute(), ublox.getSecond());
                gps_rtc_synced = true;
                Serial.printf("RTC synced from u-blox: %04d-%02d-%02d %02d:%02d:%02d\n",
                    ublox.getYear(), ublox.getMonth(), ublox.getDay(),
                    ublox.getHour(), ublox.getMinute(), ublox.getSecond());
            }
        }
        delay(100);
    }
}

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
    // Try L76K first (Quectel, 9600 baud NMEA)
    if (detect_L76K()) {
        gps_is_ublox = false;
        xTaskCreate(gps_task_nmea, "gps", 1024 * 4, NULL, GPS_PRIORITY, &gps_handle);
        return true;
    }

    // Try u-blox M10Q (38400/9600 baud, UBX protocol)
    if (detect_ublox()) {
        gps_is_ublox = true;
        xTaskCreate(gps_task_ublox, "gps", 1024 * 4, NULL, GPS_PRIORITY, &gps_handle);
        return true;
    }

    Serial.println("GPS: no module detected");
    return false;
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
    *lat = gps_lat;
    *lng = gps_lng;
}

uint32_t gps_satellites() {
    return gps_vsat;
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
