#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "iss_data.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "settings.h"
#include "screens/screen_manager.h"

// ─── Global State ───────────────────────────────────────
AppState appState;

// ─── FreeRTOS Mutex ─────────────────────────────────────
// Protects appState when the network task writes and the
// render loop reads. Both sides hold it only briefly.
static SemaphoreHandle_t stateMutex;

// ─── Shadow buffers for network task ────────────────────
static IssPosition  shadowPosition;
static CrewData     shadowCrew;

// ─── Flags set by network task, read by main loop ───────
static volatile bool newPositionReady = false;
static volatile bool newCrewReady     = false;

// ─── Network Task (runs on Core 0) ─────────────────────
static void networkTask(void *param) {
    unsigned long lastPositionPoll = 0;
    unsigned long lastCrewPoll     = 0;
    bool crewLoaded = false;

    // ── Enrichment state ─────────────────────────
    bool enrichmentDone  = false;   // Bio/nationality fetched from SpaceDevs
    int  avatarProgress  = 0;       // Next crew index to download avatar for

    vTaskDelay(pdMS_TO_TICKS(1000));
    LOG_I("[Net] Network task started on Core %d", xPortGetCoreID());

    for (;;) {
        unsigned long now = millis();

        // ── WiFi health check ────────────────────────
        if (!wifiIsConnected()) {
            LOG_W("[Net] WiFi lost — reconnecting...");
            wifiReconnect();

            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50))) {
                appState.wifiConnected = wifiIsConnected();
                xSemaphoreGive(stateMutex);
            }

            if (!wifiIsConnected()) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        // Update WiFi status
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50))) {
            appState.wifiConnected = wifiIsConnected();
            crewLoaded = appState.crew.valid;
            xSemaphoreGive(stateMutex);
        }

        // ── Poll ISS position ────────────────────────
        if (now - lastPositionPoll >= POLL_POSITION_MS) {
            lastPositionPoll = now;
            IssPosition tempPos;
            bool ok = fetchIssPosition(tempPos);

            if (ok && tempPos.valid) {
                shadowPosition = tempPos;
                newPositionReady = true;
            } else {
                if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50))) {
                    appState.apiError = true;
                    xSemaphoreGive(stateMutex);
                }
            }
        }

        // ── Poll crew data ───────────────────────────
        unsigned long crewInterval = crewLoaded ? POLL_CREW_MS : 30000;
        if (now - lastCrewPoll >= crewInterval) {
            lastCrewPoll = now;
            CrewData tempCrew;
            tempCrew.count = 0;
            tempCrew.issCount = 0;
            tempCrew.valid = false;

            bool ok = fetchCrew(tempCrew);
            if (ok && tempCrew.valid) {
                // Free old avatar PSRAM buffers before overwriting
                for (int i = 0; i < shadowCrew.count; i++) {
                    if (shadowCrew.members[i].imageData) {
                        free(shadowCrew.members[i].imageData);
                        shadowCrew.members[i].imageData = nullptr;
                    }
                }
                shadowCrew = tempCrew;
                newCrewReady = true;

                // Reset enrichment when crew list refreshes
                enrichmentDone = false;
                avatarProgress = 0;
            }
        }

        // ── Crew enrichment (bios, agencies, nationalities) ──
        if (shadowCrew.valid && !enrichmentDone) {
            LOG_I("[Net] Starting crew enrichment...");
            if (fetchCrewEnrichment(shadowCrew)) {
                newCrewReady = true;
            }
            enrichmentDone = true;
            avatarProgress = 0;  // Start avatar downloads
        }

        // ── Progressive avatar downloads (one per loop iteration) ──
        if (shadowCrew.valid && enrichmentDone &&
            avatarProgress < shadowCrew.count) {

            CrewMember &m = shadowCrew.members[avatarProgress];

            if (m.enriched && m.imageUrl.length() > 4 && !m.imageLoaded) {
                uint8_t* imgData = nullptr;
                size_t   imgSize = 0;

                if (fetchAvatarImage(m.imageUrl.c_str(), &imgData, &imgSize)) {
                    m.imageData   = imgData;
                    m.imageSize   = imgSize;
                    m.imageLoaded = true;
                    newCrewReady  = true;
                    LOG_I("[Net] Avatar %d/%d loaded: '%s' (%d bytes)",
                          avatarProgress + 1, shadowCrew.count,
                          m.name.c_str(), imgSize);
                } else {
                    LOG_W("[Net] Avatar download failed for '%s'", m.name.c_str());
                }
            }
            avatarProgress++;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ─── Merge shadow data into appState (main loop) ───────
static void mergeNetworkData() {
    if (newPositionReady) {
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
            appState.position = shadowPosition;
            appState.apiError = false;
            appState.track.push(shadowPosition.latitude, shadowPosition.longitude);
            appState.lastPositionUpdate = millis();
            newPositionReady = false;
            xSemaphoreGive(stateMutex);
        }
    }

    if (newCrewReady) {
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
            appState.crew = shadowCrew;
            appState.lastCrewUpdate = millis();
            newCrewReady = false;
            xSemaphoreGive(stateMutex);
        }
    }
}

