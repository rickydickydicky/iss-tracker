#include "orbit_geo.h"

#include <math.h>

namespace orbit {

namespace {
const double kPi      = 3.14159265358979323846;
const double kTwoPi   = 2.0 * kPi;
const double kDeg2Rad = kPi / 180.0;
const double kRad2Deg = 180.0 / kPi;

// WGS-84 ellipsoid for the observer
const double kWgs84A  = 6378.137;               // km
const double kWgs84F  = 1.0 / 298.257223563;
const double kWgs84E2 = kWgs84F * (2.0 - kWgs84F);

// Earth radius used for the shadow test (matches Skyfield's ERAD)
const double kShadowRadiusKm = 6378.1366;
const double kAuKm = 149597870.700;
}  // namespace

double gmst(double jdUt1) {
    // Vallado eq. 3-45 (IAU-82), identical to python-sgp4 gstime()
    const double tut1 = (jdUt1 - 2451545.0) / 36525.0;
    double temp = -6.2e-6 * tut1 * tut1 * tut1 + 0.093104 * tut1 * tut1 +
                  (876600.0 * 3600.0 + 8640184.812866) * tut1 + 67310.54841;  // sec
    temp = fmod(temp * kDeg2Rad / 240.0, kTwoPi);
    if (temp < 0.0) temp += kTwoPi;
    return temp;
}

void temeToEcef(const double teme[3], double jd, double ecef[3]) {
    const double g = gmst(jd);
    const double c = cos(g), s = sin(g);
    ecef[0] =  c * teme[0] + s * teme[1];
    ecef[1] = -s * teme[0] + c * teme[1];
    ecef[2] =  teme[2];
}

void observerInit(Observer& obs, double latDeg, double lonDeg, double altKm) {
    obs.latDeg = latDeg;
    obs.lonDeg = lonDeg;
    obs.altKm  = altKm;
    const double lat = latDeg * kDeg2Rad;
    const double lon = lonDeg * kDeg2Rad;
    obs.sinLat = sin(lat);
    obs.cosLat = cos(lat);
    obs.sinLon = sin(lon);
    obs.cosLon = cos(lon);
    const double n = kWgs84A / sqrt(1.0 - kWgs84E2 * obs.sinLat * obs.sinLat);
    obs.ecef[0] = (n + altKm) * obs.cosLat * obs.cosLon;
    obs.ecef[1] = (n + altKm) * obs.cosLat * obs.sinLon;
    obs.ecef[2] = (n * (1.0 - kWgs84E2) + altKm) * obs.sinLat;
}

void lookAnglesEcef(const Observer& obs, const double t[3], LookAngles& out) {
    const double rx = t[0] - obs.ecef[0];
    const double ry = t[1] - obs.ecef[1];
    const double rz = t[2] - obs.ecef[2];
    // East-North-Up relative to the ellipsoid normal
    const double e = -obs.sinLon * rx + obs.cosLon * ry;
    const double n = -obs.sinLat * obs.cosLon * rx - obs.sinLat * obs.sinLon * ry +
                      obs.cosLat * rz;
    const double u =  obs.cosLat * obs.cosLon * rx + obs.cosLat * obs.sinLon * ry +
                      obs.sinLat * rz;
    const double range = sqrt(rx * rx + ry * ry + rz * rz);
    double az = atan2(e, n) * kRad2Deg;
    if (az < 0.0) az += 360.0;
    out.azDeg   = az;
    out.elDeg   = asin(u / range) * kRad2Deg;
    out.rangeKm = range;
}

void sunPositionEci(double jd, double r[3]) {
    const double tut1 = (jd - 2451545.0) / 36525.0;
    double meanlong = fmod(280.460 + 36000.771 * tut1, 360.0);
    double meananomaly = fmod(357.5291092 + 35999.05034 * tut1, 360.0) * kDeg2Rad;
    if (meananomaly < 0.0) meananomaly += kTwoPi;
    double eclplong = (meanlong + 1.914666471 * sin(meananomaly) +
                       0.019994643 * sin(2.0 * meananomaly)) * kDeg2Rad;
    const double obliquity = (23.439291 - 0.0130042 * tut1) * kDeg2Rad;
    const double magr = 1.000140612 - 0.016708617 * cos(meananomaly) -
                        0.000139589 * cos(2.0 * meananomaly);   // AU
    const double rkm = magr * kAuKm;
    r[0] = rkm * cos(eclplong);
    r[1] = rkm * cos(obliquity) * sin(eclplong);
    r[2] = rkm * sin(obliquity) * sin(eclplong);
}

bool isSunlit(const double p[3], const double sun[3]) {
    // Ray p + t*d toward the Sun; in shadow if it hits the Earth sphere ahead.
    double d[3] = { sun[0] - p[0], sun[1] - p[1], sun[2] - p[2] };
    const double dl = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    d[0] /= dl; d[1] /= dl; d[2] /= dl;
    const double b = p[0] * d[0] + p[1] * d[1] + p[2] * d[2];
    const double c = p[0] * p[0] + p[1] * p[1] + p[2] * p[2] -
                     kShadowRadiusKm * kShadowRadiusKm;
    const double disc = b * b - c;
    if (disc < 0.0) return true;           // ray misses the Earth
    const double far = -b + sqrt(disc);
    return far <= 0.0;                     // Earth is behind us
}

double sunElevationDeg(const Observer& obs, double jd) {
    double sunEci[3], sunEcef[3];
    sunPositionEci(jd, sunEci);
    temeToEcef(sunEci, jd, sunEcef);
    LookAngles la;
    lookAnglesEcef(obs, sunEcef, la);
    return la.elDeg;
}

const char* compassPoint8(double azDeg) {
    static const char* const kNames[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    double a = fmod(azDeg, 360.0);
    if (a < 0.0) a += 360.0;
    int idx = (int)((a + 22.5) / 45.0) % 8;
    return kNames[idx];
}

}  // namespace orbit
