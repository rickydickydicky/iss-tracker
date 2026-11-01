#include "screen_manager.h"
#include "../config.h"
#include "../colors.h"
#include "../display_config.h"
#include "map_screen.h"
#include "stats_screen.h"
#include "crew_screen.h"
#include "settings_screen.h"
#include "../pass_text.h"
#include <Wire.h>
#include <string.h>
#include <time.h>

// ─── Global LGFX Display Instance ───────────────────────
static LGFX lcd;

// ─── Tab Bar Constants ───────────────────────────────────
static const int TAB_WIDTH = DISPLAY_WIDTH / SCREEN_COUNT;
static const int TAB_Y = DISPLAY_HEIGHT - TAB_BAR_HEIGHT;
static const int TAB_LINE_HEIGHT = 3;

static const char* TAB_LABELS[] = { "MAP", "STATS", "CREW", "SETUP" };

// ─── Touch debounce state ───────────────────────────────
static unsigned long lastTouchTime = 0;
static const unsigned long TOUCH_DEBOUNCE = 150; // ms — fast enough for responsive taps

// ─── Render state ──────────────────────────────────────
static Screen lastRenderedScreen = SCREEN_MAP;
static bool needsFullRedraw = true;
static unsigned long lastRenderTime = 0;
static const unsigned long RENDER_INTERVAL = 200; // ms — 5 FPS is fine for tracking data

// ─── Data-change tracking ──────────────────────────────
// When data validity transitions (e.g. crew loads for first time),
// we need to force a full redraw to clear stale "Loading..." messages.
static bool prevPositionValid = false;
static bool prevCrewValid = false;

// ─── Helper to check data staleness ─────────────────────
static bool isDataStale(const AppState &state) {
    unsigned long now = millis();
    // Data is stale if last update was > 30 seconds ago
    return (now - state.lastPositionUpdate > 30000) || state.apiError;
}

// ─── Pass alert banner ──────────────────────────────────
// Overlays the top of whichever screen is active while a visible pass is
// imminent/underway. Stops short of the status dots in the top-right, and
// above the ISS marker's reach on the map (ISS latitude <= 51.6 deg).
static const int BANNER_W = DISPLAY_WIDTH - 50;
static const int BANNER_H = 34;
static bool bannerShown = false;
static char lastBanner[96] = "";

static void renderPassBanner(const AppState &state, bool fullRedraw) {
    char text[96];
    bool active = passAlertText(state, time(nullptr), text, sizeof(text));

    if (!active) {
        if (bannerShown) {
            // Banner gone: repaint the screen underneath it
            bannerShown = false;
            lastBanner[0] = '\0';
            needsFullRedraw = true;
        }
        return;
    }

    if (fullRedraw || !bannerShown || strcmp(text, lastBanner) != 0) {
        lcd.fillRect(0, 0, BANNER_W, BANNER_H, COL_ALERT_BG);
        strncpy(lastBanner, text, sizeof(lastBanner) - 1);
        lastBanner[sizeof(lastBanner) - 1] = '\0';
        if (!bannerShown) LOG_I("Pass alert: %s", text);
        bannerShown = true;
    }

    // Repaint the text every frame (with background) so screens that redraw
    // underneath can't leave it damaged. Same pixels each time: no flicker.
    lcd.setFont(&fonts::Font4);
    if (lcd.textWidth(text) > BANNER_W - 16) lcd.setFont(&fonts::Font2);
    lcd.setTextColor(COL_ALERT_TEXT, COL_ALERT_BG);
    lcd.setTextDatum(ML_DATUM);
    lcd.drawString(text, 8, BANNER_H / 2);
    lcd.setTextColor(COL_TEXT_PRIMARY);
}

