#pragma once

#include <cmath>

namespace ui::geo {

static constexpr double DEG_TO_RAD = M_PI / 180.0;
static constexpr double R_EARTH_KM = 6371.0;
static constexpr double KM_PER_DEG_LAT = 111.32;

// Haversine distance in km
inline double distance_km(double lat1, double lon1, double lat2, double lon2) {
    double dLat = (lat2 - lat1) * DEG_TO_RAD;
    double dLon = (lon2 - lon1) * DEG_TO_RAD;
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
               sin(dLon / 2) * sin(dLon / 2);
    return R_EARTH_KM * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

// Bearing from point 1 to point 2 in degrees (0=N, 90=E)
inline double bearing(double lat1, double lon1, double lat2, double lon2) {
    double dLon = (lon2 - lon1) * DEG_TO_RAD;
    double y = sin(dLon) * cos(lat2 * DEG_TO_RAD);
    double x = cos(lat1 * DEG_TO_RAD) * sin(lat2 * DEG_TO_RAD) -
               sin(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) * cos(dLon);
    double b = atan2(y, x) * (180.0 / M_PI);
    return fmod(b + 360.0, 360.0);
}

inline const char* bearing_to_cardinal(double bearing) {
    if (bearing >= 337.5 || bearing < 22.5)  return "N";
    if (bearing < 67.5)  return "NE";
    if (bearing < 112.5) return "E";
    if (bearing < 157.5) return "SE";
    if (bearing < 202.5) return "S";
    if (bearing < 247.5) return "SW";
    if (bearing < 292.5) return "W";
    return "NW";
}

} // namespace ui::geo
