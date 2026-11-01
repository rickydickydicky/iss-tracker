#include "orbit_tle.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace orbit {

static const double kPi    = 3.14159265358979323846;
static const double kDeg2Rad = kPi / 180.0;
static const double kXpdotp  = 1440.0 / (2.0 * kPi);   // rev/day -> rad/min

static const size_t kTleLineLen = 69;

const char* tleErrorString(TleError err) {
    switch (err) {
        case TLE_OK:           return "ok";
        case TLE_ERR_NULL:     return "null line";
        case TLE_ERR_LENGTH:   return "line too short";
        case TLE_ERR_FORMAT:   return "bad format";
        case TLE_ERR_CHECKSUM: return "checksum mismatch";
        case TLE_ERR_SATNUM:   return "satellite number mismatch";
        case TLE_ERR_RANGE:    return "element out of range";
    }
    return "unknown";
}

int tleChecksum(const char* line) {
    int sum = 0;
    for (size_t i = 0; i < 68 && line[i] != '\0'; i++) {
        char c = line[i];
        if (c >= '0' && c <= '9') sum += c - '0';
        else if (c == '-')        sum += 1;
    }
    return sum % 10;
}

// Length of a line ignoring trailing whitespace / CR / LF.
static size_t trimmedLength(const char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\r' ||
                     s[n - 1] == '\n' || s[n - 1] == '\t')) {
        n--;
    }
    return n;
}

// Copy columns [start, end) into buf, trimming spaces at both ends.
static void field(const char* line, size_t start, size_t end, char* buf, size_t bufSize) {
    size_t n = 0;
    for (size_t i = start; i < end && n + 1 < bufSize; i++) buf[n++] = line[i];
    buf[n] = '\0';
    // trim
    size_t s = 0;
    while (buf[s] == ' ') s++;
    if (s > 0) memmove(buf, buf + s, n - s + 1);
    n = strlen(buf);
    while (n > 0 && buf[n - 1] == ' ') buf[--n] = '\0';
}

// Parse a decimal field; fails on empty input or trailing garbage.
static bool parseDouble(const char* line, size_t start, size_t end, double& out) {
    char buf[24];
    field(line, start, end, buf, sizeof(buf));
    if (buf[0] == '\0') return false;
    char* endp = nullptr;
    double v = strtod(buf, &endp);
    if (endp == buf || *endp != '\0') return false;
    if (!isfinite(v)) return false;
    out = v;
    return true;
}

static bool parseLong(const char* line, size_t start, size_t end, long& out) {
    char buf[16];
    field(line, start, end, buf, sizeof(buf));
    if (buf[0] == '\0') return false;
    char* endp = nullptr;
    long v = strtol(buf, &endp, 10);
    if (endp == buf || *endp != '\0') return false;
    out = v;
    return true;
}

// "Assumed decimal point" fields such as " 38792-4" (= 0.38792e-4) or
// "-11606-4". `col` is the sign column; 5 mantissa digits follow, then a
// signed one-digit exponent.
static bool parseAssumedDecimal(const char* line, size_t col, double& out) {
    char sign = line[col];
    if (sign != ' ' && sign != '+' && sign != '-') return false;
    double mant = 0.0;
    for (size_t i = col + 1; i < col + 6; i++) {
        char c = line[i];
        if (c == ' ') c = '0';
        if (c < '0' || c > '9') return false;
        mant = mant * 10.0 + (c - '0');
    }
    mant /= 100000.0;
    char es = line[col + 6];
    char ed = line[col + 7];
    if ((es != '+' && es != '-' && es != ' ') || ed < '0' || ed > '9') return false;
    int ex = ed - '0';
    if (es == '-') ex = -ex;
    double v = mant * pow(10.0, ex);
    out = (sign == '-') ? -v : v;
    return true;
}

double julianDate(int year, int month, int day, int hour, int minute, double sec) {
    // Vallado, "Fundamentals of Astrodynamics", algorithm 14 (valid 1900-2100).
    return 367.0 * year -
           floor((7 * (year + floor((month + 9) / 12.0))) * 0.25) +
           floor(275 * month / 9.0) +
           day + 1721013.5 +
           ((sec / 60.0 + minute) / 60.0 + hour) / 24.0;
}

