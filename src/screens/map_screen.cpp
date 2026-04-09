#include "map_screen.h"
#include "../config.h"
#include "../colors.h"
#include "../display_config.h"
#include "../map_data.h"
#include <time.h>

// ─── Map Projection Helpers ─────────────────────────────
// Equirectangular: lon -180..180 → x 0..799, lat 90..-90 → y 0..429

static int lonToX(float lon) {
    return (int)(((lon + 180.0f) / 360.0f) * DISPLAY_WIDTH);
}

static int latToY(float lat) {
    return (int)(((90.0f - lat) / 180.0f) * MAP_AREA_HEIGHT);
}

// ─── Previous-frame state for incremental erase ─────────
static int prevISSx = -1, prevISSy = -1;

// ─── Grid Drawing ───────────────────────────────────────
static void drawGrid(LGFX &lcd) {
    // Vertical lines every 30° longitude
    for (int lon = -150; lon <= 180; lon += 30) {
        int x = lonToX(lon);
        // Dashed line
        for (int y = 0; y < MAP_AREA_HEIGHT; y += 4) {
            lcd.drawPixel(x, y, COL_GRID);
        }
    }

    // Horizontal lines every 30° latitude
    for (int lat = -60; lat <= 60; lat += 30) {
        int y = latToY(lat);
        for (int x = 0; x < DISPLAY_WIDTH; x += 4) {
            lcd.drawPixel(x, y, COL_GRID);
        }
    }

    // Equator — slightly brighter, tighter dash
    int eq_y = latToY(0);
    for (int x = 0; x < DISPLAY_WIDTH; x += 3) {
        lcd.drawPixel(x, eq_y, COL_EQUATOR);
        if (x + 1 < DISPLAY_WIDTH) lcd.drawPixel(x + 1, eq_y, COL_EQUATOR);
    }
}

// ─── Continent Outline Rendering ────────────────────────
// Reads PROGMEM data and draws polylines with MAP_BREAK gaps
static void drawCoastlines(LGFX &lcd) {
    for (int c = 0; c < NUM_CONTINENTS; c++) {
        const int16_t* pts = continents[c].points;
        uint16_t total = continents[c].totalValues;

        bool penDown = false;
        int prevX = 0, prevY = 0;

        for (uint16_t i = 0; i < total; i += 2) {
            int16_t lat10 = pgm_read_word(&pts[i]);
            int16_t lon10 = pgm_read_word(&pts[i + 1]);

            // Check for pen-up sentinel
            if (lat10 == (int16_t)MAP_BREAK) {
                penDown = false;
                continue;
            }

            // Convert to screen coords
            float lat = lat10 / 10.0f;
            float lon = lon10 / 10.0f;
            int sx = lonToX(lon);
            int sy = latToY(lat);

            // Clamp to map area
            if (sx < 0) sx = 0;
            if (sx >= DISPLAY_WIDTH) sx = DISPLAY_WIDTH - 1;
            if (sy < 0) sy = 0;
            if (sy >= MAP_AREA_HEIGHT) sy = MAP_AREA_HEIGHT - 1;

            if (penDown) {
                // Skip lines that wrap around the date line (>half screen width)
                int dx = abs(sx - prevX);
                if (dx < DISPLAY_WIDTH / 2) {
                    // Draw 2px thick lines — original + 1px offset in y
                    lcd.drawLine(prevX, prevY,     sx, sy,     COL_COASTLINE);
                    lcd.drawLine(prevX, prevY + 1, sx, sy + 1, COL_COASTLINE);
                }
            }

            prevX = sx;
            prevY = sy;
            penDown = true;
        }
    }
}

