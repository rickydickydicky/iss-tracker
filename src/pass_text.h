#pragma once
// ─── Human-readable pass prediction text for the screens ─
#include <stddef.h>
#include <time.h>
#include "iss_data.h"

struct PassSummaryText {
    char title[32];    // e.g. "NEXT VISIBLE PASS"
    char line1[64];    // e.g. "Today 19:42   NW to SE   max 54 deg"
    char line2[96];    // detail line (dim)
};

/// Summary of the next visible pass (or the next pass, or a status message).
void passSummary(const AppState &state, time_t now, PassSummaryText &out);

/// Short one-liner for the Setup screen, e.g. "Next visible: Today 19:42 (max 54 deg)".
void passShortText(const AppState &state, time_t now, char *buf, size_t bufSize);

/// Alert banner text. Returns false (and leaves buf empty) if no alert is
/// active — alerts require passNotifyEnabled and a visible pass within
/// PASS_ALERT_LEAD_S or underway.
bool passAlertText(const AppState &state, time_t now, char *buf, size_t bufSize);
