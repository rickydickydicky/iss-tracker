#include "crew_screen.h"
#include "../config.h"
#include "../colors.h"

// ─── Layout Constants ───────────────────────────────────
static const int TITLE_Y      = 20;
static const int DIVIDER_Y    = 50;
static const int CONTENT_Y    = DIVIDER_Y + 10;
static const int GRID_PAD     = 4;     // padding between cells

// ─── Modal Constants ────────────────────────────────────
static const int MODAL_X      = 80;
static const int MODAL_Y      = 35;
static const int MODAL_W      = 640;
static const int MODAL_H      = 360;
static const int MODAL_PAD    = 20;
static const int MODAL_AV     = 80;   // large avatar size

// ─── Grid state (computed by computeGridLayout) ─────────
static int gridCols      = 5;
static int gridRows      = 2;
static int cellW         = 160;
static int cellH         = 178;
static int gridAvatarSize = 64;

// ─── Render state ───────────────────────────────────────
static bool modalOpen      = false;
static int  modalCrewIndex = 0;
static unsigned long lastRenderedCrewUpdate = 0;

// ═══════════════════════════════════════════════════════
//  Grid Layout Computation
// ═══════════════════════════════════════════════════════

static void computeGridLayout(int count) {
    if (count <= 0) { gridCols = 1; gridRows = 1; return; }

    // Pick rows to keep cells large — prefer 2 rows for typical crews
    if      (count <= 5)  gridRows = 1;
    else if (count <= 10) gridRows = 2;
    else                  gridRows = 3;

    gridCols = (count + gridRows - 1) / gridRows;

    int availW = DISPLAY_WIDTH;
    int availH = MAP_AREA_HEIGHT - CONTENT_Y;

    cellW = (availW - (gridCols - 1) * GRID_PAD) / gridCols;
    cellH = (availH - (gridRows - 1) * GRID_PAD) / gridRows;

    // Avatar size scales with cell height
    if      (cellH >= 160) gridAvatarSize = 72;
    else if (cellH >= 120) gridAvatarSize = 64;
    else                   gridAvatarSize = 48;
}

// ═══════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════

// ─── Placeholder avatar: coloured circle with initials ──
static void drawPlaceholder(LGFX &lcd, int cx, int cy, int r,
                            const String &name, uint16_t col) {
    lcd.fillCircle(cx, cy, r, col);
    char ini[3] = {0};
    if (name.length() > 0) ini[0] = toupper(name.charAt(0));
    int sp = name.lastIndexOf(' ');
    if (sp > 0 && sp < (int)name.length() - 1)
        ini[1] = toupper(name.charAt(sp + 1));

    lcd.setTextColor(COL_TEXT_PRIMARY);
    lcd.setFont((r >= 28) ? &fonts::Font4 : &fonts::Font2);
    lcd.setTextDatum(MC_DATUM);
    lcd.drawString(ini, cx, cy + 1);
}

// ─── Draw avatar (JPEG if available, else placeholder) ──
static void drawAvatar(LGFX &lcd, const CrewMember &m,
                       int x, int y, int size) {
    uint16_t borderCol = (m.craft == "ISS") ? COL_CREW_ISS : COL_CREW_OTHER;

    if (m.imageLoaded && m.imageData && m.imageSize > 2 &&
        m.imageData[0] == 0xFF && m.imageData[1] == 0xD8) {

        // TJpgDec scales: use 1/4 for ≤32px, 1/2 for larger
        float sc = (size <= 32) ? 0.25f : 0.5f;

        if (!lcd.drawJpg(m.imageData, m.imageSize,
                         x, y, size, size, 0, 0, sc, sc)) {
            drawPlaceholder(lcd, x + size/2, y + size/2,
                            size/2 - 1, m.name, borderCol);
        }
        lcd.drawRect(x, y, size, size, borderCol);
    } else {
        drawPlaceholder(lcd, x + size/2, y + size/2,
                        size/2 - 1, m.name, borderCol);
    }
}

