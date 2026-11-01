#include "orbit_alert.h"

#include <math.h>
#include <stdio.h>

namespace orbit {

int nextPassIndex(const Pass* passes, int count, int64_t now, bool visibleOnly) {
    if (!passes) return -1;
    for (int i = 0; i < count; i++) {
        const Pass& p = passes[i];
        if (visibleOnly) {
            if (p.visible && p.visEnd > now) return i;
        } else if (p.los > now) {
            return i;
        }
    }
    return -1;
}

PassAlert passAlert(const Pass* passes, int count, int64_t now, int64_t leadSec) {
    PassAlert a = { false, false, -1, 0 };
    int i = nextPassIndex(passes, count, now, true);
    if (i < 0) return a;
    const Pass& p = passes[i];
    const int64_t toStart = p.visStart - now;
    if (toStart <= leadSec) {
        a.active = true;
        a.underway = toStart <= 0;
        a.index = i;
        a.secondsToStart = toStart;
    }
    return a;
}

static bool localTm(int64_t t, struct tm& out) {
    time_t tt = (time_t)t;
    return localtime_r(&tt, &out) != nullptr;
}

char* formatLocalDayTime(int64_t t, int64_t now, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return buf;
    struct tm a, b;
    if (!localTm(t, a) || !localTm(now, b)) {
        buf[0] = '\0';
        return buf;
    }
    // Calendar-day difference via the day-of-year of each date
    struct tm b1 = b;
    b1.tm_hour = 12; b1.tm_min = 0; b1.tm_sec = 0;
    struct tm a1 = a;
    a1.tm_hour = 12; a1.tm_min = 0; a1.tm_sec = 0;
    const long dayDiff = lround(difftime(mktime(&a1), mktime(&b1)) / 86400.0);

    char hm[8];
    strftime(hm, sizeof(hm), "%H:%M", &a);
    if (dayDiff == 0) {
        snprintf(buf, bufSize, "Today %s", hm);
    } else if (dayDiff == 1) {
        snprintf(buf, bufSize, "Tomorrow %s", hm);
    } else {
        char day[8];
        strftime(day, sizeof(day), "%a", &a);
        snprintf(buf, bufSize, "%s %s", day, hm);
    }
    return buf;
}

char* formatLocalTime(int64_t t, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return buf;
    struct tm a;
    if (!localTm(t, a)) {
        buf[0] = '\0';
        return buf;
    }
    strftime(buf, bufSize, "%H:%M", &a);
    return buf;
}

}  // namespace orbit
