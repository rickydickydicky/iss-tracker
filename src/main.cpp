#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "iss_data.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "settings.h"
#include "screens/screen_manager.h"
#include <orbit_alert.h>  // formatLocalDayTime for logs
#include <orbit_pass.h>
#include <orbit_sgp4.h>
#include <orbit_tle.h>

// ─── Global State ───────────────────────────────────────
AppState appState;

// ─── FreeRTOS Mutex ─────────────────────────────────────
// Protects appState when the network task writes and the
// render loop reads. Both sides hold it only briefly.
static SemaphoreHandle_t stateMutex;

// ─── Shadow buffers for network task ────────────────────
static IssPosition  shadowPosition;
static CrewData     shadowCrew;

// Pass predictions: written by the network task *under stateMutex*,
// merged into appState by the main loop (also under the mutex).
static PassPredictions shadowPasses;

// ─── Flags set by network task, read by main loop ───────
static volatile bool newPositionReady = false;
static volatile bool newCrewReady     = false;
static volatile bool newPassesReady   = false;

// ─── Pass prediction (network task only) ────────────────
// Unix times before this mean the clock hasn't been set by NTP yet.
static const time_t MIN_VALID_UNIX = 1700000000;  // Nov 2023

static void yieldToRtos(void*) {
    vTaskDelay(1);  // let the idle task run (task watchdog) during long searches
}

// Recompute predictions when the elements change, the first predicted
// pass is over, or periodically. Returns true if shadowPasses was updated.
static bool updatePassPredictions(const orbit::Sgp4* sat, bool tleChanged) {
    static PassPredictions work;           // static: ~1 KB, keep it off the stack
    static bool haveResult = false;
    static int64_t lastRun = 0;

    const time_t now = time(nullptr);
    PassStatus status;

    if (!sat) {
        status = PASS_STATUS_NO_TLE;
    } else if (now < MIN_VALID_UNIX) {
        status = PASS_STATUS_NO_TIME;
    } else {
        // Decide whether the existing prediction is still good
        const int64_t sinceRun = (int64_t)now - lastRun;
        bool due = tleChanged || !haveResult || sinceRun > 6 * 3600;
        if (work.status == PASS_STATUS_OK) {
            if (work.count > 0 && now > work.passes[0].los) due = true;  // first pass over
            if (work.count == 0 && sinceRun > 3600) due = true;
        } else if (sinceRun > 600) {
            due = true;   // retry after stale elements / errors (cheap check first)
        }
        if (!due) return false;

        const double ageDays = orbit::unixToJd((double)now) - sat->epochJd;
        if (ageDays > TLE_MAX_AGE_DAYS) {
            LOG_W("[Pass] TLE is %.1f days old — not predicting", ageDays);
            lastRun = now;
            status = PASS_STATUS_STALE_TLE;
        } else {
            float homeLat = HOME_LAT, homeLon = HOME_LON;
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50))) {
                homeLat = appState.settings.homeLat;
                homeLon = appState.settings.homeLon;
                xSemaphoreGive(stateMutex);
            }
            orbit::Observer obs;
            orbit::observerInit(obs, homeLat, homeLon, HOME_ALT_M / 1000.0);
            orbit::PassOptions opt;
            opt.minElevationDeg = PASS_MIN_ELEVATION_DEG;
            opt.maxSunAltDeg    = PASS_SUN_MAX_ALT_DEG;

            unsigned long t0 = millis();
            orbit::Sgp4Error err = orbit::SGP4_OK;
            int n = orbit::predictPasses(*sat, obs, (int64_t)now,
                                         (int64_t)now + PASS_PREDICT_HOURS * 3600,
                                         opt, work.passes, PASS_MAX_COUNT,
                                         yieldToRtos, nullptr, &err);
            unsigned long dt = millis() - t0;
            lastRun = now;
            haveResult = true;
            work.computedAt = now;
            work.tleEpochJd = sat->epochJd;
            if (n < 0) {
                LOG_E("[Pass] SGP4 error %d during prediction", (int)err);
                work.count = 0;
                status = PASS_STATUS_ERROR;
            } else {
                work.count = n;
                status = PASS_STATUS_OK;
                int visible = 0;
                for (int i = 0; i < n; i++) if (work.passes[i].visible) visible++;
                LOG_I("[Pass] %d passes (%d visible) in next %dh, computed in %lu ms "
                      "(TLE age %.1f d)", n, visible, PASS_PREDICT_HOURS, dt, ageDays);
                for (int i = 0; i < n; i++) {
                    const orbit::Pass &p = work.passes[i];
                    char when[24];
                    orbit::formatLocalDayTime(p.aos, now, when, sizeof(when));
                    LOG_I("[Pass]   %s  %s->%s  max %.0f deg%s", when,
                          orbit::compassPoint8(p.aosAzDeg), orbit::compassPoint8(p.losAzDeg),
                          p.maxElDeg, p.visible ? "  VISIBLE" : "");
                }
            }
        }
    }

    if (status != PASS_STATUS_OK) {
        if (haveResult && work.status == status) return false;  // unchanged
        work.count = 0;
        haveResult = true;
    }
    work.status = status;

    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
        shadowPasses = work;
        newPassesReady = true;
        xSemaphoreGive(stateMutex);
        return true;
    }
    return false;
}