// ─── Truncate name to fit pixel width ───────────────────
static String fitName(LGFX &lcd, const String &name, int maxW) {
    if (lcd.textWidth(name.c_str()) <= maxW) return name;

    // Try: "First L." (first name + last initial)
    int sp = name.lastIndexOf(' ');
    if (sp > 0) {
        String first = name.substring(0, sp);
        String abbr = first + " " + name.charAt(sp + 1) + ".";
        if (lcd.textWidth(abbr.c_str()) <= maxW) return abbr;
    }

    // Fall back to simple truncation
    String trunc = name;
    while (trunc.length() > 4 && lcd.textWidth((trunc + ".").c_str()) > maxW) {
        trunc = trunc.substring(0, trunc.length() - 1);
    }
    return trunc + ".";
}

// ─── Draw one crew card in the grid ─────────────────────
static void drawCrewCard(LGFX &lcd, const CrewMember &m,
                         int x, int y, int w, int h) {
    uint16_t accentCol = (m.craft == "ISS") ? COL_CREW_ISS : COL_CREW_OTHER;

    // Card background — very subtle raised card
    lcd.fillRect(x, y, w, h, COL_BG);
    lcd.drawRect(x + 1, y + 1, w - 2, h - 2, 0x0861);

    // Coloured accent line at top of card
    lcd.fillRect(x + 4, y + 2, w - 8, 2, accentCol);

    // ── Avatar (centred horizontally) ────────────────
    int avX = x + (w - gridAvatarSize) / 2;
    int avY = y + 10;
    drawAvatar(lcd, m, avX, avY, gridAvatarSize);

    // ── Name (centred, up to 2 lines) ────────────────
    int nameY = avY + gridAvatarSize + 8;
    int maxTextW = w - 12;

    lcd.setTextColor(COL_TEXT_PRIMARY);
    lcd.setTextDatum(TC_DATUM);

    // Try full name on one line first
    int sp = m.name.lastIndexOf(' ');
    if (sp > 0 && lcd.textWidth(m.name.c_str()) > maxTextW) {
        // Split into first name / last name on two lines
        String firstName = m.name.substring(0, sp);
        String lastName  = m.name.substring(sp + 1);

        lcd.setFont(&fonts::Font2);
        lcd.drawString(fitName(lcd, firstName, maxTextW).c_str(),
                       x + w / 2, nameY);
        lcd.drawString(fitName(lcd, lastName, maxTextW).c_str(),
                       x + w / 2, nameY + 18);
        nameY += 36;
    } else {
        lcd.setFont(&fonts::Font2);
        lcd.drawString(fitName(lcd, m.name, maxTextW).c_str(),
                       x + w / 2, nameY);
        nameY += 20;
    }

    // ── Craft badge ─────────────────────────────────
    lcd.setTextColor(accentCol);
    lcd.setFont(&fonts::Font0);
    lcd.setTextDatum(TC_DATUM);
    lcd.drawString(m.craft.c_str(), x + w / 2, nameY);

    // ── Enrichment dot (signals tappable) ────────────
    if (m.enriched) {
        lcd.fillCircle(x + w - 10, y + 10, 3, COL_TEXT_ACCENT);
    }
}

// ─── Word-wrapped text drawing ──────────────────────────
static int drawWrapped(LGFX &lcd, const char* text,
                       int x, int y, int maxW, int lineH, int maxLines) {
    String str = text;
    int lines = 0, pos = 0;

    while (pos < (int)str.length() && lines < maxLines) {
        int end = pos, lastSp = pos;
        while (end < (int)str.length()) {
            char c = str.charAt(end);
            if (c == ' ')  lastSp = end;
            if (c == '\n') { lastSp = end; break; }
            if (lcd.textWidth(str.substring(pos, end + 1).c_str()) > maxW) break;
            end++;
        }

        int brk;
        if (end >= (int)str.length()) brk = str.length();
        else if (str.charAt(end) == '\n') brk = end;
        else if (lastSp > pos) brk = lastSp;
        else brk = end;

        String line = str.substring(pos, brk);
        line.trim();
        lcd.drawString(line.c_str(), x, y + lines * lineH);
        lines++;

        pos = brk;
        while (pos < (int)str.length() &&
               (str.charAt(pos) == ' ' || str.charAt(pos) == '\n')) pos++;
    }
    return lines;
}