// ─── Onboarding: wait for touch or timeout ──────────────
// Returns true if the user touched the screen during the wait.
static bool waitForTouchOrTimeout(unsigned long durationMs) {
    unsigned long start = millis();
    while (millis() - start < durationMs) {
        if (screenCheckTouch()) {
            // Wait for finger lift to avoid accidental double-triggers
            delay(200);
            return true;
        }
        delay(50);
    }
    return false;
}

// ─── Onboarding: run the captive portal until connected ─
static void runPortalUntilConnected() {
    screenShowStatus("WiFi Setup", "Connect to 'ISS-Tracker' on your phone");
    wifiStartPortal();

    LOG_I("Waiting for user to configure WiFi via portal...");

    while (!wifiPortalLoop()) {
        delay(10);  // Process DNS + HTTP requests
    }

    // Give the success page time to reach the phone
    delay(1500);
    wifiStopPortal();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    LOG_I("===================================");
    LOG_I("  ISS Tracker — Starting up...");
    LOG_I("  Waveshare ESP32-S3-Touch-LCD-4.3");
    LOG_I("===================================");

    // Init state
    memset(&appState, 0, sizeof(AppState));
    appState.activeScreen = SCREEN_MAP;
    appState.track.reset();

    // Load saved settings from NVS
    settingsLoad(appState.settings);

    // Create mutex
    stateMutex = xSemaphoreCreateMutex();

    // Init display
    screenManagerInit();

    // ════════════════════════════════════════════════
    //  LANDING PAGE
    // ════════════════════════════════════════════════
    screenShowLanding();

    // Wait 3 seconds — if user touches, enter WiFi setup mode
    bool userRequestedSetup = waitForTouchOrTimeout(3000);

    if (userRequestedSetup) {
        LOG_I("User touched screen — entering WiFi setup");
        wifiClearCredentials();
    }

    // ════════════════════════════════════════════════
    //  WIFI CONNECTION
    // ════════════════════════════════════════════════
    bool connected = false;

    if (!userRequestedSetup && wifiHasSavedCredentials()) {
        // Try connecting with saved credentials
        String ssid = wifiGetSavedSSID();
        char buf[64];
        snprintf(buf, sizeof(buf), "Connecting to %s...", ssid.c_str());
        screenShowStatus(buf);

        connected = wifiConnectSaved();

        if (!connected) {
            LOG_W("Saved credentials failed — launching portal");
            screenShowStatus("Connection failed", "Launching WiFi setup...");
            delay(1500);
        }
    }

    if (!connected) {
        // No saved credentials, or they failed — run the captive portal
        runPortalUntilConnected();
        connected = wifiIsConnected();
    }

    appState.wifiConnected = connected;

    if (connected) {
        LOG_I("WiFi connected: %s", WiFi.localIP().toString().c_str());
        screenShowStatus("Connected!", WiFi.localIP().toString().c_str());
        delay(1000);

        // ════════════════════════════════════════════════
        //  NTP TIME SYNC
        // ════════════════════════════════════════════════
        screenShowStatus("Syncing time...");
        configTime(0, 0, NTP_SERVER);
        setenv("TZ", TIMEZONE_POSIX, 1);
        tzset();

        // Wait up to 5 seconds for NTP sync
        struct tm ti;
        for (int i = 0; i < 50; i++) {
            if (getLocalTime(&ti, 100)) {
                appState.ntpSynced = true;
                char buf[40];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
                LOG_I("NTP synced: %s", buf);
                break;
            }
        }
        if (!appState.ntpSynced) {
            LOG_W("NTP sync timed out — will retry in background");
        }

        // ════════════════════════════════════════════════
        //  INITIAL DATA FETCH
        // ════════════════════════════════════════════════
        screenShowStatus("Fetching ISS data...");

        appState.apiError = !fetchIssPosition(appState.position);
        if (appState.position.valid) {
            appState.track.push(appState.position.latitude, appState.position.longitude);
            appState.lastPositionUpdate = millis();
        }

        fetchCrew(appState.crew);
        appState.lastCrewUpdate = millis();

        // Seed shadow buffer so network task can enrich immediately
        shadowCrew = appState.crew;
    } else {
        LOG_E("WiFi connection failed — proceeding without data");
        screenShowStatus("No WiFi", "Tracker will retry when connected");
        delay(2000);
    }

    // ════════════════════════════════════════════════
    //  LAUNCH BACKGROUND NETWORK TASK
    // ════════════════════════════════════════════════
    xTaskCreatePinnedToCore(
        networkTask,    // function
        "network",      // name
        12288,          // stack bytes (enrichment + avatar downloads need headroom)
        NULL,           // parameter
        1,              // priority
        NULL,           // handle
        0               // Core 0
    );

    LOG_I("Setup complete. Main loop on Core %d.", xPortGetCoreID());
}