// ─── Erase ISS and its glow ring from previous frame ────
static void eraseISS(LGFX &lcd) {
    if (prevISSx < 0) return;
    // The ISS drawing covers a ~30x30 pixel area (14px glow radius + crosshair 10px arms)
    int r = 16;
    int ex = prevISSx - r;
    int ey = prevISSy - r;
    int ew = r * 2 + 1;
    int eh = r * 2 + 1;
    // Clamp to map area
    if (ex < 0) { ew += ex; ex = 0; }
    if (ey < 0) { eh += ey; ey = 0; }
    if (ex + ew > DISPLAY_WIDTH) ew = DISPLAY_WIDTH - ex;
    if (ey + eh > MAP_AREA_HEIGHT) eh = MAP_AREA_HEIGHT - ey;
    if (ew > 0 && eh > 0) {
        lcd.fillRect(ex, ey, ew, eh, COL_OCEAN);
    }
}

// ─── Ground Track Drawing ───────────────────────────────
static void drawGroundTrack(LGFX &lcd, const AppState &state) {
    if (state.track.count < 2) return;

    for (int i = 0; i < state.track.count; i++) {
        float tLat, tLon;
        state.track.get(i, tLat, tLon);

        int tx = lonToX(tLon);
        int ty = latToY(tLat);

        // Clamp to map bounds
        if (tx < 0 || tx >= DISPLAY_WIDTH || ty < 0 || ty >= MAP_AREA_HEIGHT) continue;

        // Fade from old (dim) to new (bright)
        float progress = (float)i / (float)state.track.count;
        uint16_t color;
        int radius;

        if (progress < 0.33f) {
            color = COL_TRACK_OLD;
            radius = 1;
        } else if (progress < 0.66f) {
            color = COL_TRACK_MID;
            radius = 1;
        } else {
            color = COL_TRACK_NEW;
            radius = 2;
        }

        lcd.fillCircle(tx, ty, radius, color);
    }
}

// ─── ISS Position Drawing ───────────────────────────────
static void drawISSPosition(LGFX &lcd, const AppState &state) {
    if (!state.position.valid) return;

    int ix = lonToX(state.position.longitude);
    int iy = latToY(state.position.latitude);

    if (ix < 0 || ix >= DISPLAY_WIDTH || iy < 0 || iy >= MAP_AREA_HEIGHT) return;

    // Outer glow ring
    lcd.drawCircle(ix, iy, 14, COL_ISS_GLOW);
    lcd.drawCircle(ix, iy, 13, COL_ISS_GLOW);

    // Pulsing indicator — alternate dot/crosshair every 500ms
    if ((millis() / 500) % 2 == 0) {
        lcd.fillCircle(ix, iy, 6, COL_ISS_PRIMARY);
    } else {
        lcd.drawLine(ix - 10, iy, ix + 10, iy, COL_ISS_PRIMARY);
        lcd.drawLine(ix, iy - 10, ix, iy + 10, COL_ISS_PRIMARY);
        lcd.fillCircle(ix, iy, 3, COL_ISS_PRIMARY);
    }

    // Remember position for next-frame erase
    prevISSx = ix;
    prevISSy = iy;
}

// ─── Home Marker ────────────────────────────────────────
static void drawHomeMarker(LGFX &lcd) {
    int hx = lonToX(HOME_LON);
    int hy = latToY(HOME_LAT);

    if (hx < 0 || hx >= DISPLAY_WIDTH || hy < 0 || hy >= MAP_AREA_HEIGHT) return;

    // Small yellow diamond
    lcd.drawLine(hx - 5, hy, hx, hy - 5, COL_HOME);
    lcd.drawLine(hx, hy - 5, hx + 5, hy, COL_HOME);
    lcd.drawLine(hx + 5, hy, hx, hy + 5, COL_HOME);
    lcd.drawLine(hx, hy + 5, hx - 5, hy, COL_HOME);

    // Label
    lcd.setTextColor(COL_HOME);
    lcd.setFont(&fonts::Font0);
    lcd.setTextDatum(BC_DATUM);
    lcd.drawString("HOME", hx, hy - 8);
}

