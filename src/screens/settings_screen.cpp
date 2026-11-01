#include "settings_screen.h"
#include "../config.h"
#include "../colors.h"
#include "../settings.h"
#include "../pass_text.h"
#include <time.h>

// ─── Layout ─────────────────────────────────────────────
static const int TITLE_Y    = 20;
static const int DIVIDER_Y  = 50;
static const int CONTENT_Y  = DIVIDER_Y + 12;
static const int ROW_H      = 72;    // height per setting row
static const int ROW_PAD    = 16;    // left padding
static const int TOGGLE_W   = 60;    // toggle switch width
static const int TOGGLE_H   = 28;    // toggle switch height
static const int TOGGLE_X   = DISPLAY_WIDTH - ROW_PAD - TOGGLE_W;

// ─── Setting row indices ────────────────────────────────
static const int ROW_SLEEP       = 0;
static const int ROW_SLEEP_HOURS = 1;
static const int ROW_PASS_NOTIFY = 2;
static const int ROW_HOME        = 3;
static const int ROW_ABOUT       = 4;
static const int ROW_COUNT       = 5;

// ─── Sleep hour presets ─────────────────────────────────
struct HourPreset { uint8_t start; uint8_t end; const char* label; };
static const HourPreset PRESETS[] = {
    { 22, 7,  "10 PM - 7 AM" },
    { 23, 6,  "11 PM - 6 AM" },
    { 21, 7,  "9 PM - 7 AM"  },
    { 0,  6,  "12 AM - 6 AM" },
};
static const int PRESET_COUNT = 4;

// Track if settings changed (to trigger NVS save)
static bool settingsDirty = false;

// ═══════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════

static int rowY(int row) {
    return CONTENT_Y + row * ROW_H;
}

static void drawToggle(LGFX &lcd, int x, int y, bool on) {
    uint16_t bgCol  = on ? COL_TEXT_ACCENT : 0x2104;  // cyan or dark grey
    uint16_t knobCol = COL_TEXT_PRIMARY;

    // Track
    lcd.fillRoundRect(x, y, TOGGLE_W, TOGGLE_H, TOGGLE_H / 2, bgCol);

    // Knob
    int knobR = TOGGLE_H / 2 - 3;
    int knobX = on ? (x + TOGGLE_W - TOGGLE_H / 2) : (x + TOGGLE_H / 2);
    int knobY = y + TOGGLE_H / 2;
    lcd.fillCircle(knobX, knobY, knobR, knobCol);
}

static void drawSectionDivider(LGFX &lcd, int y) {
    lcd.drawLine(ROW_PAD, y, DISPLAY_WIDTH - ROW_PAD, y, COL_DIVIDER);
}

static int findCurrentPreset(const UserSettings &s) {
    for (int i = 0; i < PRESET_COUNT; i++) {
        if (PRESETS[i].start == s.sleepStartHour &&
            PRESETS[i].end == s.sleepEndHour)
            return i;
    }
    return 0;  // default to first preset if no match
}

// ═══════════════════════════════════════════════════════
//  Row Renderers
// ═══════════════════════════════════════════════════════

static void drawSleepRow(LGFX &lcd, const UserSettings &s, int y) {
    lcd.fillRect(0, y, DISPLAY_WIDTH, ROW_H, COL_BG);

    lcd.setTextColor(COL_TEXT_PRIMARY);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(ML_DATUM);
    lcd.drawString("Display Sleep", ROW_PAD, y + ROW_H / 2 - 6);

    lcd.setTextColor(COL_TEXT_DIM);
    lcd.setFont(&fonts::Font2);
    lcd.drawString("Auto-off during night", ROW_PAD, y + ROW_H / 2 + 14);

    drawToggle(lcd, TOGGLE_X, y + (ROW_H - TOGGLE_H) / 2, s.sleepEnabled);
    drawSectionDivider(lcd, y + ROW_H - 1);
}

static void drawSleepHoursRow(LGFX &lcd, const UserSettings &s, int y) {
    lcd.fillRect(0, y, DISPLAY_WIDTH, ROW_H, COL_BG);

    if (!s.sleepEnabled) {
        // Greyed out when sleep is disabled
        lcd.setTextColor(0x2104);
        lcd.setFont(&fonts::Font2);
        lcd.setTextDatum(ML_DATUM);
        lcd.drawString("Sleep hours (enable sleep first)", ROW_PAD + 20, y + ROW_H / 2);
    } else {
        lcd.setTextColor(COL_TEXT_SECONDARY);
        lcd.setFont(&fonts::Font2);
        lcd.setTextDatum(ML_DATUM);
        lcd.drawString("Sleep hours:", ROW_PAD + 20, y + ROW_H / 2 - 8);

        // Current hours in accent color
        int idx = findCurrentPreset(s);
        lcd.setTextColor(COL_TEXT_ACCENT);
        lcd.setFont(&fonts::Font4);
        lcd.drawString(PRESETS[idx].label, ROW_PAD + 20, y + ROW_H / 2 + 12);

        // Tap hint
        lcd.setTextColor(COL_TEXT_DIM);
        lcd.setFont(&fonts::Font0);
        lcd.setTextDatum(MR_DATUM);
        lcd.drawString("tap to change", DISPLAY_WIDTH - ROW_PAD, y + ROW_H / 2);
    }
    drawSectionDivider(lcd, y + ROW_H - 1);
}

