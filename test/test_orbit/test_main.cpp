// Host-side unit tests for lib/orbit (TLE parsing, SGP4, pass prediction).
// Run with:  pio test -e native
//
// Reference values in reference_data.h come from python-sgp4 (Vallado's
// reference SGP4) and Skyfield; regenerate with test/tools/gen_reference.py.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "orbit_alert.h"
#include "orbit_geo.h"
#include "orbit_pass.h"
#include "orbit_sgp4.h"
#include "orbit_tle.h"
#include "reference_data.h"

using namespace orbit;

static const double kPi = 3.14159265358979323846;
static const double kDeg = kPi / 180.0;

static const char* ISS_L1 = "1 25544U 98067A   19343.69339541  .00001764  00000-0  38792-4 0  9991";
static const char* ISS_L2 = "2 25544  51.6439 211.2001 0007417  17.6667  85.6398 15.50103472202482";

void setUp() {}
void tearDown() {}

// Smallest signed difference between two azimuths (degrees)
static double azDiff(double a, double b) {
    double d = fmod(a - b + 540.0, 360.0) - 180.0;
    return fabs(d);
}

// ═══════════════════════════════════════════════════════
//  TLE parsing
// ═══════════════════════════════════════════════════════

static void test_tle_parse_iss() {
    Tle t;
    TEST_ASSERT_EQUAL_INT(TLE_OK, tleParse(ISS_L1, ISS_L2, t));
    TEST_ASSERT_EQUAL_INT(25544, t.satnum);
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, 2458827.19339541, t.epochJd);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 51.6439 * kDeg, t.inclo);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 211.2001 * kDeg, t.nodeo);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0007417, t.ecco);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 17.6667 * kDeg, t.argpo);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 85.6398 * kDeg, t.mo);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 15.50103472 * 2.0 * kPi / 1440.0, t.noKozai);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.38792e-4, t.bstar);
    TEST_ASSERT_DOUBLE_WITHIN(1e-20, 0.0, t.nddot);
}

static void test_tle_parse_assumed_decimal_fields() {
    // 28350 from the SGP4 verification set has a non-zero nddot ("76267-5")
    // and B* "18678-3"
    const ref::PropCase& c = ref::kPropCases[3];
    TEST_ASSERT_EQUAL_STRING("28350", c.satnum);
    Tle t;
    TEST_ASSERT_EQUAL_INT(TLE_OK, tleParse(c.l1, c.l2, t));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.18678e-3, t.bstar);
    const double xpdotp = 1440.0 / (2.0 * kPi);
    TEST_ASSERT_DOUBLE_WITHIN(1e-20, 0.76267e-5 / (xpdotp * 1440.0 * 1440.0), t.nddot);

    // Negative mantissa: ISS line 1 with B* "-38792-4" (checksum fixed up)
    char l1[70];
    strcpy(l1, ISS_L1);
    l1[53] = '-';
    l1[68] = (char)('0' + tleChecksum(l1));
    TEST_ASSERT_EQUAL_INT(TLE_OK, tleParse(l1, ISS_L2, t));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, -0.38792e-4, t.bstar);
}

static void test_tle_accepts_trailing_whitespace() {
    char l1[96], l2[96];
    snprintf(l1, sizeof(l1), "%s  \r\n", ISS_L1);
    snprintf(l2, sizeof(l2), "%s\r", ISS_L2);
    Tle t;
    TEST_ASSERT_EQUAL_INT(TLE_OK, tleParse(l1, l2, t));
}

