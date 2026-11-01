#include "stats_screen.h"
#include "../config.h"
#include "../colors.h"
#include "../pass_text.h"
#include <cmath>
#include <string.h>
#include <time.h>

// ─── Layout Constants ───────────────────────────────────
static const int STAT_TITLE_Y = 20;
static const int DIVIDER_Y = 50;
static const int LEFT_COL_X = 30;
static const int RIGHT_COL_X = 430;
static const int ROW_HEIGHT = 70;
static const int VALUE_AREA_W = 340;  // max width of value text area
static const int VALUE_AREA_H = 30;   // height of value text

// ─── Helper to draw a stat row ──────────────────────────
// On incremental renders, clears the value area then immediately draws,
// keeping the erase-to-draw gap minimal so the DMA scan doesn't catch it.
static void drawStatRow(LGFX &lcd, const char* label, const char* value,
                        int col_x, int row_y, bool fullRedraw) {
    // Label (small, grey) — only on full redraw (labels don't change)
    if (fullRedraw) {
        lcd.setTextColor(COL_STAT_LABEL);
        lcd.setFont(&fonts::Font2);
        lcd.setTextDatum(TL_DATUM);
        lcd.drawString(label, col_x, row_y);
    }

    // Value (large, white) — clear then draw immediately for minimal flicker
    int vy = row_y + 25;
    lcd.fillRect(col_x, vy, VALUE_AREA_W, VALUE_AREA_H, COL_BG);
    lcd.setTextColor(COL_STAT_VALUE);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(TL_DATUM);
    lcd.drawString(value, col_x, vy);

    lcd.setFont(&fonts::Font0);
}

// ─── Visibility Indicator ───────────────────────────────
static uint16_t getVisibilityColor(const String &visibility) {
    if (visibility == "daylight") {
        return COL_DAYLIGHT;
    } else if (visibility == "eclipsed") {
        return COL_ECLIPSED;
    } else if (visibility == "penumbra") {
        return COL_PENUMBRA;
    }
    return COL_TEXT_DIM;
}

static const char* getVisibilityLabel(const String &visibility) {
    if (visibility == "daylight") {
        return "DAYLIGHT";
    } else if (visibility == "eclipsed") {
        return "ECLIPSED";
    } else if (visibility == "penumbra") {
        return "PENUMBRA";
    }
    return "UNKNOWN";
}

// ─── Next Pass Strip (bottom of the screen) ─────────────
static const int PASS_DIVIDER_Y = 348;
static const int PASS_TITLE_Y   = 356;
static const int PASS_LINE1_Y   = 374;
static const int PASS_LINE2_Y   = 404;

static void drawPassStrip(LGFX &lcd, const AppState &state, bool fullRedraw) {
    static PassSummaryText last;
    PassSummaryText txt;
    passSummary(state, time(nullptr), txt);

    // Content changes at most once a minute — only repaint when it does
    if (!fullRedraw && memcmp(&txt, &last, sizeof(txt)) == 0) return;
    last = txt;

    lcd.fillRect(0, PASS_DIVIDER_Y, DISPLAY_WIDTH, MAP_AREA_HEIGHT - PASS_DIVIDER_Y, COL_BG);
    lcd.drawLine(LEFT_COL_X, PASS_DIVIDER_Y, DISPLAY_WIDTH - LEFT_COL_X, PASS_DIVIDER_Y,
                 COL_DIVIDER);

    lcd.setTextDatum(TL_DATUM);
    lcd.setTextColor(COL_STAT_LABEL);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(txt.title, LEFT_COL_X, PASS_TITLE_Y);

    // Large line; fall back to the small font if it doesn't fit
    const int maxW = DISPLAY_WIDTH - 2 * LEFT_COL_X;
    lcd.setTextColor(COL_TEXT_ACCENT);
    lcd.setFont(&fonts::Font4);
    if (lcd.textWidth(txt.line1) > maxW) lcd.setFont(&fonts::Font2);
    lcd.drawString(txt.line1, LEFT_COL_X, PASS_LINE1_Y);

    lcd.setTextColor(COL_TEXT_DIM);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(txt.line2, LEFT_COL_X, PASS_LINE2_Y);
    lcd.setFont(&fonts::Font0);
}

