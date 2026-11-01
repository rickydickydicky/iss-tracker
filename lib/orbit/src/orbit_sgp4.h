#pragma once
// ─── SGP4 propagator (near-earth only) ──────────────────
// A faithful port of the near-earth branch of David Vallado's SGP4
// (the reference implementation used by python-sgp4 / Skyfield), using
// WGS-72 constants and the "improved" ('i') operation mode.
//
// Deep-space objects (orbital period >= 225 min, i.e. SDP4) are NOT
// supported: sgp4Init() rejects them. The ISS period is ~92 min.
//
// Everything is double precision. On the ESP32-S3 doubles are emulated in
// software (~0.2 ms per propagation), which is fine for a background task.

#include "orbit_tle.h"

namespace orbit {

struct Sgp4 {
    double epochJd;       // TLE epoch (Julian Date, UTC)
    // Elements (copied from TLE)
    double bstar, ecco, inclo, nodeo, argpo, mo, noUnkozai;
    // Initialised coefficients
    int    isimp;
    double aycof, con41, cc1, cc4, cc5, d2, d3, d4, delmo, eta, argpdot,
           omgcof, sinmao, t2cof, t3cof, t4cof, t5cof, x1mth2, x7thm1,
           mdot, nodedot, xlcof, xmcof, nodecf;
};

enum Sgp4Error {
    SGP4_OK = 0,
    SGP4_ERR_ECCENTRICITY = 1,   // mean eccentricity out of range
    SGP4_ERR_MEAN_MOTION  = 2,   // mean motion <= 0
    SGP4_ERR_SEMILATUS    = 4,   // semi-latus rectum < 0
    SGP4_ERR_DECAYED      = 6,   // satellite has decayed (r < 1 earth radius)
    SGP4_ERR_DEEP_SPACE   = 10,  // period >= 225 min: SDP4 not implemented
};

// Initialise propagator state from parsed elements.
Sgp4Error sgp4Init(const Tle& tle, Sgp4& s);

// Propagate to `tsinceMin` minutes after the TLE epoch.
// Outputs position (km) and velocity (km/s) in the TEME frame.
Sgp4Error sgp4Propagate(const Sgp4& s, double tsinceMin, double r[3], double v[3]);

// Convenience: propagate to an absolute Julian Date (UTC).
inline Sgp4Error sgp4PropagateJd(const Sgp4& s, double jd, double r[3], double v[3]) {
    return sgp4Propagate(s, (jd - s.epochJd) * 1440.0, r, v);
}

// WGS-72 constants used by SGP4
namespace wgs72 {
    const double kRadiusKm = 6378.135;
    const double kMu       = 398600.8;
}

}  // namespace orbit