static void test_tle_rejects_bad_input() {
    Tle t;
    t.satnum = -42;  // sentinel: must stay untouched on failure
    char bad[96];

    TEST_ASSERT_EQUAL_INT(TLE_ERR_NULL, tleParse(nullptr, ISS_L2, t));
    TEST_ASSERT_EQUAL_INT(TLE_ERR_LENGTH, tleParse("1 25544U", ISS_L2, t));
    TEST_ASSERT_EQUAL_INT(TLE_ERR_LENGTH, tleParse("", "", t));

    // Flipped digit -> checksum mismatch
    strcpy(bad, ISS_L2);
    bad[10] = '2';
    TEST_ASSERT_EQUAL_INT(TLE_ERR_CHECKSUM, tleParse(ISS_L1, bad, t));

    // Lines swapped
    TEST_ASSERT_EQUAL_INT(TLE_ERR_FORMAT, tleParse(ISS_L2, ISS_L1, t));

    // Garbage of the right length
    memset(bad, 'x', 69);
    bad[69] = '\0';
    TEST_ASSERT_EQUAL_INT(TLE_ERR_FORMAT, tleParse(bad, ISS_L2, t));

    // Satellite number mismatch (fix up the checksum so only that differs)
    strcpy(bad, ISS_L2);
    bad[6] = '5';
    bad[68] = (char)('0' + tleChecksum(bad));
    TEST_ASSERT_EQUAL_INT(TLE_ERR_SATNUM, tleParse(ISS_L1, bad, t));

    // Non-digit inside the eccentricity field
    strcpy(bad, ISS_L2);
    bad[28] = 'Z';
    bad[68] = (char)('0' + tleChecksum(bad));
    TEST_ASSERT_EQUAL_INT(TLE_ERR_FORMAT, tleParse(ISS_L1, bad, t));

    // Inclination out of range (> 180)
    strcpy(bad, ISS_L2);
    memcpy(bad + 8, "251.6439", 8);
    bad[68] = (char)('0' + tleChecksum(bad));
    TEST_ASSERT_EQUAL_INT(TLE_ERR_RANGE, tleParse(ISS_L1, bad, t));

    TEST_ASSERT_EQUAL_INT(-42, t.satnum);
}

static void test_tle_checksum() {
    TEST_ASSERT_EQUAL_INT(1, tleChecksum(ISS_L1));
    TEST_ASSERT_EQUAL_INT(2, tleChecksum(ISS_L2));
}

static void test_tle_find_lines() {
    char text[256];
    snprintf(text, sizeof(text), "ISS (ZARYA)             \r\n%s\r\n%s\r\n", ISS_L1, ISS_L2);
    char l1[70], l2[70];
    TEST_ASSERT_TRUE(tleFindLines(text, l1, l2, sizeof(l1)));
    TEST_ASSERT_EQUAL_STRING(ISS_L1, l1);
    TEST_ASSERT_EQUAL_STRING(ISS_L2, l2);

    // Unix newlines, no name line
    snprintf(text, sizeof(text), "%s\n%s", ISS_L1, ISS_L2);
    TEST_ASSERT_TRUE(tleFindLines(text, l1, l2, sizeof(l1)));
    TEST_ASSERT_EQUAL_STRING(ISS_L2, l2);

    TEST_ASSERT_FALSE(tleFindLines("No GP data found", l1, l2, sizeof(l1)));
    TEST_ASSERT_FALSE(tleFindLines(ISS_L1, l1, l2, sizeof(l1)));      // line 2 missing
    TEST_ASSERT_FALSE(tleFindLines(text, l1, l2, 10));                // buffer too small
    TEST_ASSERT_FALSE(tleFindLines(nullptr, l1, l2, sizeof(l1)));

    // Over-long line is truncated to the buffer, never overflowed
    char longText[400];
    memset(longText, '9', sizeof(longText));
    longText[0] = '1'; longText[1] = ' ';
    longText[200] = '\n'; longText[201] = '2'; longText[202] = ' ';
    longText[399] = '\0';
    char s1[70], s2[70];
    TEST_ASSERT_TRUE(tleFindLines(longText, s1, s2, sizeof(s1)));
    TEST_ASSERT_EQUAL_size_t(69, strlen(s1));
    TEST_ASSERT_EQUAL_size_t(69, strlen(s2));
}

// ═══════════════════════════════════════════════════════
//  Time & frames
// ═══════════════════════════════════════════════════════

static void test_julian_date() {
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2451545.0, julianDate(2000, 1, 1, 12, 0, 0.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2440587.5, unixToJd(0.0));
    // JD in a double resolves ~40 us at current dates
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 1700000000.0, jdToUnix(unixToJd(1700000000.0)));
}

