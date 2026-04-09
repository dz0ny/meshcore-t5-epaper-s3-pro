#pragma once

#include <helpers/sensors/LocationProvider.h>
#include "../../board.h"

// LocationProvider implementation that wraps our board's GPS (TinyGPSPlus / u-blox).
// Used by MeshCore's SensorManager for GPS telemetry and location sharing.
class BoardGPS : public LocationProvider {
public:
    void begin() override {}      // GPS already initialized by board::init()
    void stop() override {}
    void loop() override {}       // GPS task already running on its own
    void reset() override {}
    bool isEnabled() override { return board::peri_status[E_PERI_GPS]; }

    bool isValid() override {
        return board::gps.location.isValid();
    }

    long getLatitude() override {
        return (long)(board::gps.location.lat() * 1e6);
    }

    long getLongitude() override {
        return (long)(board::gps.location.lng() * 1e6);
    }

    long getAltitude() override {
        return board::gps.altitude.isValid() ? (long)(board::gps.altitude.meters() * 100) : 0;
    }

    long satellitesCount() override {
        return board::gps_satellites();
    }

    long getTimestamp() override {
        return 0; // not used
    }
};