// ─── Network Task (runs on Core 0) ─────────────────────
static void networkTask(void *param) {
    unsigned long lastPositionPoll = 0;
    unsigned long lastCrewPoll     = 0;
    bool crewLoaded = false;

    // ── Enrichment state ─────────────────────────
    bool enrichmentDone  = false;   // Bio/nationality fetched from SpaceDevs
    int  avatarProgress  = 0;       // Next crew index to download avatar for

    // ── Orbital elements for pass prediction ─────
    static orbit::Sgp4 issSat;      // static: keep ~260 bytes off the task stack
    bool tleValid      = false;
    bool tleAttempted  = false;
    unsigned long lastTlePoll = 0;

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

        // ── Orbital elements (TLE) ───────────────────
        unsigned long tleInterval = tleValid ? POLL_TLE_MS : POLL_TLE_RETRY_MS;
        bool tleChanged = false;
        if (!tleAttempted || now - lastTlePoll >= tleInterval) {
            tleAttempted = true;
            lastTlePoll  = now;
            char l1[72], l2[72];
            if (fetchIssTle(l1, l2, sizeof(l1))) {
                orbit::Tle tle;
                orbit::Sgp4 sat;
                if (orbit::tleParse(l1, l2, tle) == orbit::TLE_OK &&
                    orbit::sgp4Init(tle, sat) == orbit::SGP4_OK) {
                    issSat     = sat;
                    tleValid   = true;
                    tleChanged = true;
                    LOG_I("[Net] TLE epoch JD %.5f", tle.epochJd);
                } else {
                    LOG_E("[Net] TLE rejected by SGP4 init");
                }
            }
        }

        // ── Pass predictions (cheap no-op unless due) ──
        updatePassPredictions(tleValid ? &issSat : nullptr, tleChanged);

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

    if (newPassesReady) {
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
            appState.passes = shadowPasses;
            newPassesReady = false;
            xSemaphoreGive(stateMutex);
            // Static screens only repaint on a full redraw
            if (appState.activeScreen == SCREEN_SETTINGS) screenForceRedraw();
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

    // ─── Pass alert ───────────────────────────────────
    // A visible ISS pass is imminent or underway: keep (or turn) the display
    // on so the banner can be seen, even inside the sleep window.
    bool passAlertActive = false;
    if (appState.settings.passNotifyEnabled && appState.ntpSynced &&
        appState.passes.status == PASS_STATUS_OK) {
        orbit::PassAlert a = orbit::passAlert(appState.passes.passes, appState.passes.count,
                                              (int64_t)time(nullptr), PASS_ALERT_LEAD_S);
        passAlertActive = a.active;
    }

    // ─── Display sleep management ─────────────────────
    if (appState.ntpSynced && appState.settings.sleepEnabled) {
        bool shouldSleep = isInSleepWindow(appState.settings) && !passAlertActive;

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
            // Sleep window ended (or a pass alert started) — wake up
            appState.displayAsleep = false;
            appState.lastWakeTime = now;
            lastTouchActivity = now;
            screenSetBacklight(true);
            screenResetTouch();
            screenForceRedraw();
            wakePhase = WAKE_RESETTING;
            LOG_I("Display waking — %s", passAlertActive ? "ISS pass alert" : "sleep window ended");
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