static void test_gmst_matches_reference() {
    for (const auto& g : ref::kGmst) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, g.gmst, gmst(g.jd));
    }
}

static void test_compass() {
    TEST_ASSERT_EQUAL_STRING("N",  compassPoint8(0.0));
    TEST_ASSERT_EQUAL_STRING("N",  compassPoint8(359.0));
    TEST_ASSERT_EQUAL_STRING("N",  compassPoint8(-10.0));
    TEST_ASSERT_EQUAL_STRING("NE", compassPoint8(30.0));
    TEST_ASSERT_EQUAL_STRING("E",  compassPoint8(90.0));
    TEST_ASSERT_EQUAL_STRING("SW", compassPoint8(225.0));
    TEST_ASSERT_EQUAL_STRING("NW", compassPoint8(320.0));
}

static void test_sunlit_geometry() {
    const double sun[3] = { 1.496e8, 0.0, 0.0 };
    const double day[3]   = { 6800.0, 0.0, 0.0 };     // between Earth and Sun
    const double night[3] = { -6800.0, 0.0, 0.0 };    // directly behind Earth
    const double limb[3]  = { -6800.0, 6500.0, 0.0 }; // behind, but outside shadow
    TEST_ASSERT_TRUE(isSunlit(day, sun));
    TEST_ASSERT_FALSE(isSunlit(night, sun));
    TEST_ASSERT_TRUE(isSunlit(limb, sun));
}

static void test_sun_elevation_noon_and_midnight() {
    // Melbourne, 2026-12-21 ~02:20 UTC is local solar noon near the solstice:
    // Sun elevation ~ 90 - (37.8 - 23.4) = ~75.6 deg; at midnight it is
    // ~ -(90 - 37.8 - 23.4) = -28.8 deg.
    Observer mel;
    observerInit(mel, -37.8136, 144.9631, 0.0);
    double noon = julianDate(2026, 12, 21, 2, 20, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 75.6, sunElevationDeg(mel, noon));
    TEST_ASSERT_DOUBLE_WITHIN(0.5, -28.8, sunElevationDeg(mel, noon + 0.5));
}

// ═══════════════════════════════════════════════════════
//  SGP4
// ═══════════════════════════════════════════════════════

static void test_sgp4_matches_reference_vectors() {
    char msg[96];
    for (int i = 0; i < ref::kPropCaseCount; i++) {
        const ref::PropCase& c = ref::kPropCases[i];
        Tle t;
        TEST_ASSERT_EQUAL_INT_MESSAGE(TLE_OK, tleParse(c.l1, c.l2, t), c.satnum);
        Sgp4 s;
        TEST_ASSERT_EQUAL_INT_MESSAGE(SGP4_OK, sgp4Init(t, s), c.satnum);
        TEST_ASSERT_TRUE(c.n > 0);
        for (int k = 0; k < c.n; k++) {
            const ref::Vec& v = c.rows[k];
            double r[3], vel[3];
            snprintf(msg, sizeof(msg), "sat %s t=%.0f", c.satnum, v.tsince);
            TEST_ASSERT_EQUAL_INT_MESSAGE(SGP4_OK, sgp4Propagate(s, v.tsince, r, vel), msg);
            for (int a = 0; a < 3; a++) {
                TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-5, v.r[a], r[a], msg);     // 1 cm
                TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-8, v.v[a], vel[a], msg);   // 10 um/s
            }
        }
    }
}

static void test_sgp4_rejects_deep_space() {
    Tle t;
    TEST_ASSERT_EQUAL_INT(TLE_OK, tleParse(ref::kDeepSpaceL1, ref::kDeepSpaceL2, t));
    Sgp4 s;
    TEST_ASSERT_EQUAL_INT(SGP4_ERR_DEEP_SPACE, sgp4Init(t, s));
}

// ═══════════════════════════════════════════════════════
//  Pass prediction vs Skyfield
// ═══════════════════════════════════════════════════════

static void yieldCounter(void* ctx) { (*(int*)ctx)++; }

