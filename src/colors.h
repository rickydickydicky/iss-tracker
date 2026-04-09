#pragma once
#include <cstdint>

// ─── ISS Tracker Dark Theme ────────────────────────────
// All colors in RGB565 format
// ────────────────────────────────────────────────────────

// Background — consistent black across all screens
#define COL_BG              0x0000  // Pure black
#define COL_OCEAN           0x0000  // Black — matches background for unified look

// Landmasses — gold/amber on black for high contrast
#define COL_COASTLINE       0xFEA0  // Gold — (255, 213, 0)
#define COL_COASTLINE_FILL  0x0861  // Very dark green-grey — optional fill

// Map grid — very subtle so it doesn't compete with coastlines
#define COL_GRID            0x18A3  // Dim charcoal grid lines
#define COL_EQUATOR         0x3186  // Slightly brighter equator
#define COL_GRID_LABEL      0x4208  // Dim text for lat/lon labels

// ISS tracking — cyan pops against gold/black
#define COL_ISS_PRIMARY     0x07FF  // Bright cyan — ISS dot
#define COL_ISS_GLOW        0x033F  // Dim cyan — glow ring
#define COL_TRACK_OLD       0x0168  // Very dim cyan — oldest track points
#define COL_TRACK_MID       0x0310  // Medium cyan — middle track
#define COL_TRACK_NEW       0x05BF  // Bright-ish cyan — recent track

// Home marker
#define COL_HOME            0xF800  // Red — stands out against gold coastlines

// Text
#define COL_TEXT_PRIMARY     0xFFFF  // White
#define COL_TEXT_SECONDARY   0xAD75  // Light grey
#define COL_TEXT_DIM         0x632C  // Medium grey
#define COL_TEXT_ACCENT      0x07FF  // Cyan — matches ISS dot

// Tab bar
#define COL_TAB_BG          0x10A2  // Dark charcoal
#define COL_TAB_DIVIDER     0x18E3  // Slightly lighter divider
#define COL_TAB_ACTIVE_LINE 0x07FF  // Cyan accent bar
#define COL_TAB_ACTIVE_TEXT 0xFFFF  // White for active tab
#define COL_TAB_INACTIVE    0x632C  // Grey for inactive tabs

// Status indicators
#define COL_STALE           0xF800  // Red — stale/error
#define COL_OK              0x07E0  // Green — connected
#define COL_DAYLIGHT        0xFFE0  // Yellow
#define COL_ECLIPSED        0x4208  // Dim grey
#define COL_PENUMBRA        0xFDA0  // Orange

// Stats screen
#define COL_STAT_VALUE      0xFFFF  // White — big numbers
#define COL_STAT_LABEL      0x8410  // Grey — labels
#define COL_STAT_UNIT       0x632C  // Dimmer — units
#define COL_DIVIDER         0x18E3  // Section dividers

// Crew screen
#define COL_CREW_ISS        0x07FF  // Cyan — ISS crew
#define COL_CREW_OTHER      0x8410  // Grey — other craft
#define COL_CREW_CRAFT_BG   0x0861  // Dark badge background
