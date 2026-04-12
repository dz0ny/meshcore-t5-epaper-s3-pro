#ifdef BOARD_TDECK

#include <Arduino.h>
#include "target.h"
#include "../../board.h"

// T-Deck board for MeshCore — battery from ADC
class TDeckMCBoard : public ESP32Board {
public:
    uint16_t getBattMilliVolts() override {
        return board::battery_voltage_mv();
    }
    const char* getManufacturerName() const override { return "LilyGo T-Deck"; }
};

static TDeckMCBoard tdeck_mc_board;

// Expose as the global mc_board expected by MeshCore
T5ePaperBoard mc_board;  // Use the base type from target.h

uint16_t T5ePaperBoard::getBattMilliVolts() {
    return board::battery_voltage_mv();
}

// Radio on T-Deck SPI bus (shared with display and SD)
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI);
WRAPPER_CLASS radio_driver(radio, mc_board);

// Software RTC (no hardware RTC on T-Deck)
ESP32RTCClock rtc_clock;

// GPS via MeshCore's MicroNMEA provider
MicroNMEALocationProvider gps_provider(Serial1);
EnvironmentSensorManager sensors(gps_provider);

void rtc_init() {
    // No hardware RTC on T-Deck — skip rtc_clock.begin()
}

bool radio_init() {
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

#endif // BOARD_TDECK
