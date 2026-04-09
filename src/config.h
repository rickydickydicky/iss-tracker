#pragma once

// ─── WiFi ───────────────────────────────────────────────
// Credentials are stored in NVS (non-volatile storage).
// On first boot, a captive portal lets the user configure WiFi.
#define WIFI_TIMEOUT_MS   10000
#define WIFI_AP_SSID      "ISS-Tracker"    // AP name shown during setup

// ─── API Endpoints ──────────────────────────────────────
#define API_ISS_POSITION  "https://api.wheretheiss.at/v1/satellites/25544"
#define API_ISS_TLE       "https://api.wheretheiss.at/v1/satellites/25544/tles"
// Primary: corquaid's maintained JSON (updated with each launch/landing)
#define API_ASTROS         "https://corquaid.github.io/international-space-station-APIs/JSON/people-in-space.json"
// Fallback: original Open Notify (unreliable, often down)
#define API_ASTROS_FALLBACK "http://api.open-notify.org/astros.json"
// TheSpaceDevs: enrichment data (bios, photos, nationality)
#define API_SPACEDEVS       "https://ll.thespacedevs.com/2.2.0/astronaut/?in_space=true&format=json&limit=25"

// ─── Poll Intervals (ms) ────────────────────────────────
#define POLL_POSITION_MS   10000    // 10 seconds (respect rate limit)
#define POLL_CREW_MS       21600000 // 6 hours
#define POLL_TLE_MS        86400000 // 24 hours

// ─── Home Location (for pass predictions) ───────────────
#define HOME_LAT  -37.8136   // Melbourne, AU
#define HOME_LON  144.9631

// ─── Display ────────────────────────────────────────────
// Waveshare ESP32-S3-Touch-LCD-4.3: 800x480 RGB LCD
#define DISPLAY_WIDTH   800
#define DISPLAY_HEIGHT  480

// ─── UI Layout ──────────────────────────────────────────
#define TAB_BAR_HEIGHT  50
#define MAP_AREA_HEIGHT (DISPLAY_HEIGHT - TAB_BAR_HEIGHT)  // 430px

// ─── Ground Track ───────────────────────────────────────
#define TRACK_BUFFER_SIZE 90  // ~15 minutes of positions at 10s intervals

// ─── NTP ────────────────────────────────────────────────
#define NTP_SERVER        "pool.ntp.org"
// Melbourne timezone: AEST (UTC+10) / AEDT (UTC+11)
// POSIX TZ string handles DST automatically
#define TIMEZONE_POSIX    "AEST-10AEDT,M10.1.0,M4.1.0/3"

// ─── Display Sleep Defaults ─────────────────────────────
#define DEFAULT_SLEEP_ENABLED   false  // Off by default — user enables via Setup tab
#define DEFAULT_SLEEP_START     22     // 10 PM
#define DEFAULT_SLEEP_END       7      // 7 AM
#define SLEEP_INACTIVITY_MS     30000  // 30s of no touch → return to sleep

// ─── Logging ────────────────────────────────────────────
#define LOG_I(fmt, ...) Serial.printf("[INFO ] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) Serial.printf("[WARN ] " fmt "\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
