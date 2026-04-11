#include <Arduino.h>
#include "target.h"
#include "../../board.h"

T5ePaperBoard mc_board;

uint16_t T5ePaperBoard::getBattMilliVolts() {
    return board::battery_voltage_mv();
}

// Use the global SPI bus — already initialized by board::init()
// Do NOT create a separate SPIClass, it conflicts with SD card on shared bus.
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI);
WRAPPER_CLASS radio_driver(radio, mc_board);

// Use software-only RTC for mesh — no I2C, avoids cross-core bus conflict with epdiy.
// Time is synced from PCF8563 hardware RTC by model::update_clock() on Core 1.
ESP32RTCClock rtc_clock;

// GPS via MeshCore's MicroNMEA provider — uses Serial1 (mapped to PIN_GPS_TX/RX)
MicroNMEALocationProvider gps_provider(Serial1);
EnvironmentSensorManager sensors(gps_provider);

void rtc_init() {
    // Skip rtc_clock.begin() — it overwrites settimeofday() with a hardcoded 2024 date.
    // System clock is already seeded from hardware RTC in board::init().
}

bool radio_init() {
    // SPI already initialized by board::init(), pass NULL to skip re-init
    return radio.std_init(NULL);
}

uint32_t radio_get_rng_seed() {
    return radio.random(0x7FFFFFFF);
}

void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
    radio.setFrequency(freq);
    radio.setSpreadingFactor(sf);
    radio.setBandwidth(bw);
    radio.setCodingRate(cr);
}

void radio_set_tx_power(int8_t dbm) {
    radio.setOutputPower(dbm);
}

mesh::LocalIdentity radio_new_identity() {
    RadioNoiseListener rng(radio);
    return mesh::LocalIdentity(&rng);
}