TleError tleParse(const char* line1, const char* line2, Tle& out) {
    if (!line1 || !line2) return TLE_ERR_NULL;

    size_t len1 = trimmedLength(line1);
    size_t len2 = trimmedLength(line2);
    if (len1 < kTleLineLen || len2 < kTleLineLen) return TLE_ERR_LENGTH;

    // Fixed-column sanity checks (same columns python-sgp4 checks)
    if (line1[0] != '1' || line1[1] != ' ' || line1[8] != ' ' || line1[23] != '.' ||
        line1[32] != ' ' || line1[43] != ' ' || line1[52] != ' ' ||
        line1[61] != ' ' || line1[63] != ' ') {
        return TLE_ERR_FORMAT;
    }
    if (line2[0] != '2' || line2[1] != ' ' || line2[7] != ' ' || line2[11] != '.' ||
        line2[16] != ' ' || line2[20] != '.' || line2[25] != ' ' || line2[33] != ' ' ||
        line2[37] != '.' || line2[42] != ' ' || line2[46] != '.' || line2[51] != ' ') {
        return TLE_ERR_FORMAT;
    }
    if (!isdigit((unsigned char)line1[68]) || !isdigit((unsigned char)line2[68])) {
        return TLE_ERR_FORMAT;
    }
    if (tleChecksum(line1) != line1[68] - '0' || tleChecksum(line2) != line2[68] - '0') {
        return TLE_ERR_CHECKSUM;
    }

    Tle t;
    long satnum2 = 0, yy = 0;
    double epochDays = 0, ndot = 0, nddot = 0, bstar = 0;
    double incl = 0, raan = 0, argp = 0, mo = 0, nRevPerDay = 0;

    if (!parseLong(line1, 2, 7, t.satnum) || !parseLong(line2, 2, 7, satnum2)) {
        return TLE_ERR_FORMAT;   // includes Alpha-5 numbers, which we don't need
    }
    if (t.satnum != satnum2) return TLE_ERR_SATNUM;

    if (!parseLong(line1, 18, 20, yy) ||
        !parseDouble(line1, 20, 32, epochDays) ||
        !parseDouble(line1, 33, 43, ndot) ||
        !parseAssumedDecimal(line1, 44, nddot) ||
        !parseAssumedDecimal(line1, 53, bstar)) {
        return TLE_ERR_FORMAT;
    }

    // Eccentricity has an assumed leading "0."
    double ecc = 0.0;
    for (size_t i = 26; i < 33; i++) {
        char c = line2[i] == ' ' ? '0' : line2[i];
        if (c < '0' || c > '9') return TLE_ERR_FORMAT;
        ecc = ecc * 10.0 + (c - '0');
    }
    ecc /= 1.0e7;

    if (!parseDouble(line2, 8, 16, incl) ||
        !parseDouble(line2, 17, 25, raan) ||
        !parseDouble(line2, 34, 42, argp) ||
        !parseDouble(line2, 43, 51, mo) ||
        !parseDouble(line2, 52, 63, nRevPerDay)) {
        return TLE_ERR_FORMAT;
    }

    if (yy < 0 || yy > 99 || epochDays < 1.0 || epochDays >= 367.0 ||
        incl < 0.0 || incl > 180.0 || raan < 0.0 || raan >= 360.0 ||
        argp < 0.0 || argp >= 360.0 || mo < 0.0 || mo >= 360.0 ||
        ecc >= 1.0 || nRevPerDay <= 0.0 || nRevPerDay > 20.0) {
        return TLE_ERR_RANGE;
    }

    int year = (yy < 57) ? (int)(2000 + yy) : (int)(1900 + yy);
    t.epochJd = julianDate(year, 1, 1, 0, 0, 0.0) - 1.0 + epochDays;

    // Convert to SGP4 units (same as python-sgp4 twoline2rv)
    t.noKozai = nRevPerDay / kXpdotp;                     // rad/min
    t.ndot    = ndot / (kXpdotp * 1440.0);
    t.nddot   = nddot / (kXpdotp * 1440.0 * 1440.0);
    t.bstar   = bstar;
    t.ecco    = ecc;
    t.inclo   = incl * kDeg2Rad;
    t.nodeo   = raan * kDeg2Rad;
    t.argpo   = argp * kDeg2Rad;
    t.mo      = mo * kDeg2Rad;

    out = t;
    return TLE_OK;
}

// Copy one line (up to the next CR/LF) into dst. Returns pointer past it.
static const char* copyLine(const char* p, char* dst, size_t bufSize) {
    size_t n = 0;
    while (*p && *p != '\n' && *p != '\r') {
        if (n + 1 < bufSize) dst[n++] = *p;
        p++;
    }
    dst[n] = '\0';
    while (*p == '\r' || *p == '\n') p++;
    return p;
}

bool tleFindLines(const char* text, char* line1, char* line2, size_t bufSize) {
    if (!text || !line1 || !line2 || bufSize < kTleLineLen + 1) return false;
    line1[0] = line2[0] = '\0';

    const char* p = text;
    char cur[96];
    bool have1 = false;
    while (*p) {
        // skip leading spaces on a line
        while (*p == ' ' || *p == '\t') p++;
        p = copyLine(p, cur, sizeof(cur));
        if (cur[0] == '1' && cur[1] == ' ') {
            strncpy(line1, cur, bufSize - 1);
            line1[bufSize - 1] = '\0';
            have1 = true;
        } else if (have1 && cur[0] == '2' && cur[1] == ' ') {
            strncpy(line2, cur, bufSize - 1);
            line2[bufSize - 1] = '\0';
            return true;
        }
    }
    line1[0] = line2[0] = '\0';
    return false;
}

}  // namespace orbit
