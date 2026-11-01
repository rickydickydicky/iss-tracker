#pragma once
// ─── Two-Line Element (TLE) parsing ─────────────────────
// Pure C++ (no Arduino dependencies) so it can be unit-tested on the host.
// All parsing is bounds-checked: lines are treated as untrusted network input.

#include <stddef.h>
#include <stdint.h>

namespace orbit {

// Mean orbital elements as published in a TLE, already converted to the
// units SGP4 expects (radians, radians/minute) and a Julian Date epoch.
struct Tle {
    long   satnum;         // NORAD catalogue number (e.g. 25544 for the ISS)
    double epochJd;        // Epoch as a full Julian Date (UTC)
    double bstar;          // Drag term (1/earth radii)
    double ndot;           // First derivative of mean motion (rad/min^2)
    double nddot;          // Second derivative of mean motion (rad/min^3)
    double ecco;           // Eccentricity
    double inclo;          // Inclination (rad)
    double nodeo;          // Right ascension of ascending node (rad)
    double argpo;          // Argument of perigee (rad)
    double mo;             // Mean anomaly (rad)
    double noKozai;        // Mean motion, Kozai form (rad/min)
};

enum TleError {
    TLE_OK = 0,
    TLE_ERR_NULL,          // a line pointer was null
    TLE_ERR_LENGTH,        // line shorter than 69 characters
    TLE_ERR_FORMAT,        // line does not have the fixed-column layout
    TLE_ERR_CHECKSUM,      // modulo-10 checksum mismatch
    TLE_ERR_SATNUM,        // catalogue numbers in line 1 and 2 differ
    TLE_ERR_RANGE,         // a field is outside its physical range
};

// Human-readable name for a TleError (static string).
const char* tleErrorString(TleError err);

// Modulo-10 checksum of the first 68 characters of a TLE line
// (digits count their value, '-' counts 1, everything else 0).
int tleChecksum(const char* line);

// Parse a TLE. Lines may have trailing whitespace / CR. Returns TLE_OK and
// fills `out` on success; `out` is untouched on failure.
TleError tleParse(const char* line1, const char* line2, Tle& out);

// Find the first "1 ..." / "2 ..." line pair inside a block of text
// (e.g. a CelesTrak 3-line response). Copies each line (without line
// terminator, max 69 chars + NUL) into the caller's buffers, which must be
// at least `bufSize` bytes (>= 70). Returns true if both lines were found.
bool tleFindLines(const char* text, char* line1, char* line2, size_t bufSize);

// Convert a calendar date (UTC) to a Julian Date.
double julianDate(int year, int month, int day, int hour, int minute, double sec);

// Unix time (seconds, may be fractional) <-> Julian Date.
inline double unixToJd(double unixSec) { return unixSec / 86400.0 + 2440587.5; }
inline double jdToUnix(double jd)      { return (jd - 2440587.5) * 86400.0; }

}  // namespace orbit