// ─── Display sleep logic ────────────────────────────────
// Returns true if current local time falls within the sleep window.
static bool isInSleepWindow(const UserSettings &s) {
    if (!s.sleepEnabled) return false;

    struct tm ti;
    time_t now = time(nullptr);
    localtime_r(&now, &ti);
    int hour = ti.tm_hour;

    if (s.sleepStartHour > s.sleepEndHour) {
        // Wraps midnight: e.g. 22-7 means 22,23,0,1,2,3,4,5,6
        return (hour >= s.sleepStartHour || hour < s.sleepEndHour);
    } else {
        // Same-day range (unlikely but handle it)
        return (hour >= s.sleepStartHour && hour < s.sleepEndHour);
    }
}

// ─── Wake state machine ─────────────────────────────────
// Instead of blocking delay() calls during wake, we track
// the wake phase with millis() and let the main loop keep running.
enum WakePhase { WAKE_IDLE, WAKE_RESETTING, WAKE_SETTLING };
static WakePhase wakePhase = WAKE_IDLE;
static unsigned long wakePhaseStart = 0;
static unsigned long lastTouchActivity = 0;   // Inactivity-based sleep timeout

void loop() {
    unsigned long now = millis();

    // ─── Wake state machine (non-blocking GT911 recovery) ──
    if (wakePhase == WAKE_RESETTING) {
        // GT911 reset is in progress (started by touch-to-wake).
        // screenResetTouch() was called — wait for it to settle.
        // The reset itself takes ~200ms (delays inside screenResetTouch).
        // After return, we enter settling phase for a few more reads.
        wakePhase = WAKE_SETTLING;
        wakePhaseStart = now;
        return;
    }
    if (wakePhase == WAKE_SETTLING) {
        // Give GT911 a few loop cycles to produce stable reads
        if (now - wakePhaseStart < 200) {
            screenCheckTouch();  // Discard potentially stale data
            return;
        }
        // Settling complete — resume normal operation
        wakePhase = WAKE_IDLE;
        LOG_I("GT911 wake sequence complete — touch ready");
    }

    // ─── Display sleep management ─────────────────────
    if (appState.ntpSynced && appState.settings.sleepEnabled) {
        bool shouldSleep = isInSleepWindow(appState.settings);

        if (shouldSleep && !appState.displayAsleep) {
            // Enter sleep after inactivity timeout
            bool inactivityExpired = (lastTouchActivity == 0) ||
                (now - lastTouchActivity > SLEEP_INACTIVITY_MS);
            if (inactivityExpired) {
                appState.displayAsleep = true;
                screenSetBacklight(false);
                LOG_I("Display entering sleep — %lus inactive",
                      lastTouchActivity ? (now - lastTouchActivity) / 1000 : 0);
            }
        } else if (!shouldSleep && appState.displayAsleep) {
            // Sleep window ended — wake up naturally
            appState.displayAsleep = false;
            appState.lastWakeTime = now;
            lastTouchActivity = now;
            screenSetBacklight(true);
            screenResetTouch();
            screenForceRedraw();
            wakePhase = WAKE_RESETTING;
            LOG_I("Display waking — sleep window ended");
            return;
        }

        // Touch-to-wake during sleep
        if (appState.displayAsleep && screenCheckTouch()) {
            appState.displayAsleep = false;
            appState.lastWakeTime = now;
            lastTouchActivity = now;
            screenSetBacklight(true);
            screenForceRedraw();

            // Hard-reset the GT911 touch controller.
            // This is non-negotiable — after prolonged sleep the GT911
            // enters an internal low-power state and dummy reads alone
            // don't reliably recover it.
            screenResetTouch();
            wakePhase = WAKE_RESETTING;

            LOG_I("Display waking — touch detected");
            return;
        }

        // If asleep, skip rendering but keep network task running
        if (appState.displayAsleep) {
            delay(100);
            return;
        }
    }

    // ─── NTP background retry ─────────────────────────
    if (!appState.ntpSynced && appState.wifiConnected) {
        static unsigned long lastNtpRetry = 0;
        if (now - lastNtpRetry > 30000) {
            lastNtpRetry = now;
            struct tm ti;
            if (getLocalTime(&ti, 100)) {
                appState.ntpSynced = true;
                LOG_I("NTP synced (background retry)");
            }
        }
    }

    // ─── Normal operation ─────────────────────────────
    if (screenManagerHandleTouch(appState)) {
        lastTouchActivity = now;  // Reset inactivity timer on any touch
    }
    mergeNetworkData();
    screenManagerRender(appState);
    yield();
}