// ═══════════════════════════════════════════════════════
//  Modal — astronaut detail overlay
// ═══════════════════════════════════════════════════════

static void renderModal(LGFX &lcd, const AppState &state) {
    if (modalCrewIndex < 0 || modalCrewIndex >= state.crew.count) {
        modalOpen = false;
        return;
    }
    const CrewMember &m = state.crew.members[modalCrewIndex];

    // Dark overlay
    lcd.fillRect(0, 0, DISPLAY_WIDTH, MAP_AREA_HEIGHT, 0x0000);

    // Card background
    lcd.fillRect(MODAL_X, MODAL_Y, MODAL_W, MODAL_H, 0x1082);
    lcd.drawRect(MODAL_X, MODAL_Y, MODAL_W, MODAL_H, COL_DIVIDER);

    // ── Avatar (large) ─────────────────────────────
    int avX = MODAL_X + MODAL_PAD;
    int avY = MODAL_Y + MODAL_PAD;
    drawAvatar(lcd, m, avX, avY, MODAL_AV);

    // ── Name ───────────────────────────────────────
    int txX = avX + MODAL_AV + 20;
    lcd.setTextColor(COL_TEXT_PRIMARY);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(TL_DATUM);
    lcd.drawString(m.name.c_str(), txX, avY + 5);

    // ── Agency · Nationality ───────────────────────
    String info;
    if (m.agency.length() > 0) info = m.agency;
    if (m.nationality.length() > 0) {
        if (info.length() > 0) info += "  ·  ";
        info += m.nationality;
    }
    if (info.length() > 0) {
        lcd.setTextColor(COL_TEXT_SECONDARY);
        lcd.setFont(&fonts::Font2);
        lcd.drawString(info.c_str(), txX, avY + 35);
    }

    // ── Craft badge ────────────────────────────────
    uint16_t craftCol = (m.craft == "ISS") ? COL_CREW_ISS : COL_CREW_OTHER;
    lcd.setTextColor(craftCol);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(m.craft.c_str(), txX, avY + 58);

    // ── Divider ────────────────────────────────────
    int divY = avY + MODAL_AV + 15;
    lcd.drawLine(MODAL_X + MODAL_PAD, divY,
                 MODAL_X + MODAL_W - MODAL_PAD, divY, COL_DIVIDER);

    // ── Bio text (wrapped) ─────────────────────────
    int bioY = divY + 12;
    int bioW = MODAL_W - 2 * MODAL_PAD;

    lcd.setTextDatum(TL_DATUM);
    if (m.bio.length() > 0) {
        lcd.setTextColor(COL_TEXT_SECONDARY);
        lcd.setFont(&fonts::Font2);
        drawWrapped(lcd, m.bio.c_str(),
                    MODAL_X + MODAL_PAD, bioY, bioW, 20, 8);
    } else if (!m.enriched) {
        lcd.setTextColor(COL_TEXT_DIM);
        lcd.setFont(&fonts::Font2);
        lcd.drawString("Loading profile data...",
                       MODAL_X + MODAL_PAD, bioY);
    } else {
        lcd.setTextColor(COL_TEXT_DIM);
        lcd.setFont(&fonts::Font2);
        lcd.drawString("No additional information available.",
                       MODAL_X + MODAL_PAD, bioY);
    }

    // ── Dismiss hint ───────────────────────────────
    lcd.setTextColor(COL_TEXT_DIM);
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(BR_DATUM);
    lcd.drawString("Tap anywhere to close",
                   MODAL_X + MODAL_W - MODAL_PAD,
                   MODAL_Y + MODAL_H - 12);

    lcd.setTextDatum(TL_DATUM);
    lcd.setFont(&fonts::Font0);
}