static void test_passes_match_skyfield() {
    char msg[160];
    int totalMatched = 0, totalVisible = 0;

    for (int i = 0; i < ref::kPassCaseCount; i++) {
        const ref::PassCase& c = ref::kPassCases[i];
        Tle t;
        TEST_ASSERT_EQUAL_INT(TLE_OK, tleParse(c.l1, c.l2, t));
        Sgp4 s;
        TEST_ASSERT_EQUAL_INT(SGP4_OK, sgp4Init(t, s));
        Observer obs;
        observerInit(obs, c.lat, c.lon, c.altKm);

        PassOptions opt;
        opt.minElevationDeg = ref::kMinEl;
        opt.maxSunAltDeg = ref::kSunMaxAlt;

        Pass passes[48];
        int yields = 0;
        int n = predictPasses(s, obs, (int64_t)floor(c.start), (int64_t)c.end, opt,
                              passes, 48, yieldCounter, &yields);
        snprintf(msg, sizeof(msg), "%s @ %s", c.name, c.observer);
        TEST_ASSERT_TRUE_MESSAGE(n >= 0, msg);
        TEST_ASSERT_TRUE_MESSAGE(yields > 0, msg);

        // Every Skyfield pass must be found, with matching geometry
        for (int k = 0; k < c.n; k++) {
            const ref::RefPass& rp = c.passes[k];
            const Pass* match = nullptr;
            for (int j = 0; j < n; j++) {
                if (!passes[j].aosClipped && fabs((double)passes[j].aos - rp.aos) < 60.0) {
                    match = &passes[j];
                }
            }
            snprintf(msg, sizeof(msg), "%s @ %s pass %d (max %.2f deg)",
                     c.name, c.observer, k, rp.maxEl);
            TEST_ASSERT_NOT_NULL_MESSAGE(match, msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(3.0, rp.aos, (double)match->aos, msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(3.0, rp.los, (double)match->los, msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(5.0, rp.tca, (double)match->tca, msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.1, rp.maxEl, match->maxElDeg, msg);
            TEST_ASSERT_TRUE_MESSAGE(azDiff(rp.aosAz, match->aosAzDeg) < 0.5, msg);
            TEST_ASSERT_TRUE_MESSAGE(azDiff(rp.losAz, match->losAzDeg) < 0.5, msg);
            if (rp.maxEl < 60.0) {  // azimuth swings fast at culmination of high passes
                TEST_ASSERT_TRUE_MESSAGE(azDiff(rp.tcaAz, match->tcaAzDeg) < 0.5, msg);
            }
            TEST_ASSERT_EQUAL_MESSAGE(rp.visible, match->visible, msg);
            if (rp.visible) {
                TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(5.0, rp.visStart, (double)match->visStart, msg);
                TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(5.0, rp.visEnd, (double)match->visEnd, msg);
                totalVisible++;
            }
            totalMatched++;
        }

        // ...and we must not invent passes Skyfield didn't see (ignoring a
        // pass clipped at the start, or one still in progress at the end)
        int expectedMax = c.n;
        for (int j = 0; j < n; j++) {
            if (passes[j].aosClipped || (double)passes[j].los > c.end) expectedMax++;
        }
        snprintf(msg, sizeof(msg), "%s @ %s: %d passes vs %d", c.name, c.observer, n, c.n);
        TEST_ASSERT_TRUE_MESSAGE(n <= expectedMax, msg);

        // Basic sanity on every pass
        for (int j = 0; j < n; j++) {
            TEST_ASSERT_TRUE(passes[j].aos <= passes[j].tca);
            TEST_ASSERT_TRUE(passes[j].tca <= passes[j].los);
            TEST_ASSERT_TRUE(passes[j].maxElDeg >= ref::kMinEl - 0.01);
            if (j > 0) TEST_ASSERT_TRUE(passes[j].aos > passes[j - 1].los);
            if (passes[j].visible) {
                TEST_ASSERT_TRUE(passes[j].visStart >= passes[j].aos);
                TEST_ASSERT_TRUE(passes[j].visEnd <= passes[j].los);
                TEST_ASSERT_TRUE(passes[j].visStart <= passes[j].visEnd);
            }
        }
    }
    TEST_ASSERT_TRUE(totalMatched > 50);
    TEST_ASSERT_TRUE(totalVisible > 10);
}

static void test_pass_in_progress_is_clipped() {
    const ref::PassCase& c = ref::kPassCases[0];
    const ref::RefPass& rp = c.passes[0];
    Tle t;
    tleParse(c.l1, c.l2, t);
    Sgp4 s;
    sgp4Init(t, s);
    Observer obs;
    observerInit(obs, c.lat, c.lon, c.altKm);
    PassOptions opt;

    const int64_t mid = (int64_t)rp.tca;
    Pass p[2];
    int n = predictPasses(s, obs, mid, mid + 600, opt, p, 2);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_TRUE(p[0].aosClipped);
    TEST_ASSERT_EQUAL_INT64(mid, p[0].aos);
    TEST_ASSERT_DOUBLE_WITHIN(3.0, rp.los, (double)p[0].los);
}

static void test_pass_output_limit_and_empty_window() {
    const ref::PassCase& c = ref::kPassCases[0];
    Tle t;
    tleParse(c.l1, c.l2, t);
    Sgp4 s;
    sgp4Init(t, s);
    Observer obs;
    observerInit(obs, c.lat, c.lon, c.altKm);
    PassOptions opt;

    Pass p[3];
    TEST_ASSERT_EQUAL_INT(3, predictPasses(s, obs, (int64_t)c.start, (int64_t)c.end, opt, p, 3));
    TEST_ASSERT_EQUAL_INT(0, predictPasses(s, obs, (int64_t)c.start, (int64_t)c.start, opt, p, 3));
    // No pass between the end of the first and the start of the second
    TEST_ASSERT_EQUAL_INT(0, predictPasses(s, obs, (int64_t)c.passes[0].los + 30,
                                           (int64_t)c.passes[1].aos - 30, opt, p, 3));
}

static void test_pass_prediction_reports_sgp4_failure() {
    // 28350 is a re-entering object: SGP4 fails a couple of days after epoch
    const ref::PropCase& c = ref::kPropCases[3];
    Tle t;
    tleParse(c.l1, c.l2, t);
    Sgp4 s;
    TEST_ASSERT_EQUAL_INT(SGP4_OK, sgp4Init(t, s));
    Observer obs;
    observerInit(obs, -37.8136, 144.9631, 0.0);
    PassOptions opt;
    int64_t start = (int64_t)jdToUnix(t.epochJd + 3.0);
    Pass p[4];
    Sgp4Error err = SGP4_OK;
    int n = predictPasses(s, obs, start, start + 86400, opt, p, 4, nullptr, nullptr, &err);
    TEST_ASSERT_EQUAL_INT(-1, n);
    TEST_ASSERT_EQUAL_INT(SGP4_ERR_ECCENTRICITY, err);
}

// ═══════════════════════════════════════════════════════
//  Alert / schedule helpers
// ═══════════════════════════════════════════════════════

static Pass makePass(int64_t aos, int64_t los, bool visible, int64_t vs = 0, int64_t ve = 0) {
    Pass p = {};
    p.aos = aos;
    p.tca = (aos + los) / 2;
    p.los = los;
    p.maxElDeg = 40.0f;
    p.visible = visible;
    p.visStart = vs;
    p.visEnd = ve;
    return p;
}

static void test_next_pass_index() {
    Pass p[3] = {
        makePass(1000, 1500, false),
        makePass(7000, 7600, true, 7100, 7400),
        makePass(13000, 13600, true, 13000, 13600),
    };
    TEST_ASSERT_EQUAL_INT(0, nextPassIndex(p, 3, 0, false));
    TEST_ASSERT_EQUAL_INT(0, nextPassIndex(p, 3, 1499, false));
    TEST_ASSERT_EQUAL_INT(1, nextPassIndex(p, 3, 1500, false));
    TEST_ASSERT_EQUAL_INT(1, nextPassIndex(p, 3, 0, true));
    TEST_ASSERT_EQUAL_INT(2, nextPassIndex(p, 3, 7400, true));   // visible part over
    TEST_ASSERT_EQUAL_INT(-1, nextPassIndex(p, 3, 13600, true));
    TEST_ASSERT_EQUAL_INT(-1, nextPassIndex(p, 0, 0, false));
    TEST_ASSERT_EQUAL_INT(-1, nextPassIndex(nullptr, 3, 0, false));
}

static void test_pass_alert_window() {
    Pass p[2] = {
        makePass(1000, 1500, false),              // never alerts
        makePass(7000, 7600, true, 7100, 7400),
    };
    PassAlert a = passAlert(p, 2, 1200, 300);
    TEST_ASSERT_FALSE(a.active);                  // invisible pass in progress
    a = passAlert(p, 2, 7100 - 301, 300);
    TEST_ASSERT_FALSE(a.active);
    a = passAlert(p, 2, 7100 - 300, 300);
    TEST_ASSERT_TRUE(a.active);
    TEST_ASSERT_FALSE(a.underway);
    TEST_ASSERT_EQUAL_INT(1, a.index);
    TEST_ASSERT_EQUAL_INT64(300, a.secondsToStart);
    a = passAlert(p, 2, 7100, 300);
    TEST_ASSERT_TRUE(a.active);
    TEST_ASSERT_TRUE(a.underway);
    a = passAlert(p, 2, 7399, 300);
    TEST_ASSERT_TRUE(a.underway);
    a = passAlert(p, 2, 7400, 300);
    TEST_ASSERT_FALSE(a.active);
}

static void test_format_local_day_time() {
    setenv("TZ", "AEST-10AEDT,M10.1.0,M4.1.0/3", 1);   // Melbourne (config.h)
    tzset();
    // 2026-11-01 09:00 UTC = 20:00 AEDT (UTC+11), a Sunday
    const int64_t now = (int64_t)jdToUnix(julianDate(2026, 11, 1, 9, 0, 0.0));
    char buf[24];
    TEST_ASSERT_EQUAL_STRING("Today 20:42", formatLocalDayTime(now + 42 * 60, now, buf, sizeof(buf)));
    // 4 h later is 00:00 Monday local
    TEST_ASSERT_EQUAL_STRING("Tomorrow 00:00", formatLocalDayTime(now + 4 * 3600, now, buf, sizeof(buf)));
    // Sun 20:00 + 2 days - 14h50m = Tue 05:10
    TEST_ASSERT_EQUAL_STRING("Tue 05:10",
                             formatLocalDayTime(now + 2 * 86400 - (14 * 3600 + 50 * 60), now, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("20:00", formatLocalTime(now, buf, sizeof(buf)));
    // Tiny buffers are truncated, never overflowed
    char small[6];
    formatLocalDayTime(now, now, small, sizeof(small));
    TEST_ASSERT_EQUAL_size_t(5, strlen(small));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tle_parse_iss);
    RUN_TEST(test_tle_parse_assumed_decimal_fields);
    RUN_TEST(test_tle_accepts_trailing_whitespace);
    RUN_TEST(test_tle_rejects_bad_input);
    RUN_TEST(test_tle_checksum);
    RUN_TEST(test_tle_find_lines);
    RUN_TEST(test_julian_date);
    RUN_TEST(test_gmst_matches_reference);
    RUN_TEST(test_compass);
    RUN_TEST(test_sunlit_geometry);
    RUN_TEST(test_sun_elevation_noon_and_midnight);
    RUN_TEST(test_sgp4_matches_reference_vectors);
    RUN_TEST(test_sgp4_rejects_deep_space);
    RUN_TEST(test_passes_match_skyfield);
    RUN_TEST(test_pass_in_progress_is_clipped);
    RUN_TEST(test_pass_output_limit_and_empty_window);
    RUN_TEST(test_pass_prediction_reports_sgp4_failure);
    RUN_TEST(test_next_pass_index);
    RUN_TEST(test_pass_alert_window);
    RUN_TEST(test_format_local_day_time);
    return UNITY_END();
}