static void drawPassNotifyRow(LGFX &lcd, const AppState &state, int y) {
    const UserSettings &s = state.settings;
    lcd.fillRect(0, y, DISPLAY_WIDTH, ROW_H, COL_BG);

    lcd.setTextColor(COL_TEXT_PRIMARY);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(ML_DATUM);
    lcd.drawString("ISS Pass Alerts", ROW_PAD, y + ROW_H / 2 - 6);

    // Subtitle: next visible pass (from the predictor) or a hint
    char sub[64];
    passShortText(state, time(nullptr), sub, sizeof(sub));
    lcd.setTextColor(COL_TEXT_DIM);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(sub, ROW_PAD, y + ROW_H / 2 + 14);

    drawToggle(lcd, TOGGLE_X, y + (ROW_H - TOGGLE_H) / 2, s.passNotifyEnabled);
    drawSectionDivider(lcd, y + ROW_H - 1);
}

static void drawHomeRow(LGFX &lcd, const UserSettings &s, int y) {
    lcd.fillRect(0, y, DISPLAY_WIDTH, ROW_H, COL_BG);

    lcd.setTextColor(COL_TEXT_PRIMARY);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(ML_DATUM);
    lcd.drawString("Home Location", ROW_PAD, y + ROW_H / 2 - 6);

    char coords[40];
    snprintf(coords, sizeof(coords), "%.4f, %.4f", s.homeLat, s.homeLon);
    lcd.setTextColor(COL_TEXT_SECONDARY);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(coords, ROW_PAD, y + ROW_H / 2 + 14);

    // Home icon (small house shape)
    int hx = DISPLAY_WIDTH - ROW_PAD - 20, hy = y + ROW_H / 2;
    lcd.fillTriangle(hx, hy - 10, hx - 12, hy, hx + 12, hy, COL_COASTLINE);
    lcd.fillRect(hx - 8, hy, 16, 12, COL_COASTLINE);

    drawSectionDivider(lcd, y + ROW_H - 1);
}

static void drawAboutRow(LGFX &lcd, const AppState &state, int y) {
    lcd.fillRect(0, y, DISPLAY_WIDTH, ROW_H, COL_BG);

    lcd.setTextColor(COL_TEXT_DIM);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(ML_DATUM);
    lcd.drawString("ISS Tracker v1.0", ROW_PAD, y + ROW_H / 2 - 6);

    // Status line
    char status[80];
    snprintf(status, sizeof(status), "NTP: %s   WiFi: %s",
             state.ntpSynced ? "synced" : "pending",
             state.wifiConnected ? "connected" : "offline");
    lcd.setTextColor(COL_TEXT_DIM);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(status, ROW_PAD, y + ROW_H / 2 + 14);

    // Current time if NTP synced
    if (state.ntpSynced) {
        struct tm ti;
        time_t now = time(nullptr);
        localtime_r(&now, &ti);
        char timeBuf[20];
        strftime(timeBuf, sizeof(timeBuf), "%I:%M %p", &ti);
        lcd.setTextColor(COL_TEXT_ACCENT);
        lcd.setFont(&fonts::Font4);
        lcd.setTextDatum(MR_DATUM);
        lcd.drawString(timeBuf, DISPLAY_WIDTH - ROW_PAD, y + ROW_H / 2);
    }
}

// ═══════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════

bool handleSettingsTouch(LGFX &lcd, AppState &state, int16_t x, int16_t y) {
    if (y < CONTENT_Y) return false;

    int row = (y - CONTENT_Y) / ROW_H;
    UserSettings &s = state.settings;

    switch (row) {
        case ROW_SLEEP:
            s.sleepEnabled = !s.sleepEnabled;
            LOG_I("Sleep toggled: %s", s.sleepEnabled ? "ON" : "OFF");
            settingsDirty = true;
            return true;

        case ROW_SLEEP_HOURS:
            if (s.sleepEnabled) {
                int idx = (findCurrentPreset(s) + 1) % PRESET_COUNT;
                s.sleepStartHour = PRESETS[idx].start;
                s.sleepEndHour   = PRESETS[idx].end;
                LOG_I("Sleep hours: %s", PRESETS[idx].label);
                settingsDirty = true;
                return true;
            }
            return false;

        case ROW_PASS_NOTIFY:
            s.passNotifyEnabled = !s.passNotifyEnabled;
            LOG_I("Pass notify toggled: %s", s.passNotifyEnabled ? "ON" : "OFF");
            settingsDirty = true;
            return true;

        default:
            return false;
    }
}

void renderSettingsScreen(LGFX &lcd, const AppState &state, bool fullRedraw) {
    if (!fullRedraw) return;  // Settings screen is static

    // ─── Save if dirty ───────────────────────────────
    if (settingsDirty) {
        settingsSave(state.settings);
        settingsDirty = false;
    }

    // ─── Title ───────────────────────────────────────
    lcd.setTextColor(COL_TEXT_ACCENT);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(TL_DATUM);
    lcd.drawString("SETTINGS", 20, TITLE_Y);
    lcd.drawLine(0, DIVIDER_Y, DISPLAY_WIDTH, DIVIDER_Y, COL_DIVIDER);

    // ─── Rows ────────────────────────────────────────
    drawSleepRow(lcd, state.settings, rowY(ROW_SLEEP));
    drawSleepHoursRow(lcd, state.settings, rowY(ROW_SLEEP_HOURS));
    drawPassNotifyRow(lcd, state, rowY(ROW_PASS_NOTIFY));
    drawHomeRow(lcd, state.settings, rowY(ROW_HOME));
    drawAboutRow(lcd, state, rowY(ROW_ABOUT));

    lcd.setFont(&fonts::Font0);
    lcd.setTextDatum(TL_DATUM);
}
