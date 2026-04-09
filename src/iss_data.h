#pragma once
#include <Arduino.h>
#include "config.h"

// ─── ISS Position ───────────────────────────────────────
struct IssPosition {
    float latitude;
    float longitude;
    float altitude;       // km
    float velocity;       // km/h
    String visibility;    // "daylight", "eclipsed", "penumbra"
    float footprint;      // km diameter of visibility circle
    unsigned long timestamp;
    bool valid;
};

// ─── Ground Track (circular buffer) ─────────────────────
struct GroundTrack {
    float lat[TRACK_BUFFER_SIZE];
    float lon[TRACK_BUFFER_SIZE];
    int head;
    int count;

    void reset() { head = 0; count = 0; }

    void push(float latitude, float longitude) {
        lat[head] = latitude;
        lon[head] = longitude;
        head = (head + 1) % TRACK_BUFFER_SIZE;
        if (count < TRACK_BUFFER_SIZE) count++;
    }

    // Get position at index (0 = oldest)
    void get(int index, float &outLat, float &outLon) const {
        int i = (head - count + index + TRACK_BUFFER_SIZE) % TRACK_BUFFER_SIZE;
        outLat = lat[i];
        outLon = lon[i];
    }
};

// ─── Crew ───────────────────────────────────────────────
struct CrewMember {
    String name;
    String craft;        // "ISS", "Tiangong", etc.
    // Enrichment data (from TheSpaceDevs API)
    String bio;          // Brief biography
    String nationality;  // e.g. "American", "Russian"
    String agency;       // e.g. "NASA", "ESA", "JAXA"
    String imageUrl;     // Profile thumbnail URL
    uint8_t* imageData;  // JPEG bytes in PSRAM (nullptr if not loaded)
    size_t imageSize;    // JPEG data size in bytes
    bool enriched;       // True when enrichment data fetched
    bool imageLoaded;    // True when avatar JPEG downloaded
};

struct CrewData {
    CrewMember members[20]; // Max 20 people in space
    int count;
    int issCount;           // Filtered count for ISS only
    bool valid;
};

// ─── User Settings (persisted in NVS) ───────────────────
struct UserSettings {
    // Display sleep
    bool    sleepEnabled;     // Auto-sleep during night hours
    uint8_t sleepStartHour;   // 0-23, e.g. 22 = 10 PM
    uint8_t sleepEndHour;     // 0-23, e.g. 7 = 7 AM

    // Pass prediction (placeholder — wired up when predictor is built)
    bool passNotifyEnabled;

    // Home location
    float homeLat;
    float homeLon;
};

// ─── App State ──────────────────────────────────────────
enum Screen {
    SCREEN_MAP,
    SCREEN_STATS,
    SCREEN_CREW,
    SCREEN_SETTINGS,
    SCREEN_COUNT  // Keep last — used for cycling
};

struct AppState {
    IssPosition position;
    GroundTrack track;
    CrewData crew;
    UserSettings settings;
    Screen activeScreen;
    bool wifiConnected;
    bool apiError;
    bool ntpSynced;
    bool displayAsleep;        // True when backlight is off (sleep mode)
    unsigned long lastWakeTime; // millis() of last touch during sleep
    unsigned long lastPositionUpdate;
    unsigned long lastCrewUpdate;
};