// ─── Tab bar renderer (only on full redraw) ─────────────
static void renderTabBar(const AppState &state) {
    lcd.fillRect(0, TAB_Y, DISPLAY_WIDTH, TAB_BAR_HEIGHT, COL_TAB_BG);
    lcd.drawLine(0, TAB_Y, DISPLAY_WIDTH - 1, TAB_Y, COL_TAB_DIVIDER);

    for (int i = 0; i < SCREEN_COUNT; i++) {
        int tab_x = i * TAB_WIDTH;
        bool active = (state.activeScreen == (Screen)i);

        if (active) {
            lcd.fillRect(tab_x, TAB_Y, TAB_WIDTH, TAB_LINE_HEIGHT, COL_TAB_ACTIVE_LINE);
        }

        int label_x = tab_x + TAB_WIDTH / 2;
        int label_y = TAB_Y + TAB_LINE_HEIGHT + 15;

        lcd.setTextColor(active ? COL_TAB_ACTIVE_TEXT : COL_TAB_INACTIVE);
        lcd.setTextDatum(CC_DATUM);
        lcd.setFont(&fonts::Font2);
        lcd.drawString(TAB_LABELS[i], label_x, label_y);

        if (i < SCREEN_COUNT - 1) {
            int divider_x = (i + 1) * TAB_WIDTH;
            lcd.drawLine(divider_x, TAB_Y, divider_x, DISPLAY_HEIGHT - 1, COL_TAB_DIVIDER);
        }
    }
}