// ═══════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════

bool handleCrewTouch(LGFX &lcd, const AppState &state,
                     int16_t x, int16_t y) {
    if (modalOpen) {
        // Any tap dismisses the modal
        modalOpen = false;
        return true;
    }

    if (!state.crew.valid || state.crew.count == 0) return false;

    // Determine which grid cell was tapped
    if (y < CONTENT_Y || x < 0) return false;

    int strideW = cellW + GRID_PAD;
    int strideH = cellH + GRID_PAD;
    int col = x / strideW;
    int row = (y - CONTENT_Y) / strideH;

    if (col < 0 || col >= gridCols || row < 0 || row >= gridRows)
        return false;

    int idx = row * gridCols + col;
    if (idx >= 0 && idx < state.crew.count) {
        modalCrewIndex = idx;
        modalOpen = true;
        LOG_I("Crew modal [%d,%d] idx=%d: '%s'",
              row, col, idx, state.crew.members[idx].name.c_str());
        return true;
    }
    return false;
}

void renderCrewScreen(LGFX &lcd, const AppState &state, bool fullRedraw) {

    // ─── Modal rendering (takes over entire content area) ─
    if (modalOpen) {
        if (fullRedraw) {
            renderModal(lcd, state);
        }
        return;
    }

    // ─── Detect crew data changes (enrichment, avatars) ──
    bool crewChanged = (state.lastCrewUpdate != lastRenderedCrewUpdate);
    if (crewChanged) {
        lastRenderedCrewUpdate = state.lastCrewUpdate;
        fullRedraw = true;
    }

    if (!fullRedraw) return;  // Grid is static — only redraws on change

    // ─── Title + divider ─────────────────────────────
    lcd.setTextColor(COL_TEXT_ACCENT);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(TL_DATUM);
    char buf[64];
    snprintf(buf, sizeof(buf), "CREW IN SPACE: %d", state.crew.count);
    lcd.drawString(buf, 20, TITLE_Y);
    lcd.drawLine(0, DIVIDER_Y, DISPLAY_WIDTH, DIVIDER_Y, COL_DIVIDER);

    // ─── No-data state ──────────────────────────────
    if (!state.crew.valid || state.crew.count == 0) {
        lcd.setTextColor(COL_TEXT_DIM);
        lcd.setFont(&fonts::Font4);
        lcd.setTextDatum(CC_DATUM);
        unsigned long age = millis() - state.lastCrewUpdate;
        if (age < 15000) {
            lcd.drawString("Loading crew...",
                           DISPLAY_WIDTH / 2, MAP_AREA_HEIGHT / 2);
        } else {
            lcd.drawString("Crew API unavailable",
                           DISPLAY_WIDTH / 2, MAP_AREA_HEIGHT / 2 - 15);
            lcd.setFont(&fonts::Font2);
            lcd.drawString("Will retry automatically",
                           DISPLAY_WIDTH / 2, MAP_AREA_HEIGHT / 2 + 20);
        }
        lcd.setFont(&fonts::Font0);
        lcd.setTextDatum(TL_DATUM);
        return;
    }

    // ─── Compute grid and render cards ───────────────
    computeGridLayout(state.crew.count);

    for (int i = 0; i < state.crew.count; i++) {
        int row = i / gridCols;
        int col = i % gridCols;
        int x = col * (cellW + GRID_PAD);
        int y = CONTENT_Y + row * (cellH + GRID_PAD);

        if (y + cellH > (int)MAP_AREA_HEIGHT) break;
        drawCrewCard(lcd, state.crew.members[i], x, y, cellW, cellH);
    }

    lcd.setFont(&fonts::Font0);
    lcd.setTextDatum(TL_DATUM);
}