// ─── Coordinate Overlay ─────────────────────────────────
static void drawCoordinateOverlay(LGFX &lcd, const AppState &state) {
    if (!state.position.valid) return;

    int ox = 10;
    int oy = MAP_AREA_HEIGHT - 80;
    int ow = 195;
    int oh = 70;

    // Dark semi-opaque background
    lcd.fillRect(ox, oy, ow, oh, 0x0841);  // very dark grey — visible against black bg
    lcd.drawRect(ox, oy, ow, oh, COL_GRID);

    lcd.setTextColor(COL_TEXT_PRIMARY);
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(TL_DATUM);

    char buf[64];
    snprintf(buf, sizeof(buf), "LAT  %7.2f%c",
             fabsf(state.position.latitude),
             state.position.latitude >= 0 ? 'N' : 'S');
    lcd.drawString(buf, ox + 8, oy + 6);

    snprintf(buf, sizeof(buf), "LON %7.2f%c",
             fabsf(state.position.longitude),
             state.position.longitude >= 0 ? 'E' : 'W');
    lcd.drawString(buf, ox + 8, oy + 26);

    snprintf(buf, sizeof(buf), "ALT  %.0f km  %s",
             state.position.altitude,
             state.position.visibility.c_str());
    lcd.drawString(buf, ox + 8, oy + 46);

}

// ─── Santa Sleigh Easter Egg (Dec 25) ───────────────────
// A tiny sleigh with reindeer crosses the map once on Christmas Day.
// Uses millis() animation — zero cost when inactive.
static bool santaChecked  = false;   // Have we checked the date this boot?
static bool santaActive   = false;   // Animation in progress?
static unsigned long santaStart = 0; // millis() when animation began
static const unsigned long SANTA_DURATION = 10000;  // 10 seconds to cross
static const int SANTA_Y = 65;       // Fixed altitude on screen (upper third)

// Previous frame position for erase
static int prevSantaX = -1;

static void drawSantaSprite(LGFX &lcd, int x, int y) {
    // ── Reindeer (two, leading the sleigh) ──
    // Each reindeer: small body + legs + antlers
    // Lead reindeer
    lcd.fillRect(x - 28, y - 1, 8, 4, 0xCA40);   // Brown body
    lcd.drawPixel(x - 29, y - 3, 0xCA40);          // Antler L
    lcd.drawPixel(x - 30, y - 4, 0xCA40);
    lcd.drawPixel(x - 27, y - 3, 0xCA40);          // Antler R
    lcd.drawPixel(x - 26, y - 4, 0xCA40);
    lcd.drawPixel(x - 21, y - 1, 0xF800);          // Red nose
    lcd.drawFastVLine(x - 28, y + 3, 3, 0xCA40);   // Legs
    lcd.drawFastVLine(x - 23, y + 3, 3, 0xCA40);

    // Rear reindeer
    lcd.fillRect(x - 18, y - 1, 8, 4, 0xCA40);
    lcd.drawPixel(x - 19, y - 3, 0xCA40);
    lcd.drawPixel(x - 20, y - 4, 0xCA40);
    lcd.drawPixel(x - 17, y - 3, 0xCA40);
    lcd.drawPixel(x - 16, y - 4, 0xCA40);
    lcd.drawFastVLine(x - 18, y + 3, 3, 0xCA40);
    lcd.drawFastVLine(x - 13, y + 3, 3, 0xCA40);

    // Reins (thin line from reindeer to sleigh)
    lcd.drawLine(x - 11, y, x - 4, y + 1, 0x8410);

    // ── Sleigh ──
    lcd.fillRect(x - 4, y - 2, 14, 7, 0xF800);     // Red sleigh body
    lcd.fillRect(x - 5, y + 5, 16, 2, 0xC000);      // Dark red runner
    lcd.drawPixel(x - 6, y + 4, 0xC000);             // Runner curl front
    lcd.drawPixel(x + 11, y + 4, 0xC000);            // Runner curl back

    // ── Santa ──
    lcd.fillRect(x, y - 5, 5, 3, 0xF800);            // Red hat
    lcd.fillRect(x, y - 2, 5, 2, 0xFEA0);            // Face (skin tone)
    lcd.fillRect(x - 1, y - 6, 7, 1, 0xFFFF);        // Hat brim (white fur)
    lcd.fillCircle(x + 2, y - 7, 1, 0xFFFF);          // Pompom

    // ── Sack of presents ──
    lcd.fillRect(x + 6, y - 4, 4, 5, 0x07E0);        // Green sack
    lcd.drawPixel(x + 7, y - 5, 0xF800);              // Red ribbon
}

