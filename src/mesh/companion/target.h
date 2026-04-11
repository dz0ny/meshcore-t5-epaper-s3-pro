#pragma once

// target.h — hardware abstraction for t-paper board (companion radio style)
// This provides the same interface that MeshCore variants expect.

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/ESP32Board.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#include <helpers/sensors/MicroNMEALocationProvider.h>

// Custom board that reads battery from BQ27220 fuel gauge
class T5ePaperBoard : public ESP32Board {
public:
    uint16_t getBattMilliVolts() override;
    const char* getManufacturerName() const override { return "LilyGo T5-ePaper"; }
};

extern T5ePaperBoard mc_board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;
extern MicroNMEALocationProvider gps_provider;

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(int8_t dbm);
mesh::LocalIdentity radio_new_identity();
