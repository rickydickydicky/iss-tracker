#pragma once
// ─── Earth / observer geometry ──────────────────────────
// Frames: SGP4 outputs TEME. We rotate TEME -> Earth-fixed with Greenwich
// mean sidereal time only (no polar motion, UT1 ~= UTC). That is the usual
// approximation for amateur pass prediction and costs < 0.02 deg here.

#include "orbit_sgp4.h"

namespace orbit {

// Greenwich mean sidereal time (radians, 0..2pi) for a UT1 Julian Date.
double gmst(double jdUt1);

// Rotate a TEME vector into the Earth-fixed frame (pseudo-ECEF).
void temeToEcef(const double teme[3], double jd, double ecef[3]);

// Observer on the WGS-84 ellipsoid.
struct Observer {
    double latDeg;     // geodetic latitude, +N
    double lonDeg;     // longitude, +E
    double altKm;      // height above ellipsoid
    // Derived (filled by observerInit)
    double ecef[3];
    double sinLat, cosLat, sinLon, cosLon;
};

void observerInit(Observer& obs, double latDeg, double lonDeg, double altKm);

struct LookAngles {
    double azDeg;      // 0 = North, 90 = East
    double elDeg;      // above the horizon (no refraction)
    double rangeKm;
};

// Topocentric azimuth/elevation of an Earth-fixed point.
void lookAnglesEcef(const Observer& obs, const double targetEcef[3], LookAngles& out);

// Low-precision Sun position (Astronomical Almanac / Vallado algorithm 29),
// good to ~0.01 deg. Returns an equatorial (mean-of-date ~ TEME) vector in km.
void sunPositionEci(double jd, double rSunKm[3]);

// True if a satellite at TEME position rSat (km) is in direct sunlight, i.e.
// the line toward the Sun does not pass through the Earth (spherical Earth,
// same test as Skyfield's is_sunlit()).
bool isSunlit(const double rSatKm[3], const double rSunKm[3]);

// Sun elevation above the observer's horizon at `jd` (degrees).
double sunElevationDeg(const Observer& obs, double jd);

// 8-point compass label for an azimuth ("N", "NE", "E", ...).
const char* compassPoint8(double azDeg);

}  // namespace orbit