void screenManagerInit() {
    lcd.init();
    lcd.setRotation(0);  // Landscape: 800x480
    lcd.fillScreen(COL_BG);
    needsFullRedraw = true;
    LOG_I("LovyanGFX display initialized: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

// ─── ISS Line Art (gold silhouette) ─────────────────────
// Draws a recognizable ISS outline: truss, solar panels
// with cell grids, central modules, and radiators.
static void drawISSArt(int cx, int cy) {
    uint16_t col = COL_COASTLINE;  // Gold

    // ── Main Truss (2px thick) ──────────────────────
    lcd.fillRect(cx - 275, cy - 1, 550, 3, col);

    // ── Solar Panels (4 pairs) ──────────────────────
    // Each panel: 52px wide, 68px tall
    const int pw = 52, ph = 68;
    const int gap = 5;  // gap between truss and panel edge

    // X offsets from center for left edge of each panel pair
    //   Port outer, Port inner, Starboard inner, Starboard outer
    const int px[] = { -275, -148, 96, 223 };

    for (int i = 0; i < 4; i++) {
        int x = cx + px[i];
        int yt = cy - gap - ph;   // top panel top edge
        int yb = cy + gap;        // bottom panel top edge

        // Panel outlines
        lcd.drawRect(x, yt, pw, ph, col);
        lcd.drawRect(x, yb, pw, ph, col);

        // Cell grid: 5 horizontal divisions
        for (int g = 1; g < 5; g++) {
            int dy = g * ph / 5;
            lcd.drawFastHLine(x, yt + dy, pw, col);
            lcd.drawFastHLine(x, yb + dy, pw, col);
        }
        // Cell grid: vertical center line
        lcd.drawFastVLine(x + pw / 2, yt, ph, col);
        lcd.drawFastVLine(x + pw / 2, yb, ph, col);
    }

    // ── Central Module Cluster ──────────────────────
    lcd.drawRect(cx - 48, cy - 13, 96, 26, col);    // Main habitation
    lcd.drawRect(cx + 48, cy - 8,  28, 16, col);    // Forward lab
    lcd.drawRect(cx - 76, cy - 8,  28, 16, col);    // Russian segment
    lcd.drawRect(cx - 5,  cy - 24, 10, 11, col);    // Node stub (top)
    lcd.drawRect(cx - 5,  cy + 13, 10, 11, col);    // Node stub (bottom)

    // ── Radiator Panels (between inner panels and center) ─
    lcd.drawRect(cx - 92, cy - 20, 18, 40, col);
    lcd.drawRect(cx + 74, cy - 20, 18, 40, col);
    // Radiator cross-hatch (2 lines each)
    lcd.drawLine(cx - 92, cy - 20, cx - 74, cy + 20, col);
    lcd.drawLine(cx - 92, cy + 20, cx - 74, cy - 20, col);
    lcd.drawLine(cx + 74, cy - 20, cx + 92, cy + 20, col);
    lcd.drawLine(cx + 74, cy + 20, cx + 92, cy - 20, col);
}

void screenShowLanding() {
    lcd.fillScreen(COL_BG);

    // ISS line art — upper portion of screen
    drawISSArt(DISPLAY_WIDTH / 2, 170);

    // Title: "I S S   T R A C K E R" with wide letter spacing
    lcd.setTextColor(COL_TEXT_ACCENT);   // Cyan
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(CC_DATUM);
    lcd.drawString("I S S   T R A C K E R", DISPLAY_WIDTH / 2, 310);

    // Gold accent line under title
    int lineW = 180;
    lcd.fillRect(DISPLAY_WIDTH / 2 - lineW / 2, 332, lineW, 2, COL_COASTLINE);

    // Boot hint
    lcd.setTextColor(COL_TEXT_DIM);
    lcd.setFont(&fonts::Font2);
    lcd.drawString("Touch screen to configure WiFi", DISPLAY_WIDTH / 2, 450);

    lcd.setTextDatum(TL_DATUM);
    lcd.setFont(&fonts::Font0);
}

void screenShowStatus(const char* line1, const char* line2) {
    // Clear lower portion of screen (below ISS art area)
    lcd.fillRect(0, 360, DISPLAY_WIDTH, 120, COL_BG);

    lcd.setTextDatum(CC_DATUM);

    // Line 1: main status (cyan)
    lcd.setTextColor(COL_TEXT_ACCENT);
    lcd.setFont(&fonts::Font4);
    lcd.drawString(line1, DISPLAY_WIDTH / 2, 390);

    // Line 2: secondary info (dim)
    if (line2) {
        lcd.setTextColor(COL_TEXT_DIM);
        lcd.setFont(&fonts::Font2);
        lcd.drawString(line2, DISPLAY_WIDTH / 2, 430);
    }

    lcd.setTextDatum(TL_DATUM);
    lcd.setFont(&fonts::Font0);
}

void screenShowSplash(const char* message) {
    lcd.fillScreen(COL_BG);
    lcd.setTextColor(COL_TEXT_ACCENT);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(CC_DATUM);
    lcd.drawString(message, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2);
    lcd.setFont(&fonts::Font0);
    lcd.setTextDatum(TL_DATUM);
}

bool screenCheckTouch() {
    lgfx::touch_point_t tp;
    return lcd.getTouch(&tp, 1) > 0;
}

bool screenManagerHandleTouch(AppState &state) {
    unsigned long now = millis();

    // Debounce
    if (now - lastTouchTime < TOUCH_DEBOUNCE) {
        return false;
    }

    // Read touch via LovyanGFX
    lgfx::touch_point_t tp;
    int touchCount = lcd.getTouch(&tp, 1);
    if (touchCount == 0) {
        return false;  // No touch detected
    }

    int16_t touch_x = tp.x;
    int16_t touch_y = tp.y;

    // Check if touch is in tab bar area — allow 20px above for easier tapping
    if (touch_y > (TAB_Y - 20)) {
        // Determine which tab was tapped
        int tab = touch_x / TAB_WIDTH;
        if (tab >= 0 && tab < SCREEN_COUNT) {
            if (state.activeScreen != (Screen)tab) {
                state.activeScreen = (Screen)tab;
                needsFullRedraw = true;
            }
            lastTouchTime = now;
            LOG_I("Touch tab %d (x=%d, y=%d)", tab, touch_x, touch_y);
        }
    } else if (state.activeScreen == SCREEN_CREW) {
        // Delegate touch to crew screen (modal open/close)
        if (handleCrewTouch(lcd, state, touch_x, touch_y)) {
            needsFullRedraw = true;
            lastTouchTime = now;
        }
    } else if (state.activeScreen == SCREEN_SETTINGS) {
        if (handleSettingsTouch(lcd, state, touch_x, touch_y)) {
            needsFullRedraw = true;
            lastTouchTime = now;
        }
    }

    return true;  // A touch was detected (even if not on a target)
}

void screenManagerRender(const AppState &state) {
    unsigned long now = millis();

    // ─── Detect data-validity transitions ────────────────
    // If position or crew data becomes valid for the first time,
    // force a full redraw to clear "Loading..." / "Waiting..." text.
    if (state.position.valid && !prevPositionValid) {
        needsFullRedraw = true;
        LOG_I("Position data now valid — forcing full redraw");
    }
    if (state.crew.valid && !prevCrewValid) {
        needsFullRedraw = true;
        LOG_I("Crew data now valid — forcing full redraw");
    }
    prevPositionValid = state.position.valid;
    prevCrewValid = state.crew.valid;

    // Throttle rendering — no need to redraw faster than 5 FPS
    if (!needsFullRedraw && (now - lastRenderTime < RENDER_INTERVAL)) {
        return;
    }
    lastRenderTime = now;

    // Capture and reset the full-redraw flag so each screen knows
    bool doFullRedraw = needsFullRedraw;

    // Full clear only when switching tabs or data state changed
    if (doFullRedraw) {
        lcd.fillRect(0, 0, DISPLAY_WIDTH, MAP_AREA_HEIGHT, COL_BG);
        needsFullRedraw = false;
        lastRenderedScreen = state.activeScreen;
    }

    // Render the active screen — pass fullRedraw so it can skip static elements
    switch (state.activeScreen) {
        case SCREEN_MAP:
            renderMapScreen(lcd, state, doFullRedraw);
            break;
        case SCREEN_STATS:
            renderStatsScreen(lcd, state, doFullRedraw);
            break;
        case SCREEN_CREW:
            renderCrewScreen(lcd, state, doFullRedraw);
            break;
        case SCREEN_SETTINGS:
            renderSettingsScreen(lcd, state, doFullRedraw);
            break;
        default:
            break;
    }

    renderPassBanner(state, doFullRedraw);

    // ─── Tab Bar — only redraw on full redraw ────────────
    // The tab bar only changes when switching tabs, which sets fullRedraw.
    if (doFullRedraw) {
        renderTabBar(state);
    }

    // ─── Status Indicators ──────────────────────────────
    // Small dots in upper-right — draw with background to avoid ghosting
    int status_x = DISPLAY_WIDTH - 20;
    int status_y = 15;
    uint16_t wifi_color = state.wifiConnected ? COL_OK : COL_STALE;
    lcd.fillCircle(status_x, status_y, 5, wifi_color);

    if (isDataStale(state)) {
        lcd.fillCircle(status_x - 20, status_y, 5, COL_STALE);
    } else {
        // Clear stale indicator if data is fresh (draw over with screen bg)
        lcd.fillCircle(status_x - 20, status_y, 5, COL_BG);
    }

    lcd.setTextDatum(TL_DATUM);
    lcd.setFont(&fonts::Font0);
}

void screenSetBacklight(bool on) {
    lcd.setBrightness(on ? 255 : 0);
}

void screenForceRedraw() {
    needsFullRedraw = true;
}

void screenResetTouch() {
    // Full GT911 hardware reset via CH422G I/O expander.
    // Same sequence as LGFX::init_impl() — required after
    // prolonged sleep because the GT911 may enter an internal
    // low-power state that dummy reads can't recover from.
    //
    // CH422G pins: 1=TP_RST, 2=LCD_BL, 3=LCD_RST
    // GT911 I2C address selection: INT (GPIO 4) LOW during reset → 0x5D

    uint8_t state_bl_lcd = (1 << 2) | (1 << 3);              // BL + LCD_RST on
    uint8_t state_all    = (1 << 1) | (1 << 2) | (1 << 3);   // TP_RST + BL + LCD_RST on

    // 1. INT pin LOW so GT911 selects address 0x5D on boot
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);

    // 2. Assert TP_RST = 0 (pull pin 1 low, keep BL + LCD_RST high)
    Wire.beginTransmission(0x24);
    Wire.write(0x01);
    Wire.endTransmission();
    Wire.beginTransmission(0x38);
    Wire.write(state_bl_lcd);
    Wire.endTransmission();

    delay(50);  // GT911 datasheet: min 10ms reset pulse

    // 3. Release TP_RST = 1
    Wire.beginTransmission(0x24);
    Wire.write(0x01);
    Wire.endTransmission();
    Wire.beginTransmission(0x38);
    Wire.write(state_all);
    Wire.endTransmission();

    delay(50);  // GT911 needs ~50ms to boot after reset

    // 4. INT back to input for touch interrupts
    pinMode(4, INPUT);

    delay(100);  // Extra settle time for I2C bus and GT911 firmware init

    LOG_I("GT911 touch controller reset complete");
}
