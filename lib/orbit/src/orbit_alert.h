#pragma once
// ─── Pass schedule helpers for the UI ───────────────────
// Pure functions over a sorted array of predicted passes, so the alert and
// "next pass" logic can be unit-tested on the host.

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "orbit_pass.h"

namespace orbit {

// Index of the first pass that has not finished by `now`, or -1.
// With visibleOnly, only passes whose visible window has not ended count.
int nextPassIndex(const Pass* passes, int count, int64_t now, bool visibleOnly);

struct PassAlert {
    bool    active;          // banner should be shown / display kept awake
    bool    underway;        // the visible window has started
    int     index;           // pass index (valid if active)
    int64_t secondsToStart;  // until visStart (<= 0 when underway)
};

// Alert for naked-eye visible passes: active from `leadSec` before the
// visible window starts until it ends.
PassAlert passAlert(const Pass* passes, int count, int64_t now, int64_t leadSec);

// Format a time as "Today 19:42", "Tomorrow 05:10" or "Sun 19:42" relative
// to `now`, in the process' local time zone (TZ). Returns `buf`.
char* formatLocalDayTime(int64_t t, int64_t now, char* buf, size_t bufSize);

// Format "HH:MM" in local time. Returns `buf`.
char* formatLocalTime(int64_t t, char* buf, size_t bufSize);

}  // namespace orbit
