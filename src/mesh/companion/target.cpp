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

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);

// GPS via MeshCore's MicroNMEA provider — uses Serial1 (mapped to PIN_GPS_TX/RX)
MicroNMEALocationProvider gps_provider(Serial1, &rtc_clock);
EnvironmentSensorManager sensors(gps_provider);

// Call once from board::init() before any tasks start — avoids I2C race with epdiy
void rtc_init() {
    fallback_clock.begin();
    rtc_clock.begin(Wire);
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