static void eraseSantaRegion(LGFX &lcd, int x, int y) {
    // Erase a region covering the full sprite extent
    int ex = x - 32;
    int ey = y - 9;
    int ew = 48;
    int eh = 18;
    if (ex < 0) { ew += ex; ex = 0; }
    if (ey < 0) { eh += ey; ey = 0; }
    if (ex + ew > DISPLAY_WIDTH) ew = DISPLAY_WIDTH - ex;
    if (ey + eh > MAP_AREA_HEIGHT) eh = MAP_AREA_HEIGHT - ey;
    if (ew > 0 && eh > 0) {
        lcd.fillRect(ex, ey, ew, eh, COL_OCEAN);
    }
}

static void updateSanta(LGFX &lcd) {
    // One-time date check per boot
    if (!santaChecked) {
        struct tm ti;
        time_t now = time(nullptr);
        localtime_r(&now, &ti);
        if (ti.tm_year > 100) {  // NTP has synced (year > 2000)
            santaChecked = true;
            if (ti.tm_mon == 11 && ti.tm_mday == 25) {  // December 25
                santaActive = true;
                santaStart = millis();
                LOG_I("Ho ho ho! Santa sleigh activated");
            }
        }
        return;
    }

    if (!santaActive) return;

    unsigned long elapsed = millis() - santaStart;
    if (elapsed > SANTA_DURATION) {
        // Animation finished — erase final position and deactivate
        if (prevSantaX >= 0) {
            eraseSantaRegion(lcd, prevSantaX, SANTA_Y);
            prevSantaX = -1;
        }
        santaActive = false;
        return;
    }

    // Calculate X position: right edge → left edge
    float progress = (float)elapsed / (float)SANTA_DURATION;
    int sx = DISPLAY_WIDTH + 30 - (int)((DISPLAY_WIDTH + 60) * progress);

    // Erase previous position
    if (prevSantaX >= 0 && prevSantaX != sx) {
        eraseSantaRegion(lcd, prevSantaX, SANTA_Y);
    }

    // Draw at new position
    drawSantaSprite(lcd, sx, SANTA_Y);
    prevSantaX = sx;
}

// ─── Main Map Renderer ──────────────────────────────────
void renderMapScreen(LGFX &lcd, const AppState &state, bool fullRedraw) {

    if (fullRedraw) {
        // ── Full redraw: ocean, grid, coastlines, home ──
        lcd.fillRect(0, 0, DISPLAY_WIDTH, MAP_AREA_HEIGHT, COL_OCEAN);
        drawGrid(lcd);
        drawCoastlines(lcd);
        drawHomeMarker(lcd);
        // Reset previous positions so we don't erase stale areas
        prevISSx = -1;
        prevISSy = -1;
        prevSantaX = -1;
    } else {
        // ── Incremental: erase only the ISS dot ──
        // The overlay doesn't need erasing — its own fillRect background
        // overwrites old text, and its position is fixed on screen.
        eraseISS(lcd);
    }

    // Dynamic elements — drawn every frame
    drawGroundTrack(lcd, state);
    drawISSPosition(lcd, state);
    drawCoordinateOverlay(lcd, state);

    // Easter egg — Santa crosses the map on Dec 25
    if (state.ntpSynced) {
        updateSanta(lcd);
    }

    // "Waiting for data" message
    if (!state.position.valid) {
        lcd.setTextColor(COL_TEXT_DIM);
        lcd.setFont(&fonts::Font4);
        lcd.setTextDatum(CC_DATUM);
        lcd.drawString("Waiting for ISS data...", DISPLAY_WIDTH / 2, MAP_AREA_HEIGHT / 2);
        lcd.setFont(&fonts::Font0);
        lcd.setTextDatum(TL_DATUM);
    }
}
