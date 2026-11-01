#include "pass_text.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <orbit_alert.h>
#include <orbit_geo.h>

using orbit::compassPoint8;
using orbit::formatLocalDayTime;
using orbit::formatLocalTime;

static bool statusMessage(const PassPredictions &pp, PassSummaryText &out) {
    const char *msg = nullptr;
    switch (pp.status) {
        case PASS_STATUS_NO_TLE:    msg = "Fetching orbital elements..."; break;
        case PASS_STATUS_NO_TIME:   msg = "Waiting for time sync..."; break;
        case PASS_STATUS_STALE_TLE: msg = "Orbital elements out of date - check network"; break;
        case PASS_STATUS_ERROR:     msg = "Prediction failed - will retry"; break;
        case PASS_STATUS_OK:        return false;
    }
    snprintf(out.title, sizeof(out.title), "NEXT PASS");
    snprintf(out.line1, sizeof(out.line1), "%s", msg ? msg : "");
    out.line2[0] = '\0';
    return true;
}

void passSummary(const AppState &state, time_t now, PassSummaryText &out) {
    memset(&out, 0, sizeof(out));
    const PassPredictions &pp = state.passes;
    if (statusMessage(pp, out)) return;

    char when[24], t1[8], t2[8], t3[8];
    int vi = orbit::nextPassIndex(pp.passes, pp.count, (int64_t)now, true);
    if (vi >= 0) {
        const orbit::Pass &p = pp.passes[vi];
        snprintf(out.title, sizeof(out.title), "NEXT VISIBLE PASS");
        if (p.visStart <= (int64_t)now) {
            snprintf(when, sizeof(when), "NOW");
        } else {
            formatLocalDayTime(p.visStart, (int64_t)now, when, sizeof(when));
        }
        snprintf(out.line1, sizeof(out.line1), "%s   %s to %s   max %.0f deg",
                 when, compassPoint8(p.visStartAzDeg), compassPoint8(p.visEndAzDeg),
                 p.maxElDeg);
        long visMin = (long)((p.visEnd - p.visStart + 30) / 60);
        snprintf(out.line2, sizeof(out.line2),
                 "Visible %s-%s (%ld min), highest in the %s.  Above %.0f deg %s-%s.",
                 formatLocalTime(p.visStart, t1, sizeof(t1)),
                 formatLocalTime(p.visEnd, t2, sizeof(t2)), visMin,
                 compassPoint8(p.tcaAzDeg), PASS_MIN_ELEVATION_DEG,
                 formatLocalTime(p.aos, t3, sizeof(t3)),
                 formatLocalTime(p.los, when, sizeof(when)));
        return;
    }

    int ni = orbit::nextPassIndex(pp.passes, pp.count, (int64_t)now, false);
    if (ni >= 0) {
        const orbit::Pass &p = pp.passes[ni];
        snprintf(out.title, sizeof(out.title), "NEXT PASS (NOT VISIBLE)");
        if (p.aos <= (int64_t)now) {
            snprintf(when, sizeof(when), "NOW");
        } else {
            formatLocalDayTime(p.aos, (int64_t)now, when, sizeof(when));
        }
        snprintf(out.line1, sizeof(out.line1), "%s   %s to %s   max %.0f deg",
                 when, compassPoint8(p.aosAzDeg), compassPoint8(p.losAzDeg), p.maxElDeg);
        snprintf(out.line2, sizeof(out.line2),
                 "No naked-eye passes in the next %dh (daylight or ISS in shadow).",
                 PASS_PREDICT_HOURS);
        return;
    }

    snprintf(out.title, sizeof(out.title), "NEXT PASS");
    snprintf(out.line1, sizeof(out.line1), "None above %.0f deg in the next %dh",
             PASS_MIN_ELEVATION_DEG, PASS_PREDICT_HOURS);
}

void passShortText(const AppState &state, time_t now, char *buf, size_t bufSize) {
    if (!buf || bufSize == 0) return;
    const PassPredictions &pp = state.passes;
    if (pp.status == PASS_STATUS_OK) {
        int vi = orbit::nextPassIndex(pp.passes, pp.count, (int64_t)now, true);
        if (vi >= 0) {
            const orbit::Pass &p = pp.passes[vi];
            char when[24];
            if (p.visStart <= (int64_t)now) snprintf(when, sizeof(when), "now");
            else formatLocalDayTime(p.visStart, (int64_t)now, when, sizeof(when));
            snprintf(buf, bufSize, "Next visible: %s (max %.0f deg)", when, p.maxElDeg);
            return;
        }
        snprintf(buf, bufSize, "No visible passes in the next %dh", PASS_PREDICT_HOURS);
        return;
    }
    snprintf(buf, bufSize, "Notify when ISS is visible");
}

bool passAlertText(const AppState &state, time_t now, char *buf, size_t bufSize) {
    if (!buf || bufSize == 0) return false;
    buf[0] = '\0';
    const PassPredictions &pp = state.passes;
    if (!state.settings.passNotifyEnabled || pp.status != PASS_STATUS_OK) return false;

    orbit::PassAlert a = orbit::passAlert(pp.passes, pp.count, (int64_t)now, PASS_ALERT_LEAD_S);
    if (!a.active) return false;
    const orbit::Pass &p = pp.passes[a.index];
    char t[8];
    if (a.underway) {
        snprintf(buf, bufSize, "ISS VISIBLE NOW: %s to %s, max %.0f deg, until %s",
                 compassPoint8(p.visStartAzDeg), compassPoint8(p.visEndAzDeg),
                 p.maxElDeg, formatLocalTime(p.visEnd, t, sizeof(t)));
    } else {
        long mins = (long)((a.secondsToStart + 59) / 60);
        snprintf(buf, bufSize, "ISS IN %ld MIN: look %s, max %.0f deg, at %s",
                 mins, compassPoint8(p.visStartAzDeg), p.maxElDeg,
                 formatLocalTime(p.visStart, t, sizeof(t)));
    }
    return true;
}