void renderStatsScreen(LGFX &lcd, const AppState &state, bool fullRedraw) {
    // Title — only on full redraw (it never changes)
    if (fullRedraw) {
        lcd.setTextColor(COL_TEXT_ACCENT);
        lcd.setFont(&fonts::Font4);
        lcd.setTextDatum(TL_DATUM);
        lcd.drawString("ISS ORBITAL DATA", 20, STAT_TITLE_Y);

        // Horizontal divider
        lcd.drawLine(0, DIVIDER_Y, DISPLAY_WIDTH, DIVIDER_Y, COL_DIVIDER);
    }

    drawPassStrip(lcd, state, fullRedraw);

    if (!state.position.valid) {
        lcd.setTextColor(COL_TEXT_DIM);
        lcd.setFont(&fonts::Font4);
        lcd.setTextDatum(CC_DATUM);
        lcd.drawString("No data yet...", DISPLAY_WIDTH / 2, MAP_AREA_HEIGHT / 2);
        lcd.setFont(&fonts::Font0);
        lcd.setTextDatum(TL_DATUM);
        return;
    }

    // ─── Left Column: LAT, LON, ALTITUDE, VELOCITY ──────
    char buf[32];

    snprintf(buf, sizeof(buf), "%.4f", state.position.latitude);
    drawStatRow(lcd, "LATITUDE", buf, LEFT_COL_X, DIVIDER_Y + 20, fullRedraw);

    snprintf(buf, sizeof(buf), "%.4f", state.position.longitude);
    drawStatRow(lcd, "LONGITUDE", buf, LEFT_COL_X, DIVIDER_Y + 20 + ROW_HEIGHT, fullRedraw);

    snprintf(buf, sizeof(buf), "%.0f km", state.position.altitude);
    drawStatRow(lcd, "ALTITUDE", buf, LEFT_COL_X, DIVIDER_Y + 20 + ROW_HEIGHT * 2, fullRedraw);

    snprintf(buf, sizeof(buf), "%.0f km/h", state.position.velocity);
    drawStatRow(lcd, "VELOCITY", buf, LEFT_COL_X, DIVIDER_Y + 20 + ROW_HEIGHT * 3, fullRedraw);

    // ─── Right Column: VISIBILITY, FOOTPRINT, ORBIT #, AGE ──
    uint16_t vis_color = getVisibilityColor(state.position.visibility);
    const char* vis_label = getVisibilityLabel(state.position.visibility);

    // Visibility with colored indicator
    if (fullRedraw) {
        lcd.setTextColor(COL_STAT_LABEL);
        lcd.setFont(&fonts::Font2);
        lcd.setTextDatum(TL_DATUM);
        lcd.drawString("VISIBILITY", RIGHT_COL_X, DIVIDER_Y + 20);
    }

    // Clear visibility value area then draw immediately
    lcd.fillRect(RIGHT_COL_X - 8, DIVIDER_Y + 40, 260, 28, COL_BG);
    lcd.fillCircle(RIGHT_COL_X, DIVIDER_Y + 50, 6, vis_color);
    lcd.setTextColor(COL_STAT_VALUE);
    lcd.setFont(&fonts::Font4);
    lcd.drawString(vis_label, RIGHT_COL_X + 20, DIVIDER_Y + 35);

    // Footprint
    snprintf(buf, sizeof(buf), "%.0f km", state.position.footprint);
    drawStatRow(lcd, "FOOTPRINT", buf, RIGHT_COL_X, DIVIDER_Y + 20 + ROW_HEIGHT, fullRedraw);

    // Orbital period — computed from altitude using Kepler's third law
    // T = 2π × √((R+h)³ / μ)  where R=6371km, μ=398600.4418 km³/s²
    {
        float h = state.position.altitude;
        float r = 6371.0f + h;
        float mu = 398600.4418f;
        float T_sec = 6.2831853f * sqrtf((r * r * r) / mu);  // 2π
        float T_min = T_sec / 60.0f;
        snprintf(buf, sizeof(buf), "%.1f min", T_min);
    }
    drawStatRow(lcd, "PERIOD", buf, RIGHT_COL_X, DIVIDER_Y + 20 + ROW_HEIGHT * 2, fullRedraw);

    // Data age
    unsigned long age_ms = millis() - state.lastPositionUpdate;
    unsigned long age_s = age_ms / 1000;
    snprintf(buf, sizeof(buf), "%lu s ago", age_s);
    drawStatRow(lcd, "DATA AGE", buf, RIGHT_COL_X, DIVIDER_Y + 20 + ROW_HEIGHT * 3, fullRedraw);

    lcd.setFont(&fonts::Font0);
    lcd.setTextDatum(TL_DATUM);
}
