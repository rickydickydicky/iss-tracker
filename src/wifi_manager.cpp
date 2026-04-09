#include "wifi_manager.h"
#include "portal_html.h"
#include <Preferences.h>
#include <DNSServer.h>
#include <WebServer.h>

// ─── NVS Keys ──────────────────────────────────────────
static const char* NVS_NS   = "wifi";
static const char* NVS_SSID = "ssid";
static const char* NVS_PASS = "pass";

// ─── Portal Objects (created on demand) ─────────────────
static DNSServer* dnsServer = nullptr;
static WebServer* webServer = nullptr;
static String     scannedHTML;          // <option> tags from scan
static volatile bool portalDone = false;

// ─── Reconnect State ───────────────────────────────────
static unsigned long lastReconnectAttempt = 0;
static const unsigned long RECONNECT_INTERVAL = 5000;

// ─── Credential Cache (avoid NVS reads on every reconnect) ─
static String cachedSSID;
static String cachedPass;
static bool   cacheLoaded = false;

static void loadCacheIfNeeded() {
    if (cacheLoaded) return;
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    cachedSSID = prefs.getString(NVS_SSID, "");
    cachedPass = prefs.getString(NVS_PASS, "");
    prefs.end();
    cacheLoaded = true;
}

// ═══════════════════════════════════════════════════════
//  Public API — Credential Management
// ═══════════════════════════════════════════════════════

bool wifiHasSavedCredentials() {
    loadCacheIfNeeded();
    return cachedSSID.length() > 0;
}

String wifiGetSavedSSID() {
    loadCacheIfNeeded();
    return cachedSSID;
}

bool wifiConnectSaved() {
    loadCacheIfNeeded();
    if (cachedSSID.length() == 0) return false;

    LOG_I("Connecting to WiFi: %s", cachedSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(cachedSSID.c_str(), cachedPass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < WIFI_TIMEOUT_MS)) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        LOG_I("Connected! IP: %s", WiFi.localIP().toString().c_str());
        return true;
    }

    LOG_E("WiFi connection timed out");
    return false;
}

void wifiClearCredentials() {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.clear();
    prefs.end();
    cachedSSID  = "";
    cachedPass  = "";
    cacheLoaded = true;
    LOG_I("WiFi credentials cleared from NVS");
}

// ═══════════════════════════════════════════════════════
//  Captive Portal — Web Handlers
// ═══════════════════════════════════════════════════════

static void handleRoot() {
    String html = String(PORTAL_HTML_TEMPLATE);
    html.replace("%NETWORKS%", scannedHTML);
    html.replace("%MESSAGE%", "");
    webServer->send(200, "text/html", html);
}

static void handleSave() {
    String ssid = webServer->arg("ssid");
    String pass = webServer->arg("pass");

    // Manual SSID entry
    if (ssid == "__manual__") {
        ssid = webServer->arg("manual_ssid");
    }

    if (ssid.length() == 0) {
        String html = String(PORTAL_HTML_TEMPLATE);
        html.replace("%NETWORKS%", scannedHTML);
        html.replace("%MESSAGE%",
            "<p class=\"msg error\">Please select or enter a network name.</p>");
        webServer->send(200, "text/html", html);
        return;
    }

    LOG_I("[Portal] Connecting to '%s'...", ssid.c_str());

    // Attempt connection (AP stays up in AP_STA mode)
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        // ── Success: save to NVS ───────────────────
        Preferences prefs;
        prefs.begin(NVS_NS, false);
        prefs.putString(NVS_SSID, ssid);
        prefs.putString(NVS_PASS, pass);
        prefs.end();

        cachedSSID  = ssid;
        cachedPass  = pass;
        cacheLoaded = true;

        LOG_I("[Portal] Connected to '%s' — credentials saved", ssid.c_str());

        // Send success page
        webServer->send(200, "text/html",
            "<!DOCTYPE html><html><head>"
            "<meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<style>"
            "body{background:#0a0a0a;color:#fff;font-family:-apple-system,sans-serif;"
            "text-align:center;padding:80px 20px}"
            "h2{color:#00e5ff;letter-spacing:4px;font-weight:300;margin-bottom:16px}"
            "p{color:#888;font-size:15px;line-height:1.6}"
            ".ok{display:inline-block;width:60px;height:60px;border:2px solid #00e5ff;"
            "border-radius:50%;line-height:60px;font-size:28px;margin-bottom:20px}"
            "</style></head><body>"
            "<div class='ok'>&#10003;</div>"
            "<h2>CONNECTED</h2>"
            "<p>ISS Tracker is now connected to<br><strong style='color:#fff'>"
            + ssid +
            "</strong></p>"
            "<p style='margin-top:30px;color:#444;font-size:13px'>"
            "You can disconnect from ISS-Tracker WiFi<br>and close this page.</p>"
            "</body></html>");

        portalDone = true;
    } else {
        // ── Failed ─────────────────────────────────
        WiFi.disconnect();
        LOG_W("[Portal] Connection to '%s' failed", ssid.c_str());

        String html = String(PORTAL_HTML_TEMPLATE);
        html.replace("%NETWORKS%", scannedHTML);
        html.replace("%MESSAGE%",
            "<p class=\"msg error\">Connection failed. Check the password and try again.</p>");
        webServer->send(200, "text/html", html);
    }
}

// Catch-all redirect for captive portal detection
static void handleNotFound() {
    webServer->sendHeader("Location", "http://192.168.4.1/", true);
    webServer->send(302, "text/plain", "");
}

// ═══════════════════════════════════════════════════════
//  Captive Portal — Lifecycle
// ═══════════════════════════════════════════════════════

void wifiStartPortal() {
    portalDone = false;

    // ── Scan networks (STA mode) ─────────────────
    LOG_I("Scanning WiFi networks...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    LOG_I("Scan found %d networks", n);

    // Build <option> list, de-duplicated by SSID
    scannedHTML = "<option value='' disabled selected>Choose a network...</option>";
    String seen;
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;              // skip hidden
        if (seen.indexOf("|" + ssid + "|") >= 0) continue;  // dedup
        seen += "|" + ssid + "|";
        scannedHTML += "<option value=\"" + ssid + "\">" + ssid + "</option>";
    }
    scannedHTML += "<option value='__manual__'>Enter manually...</option>";
    WiFi.scanDelete();

    // ── Start AP ──────────────────────────────────
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(WIFI_AP_SSID);
    delay(100);
    LOG_I("AP started: %s  IP: %s", WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());

    // ── DNS: redirect all domains to our IP ──────
    dnsServer = new DNSServer();
    dnsServer->start(53, "*", WiFi.softAPIP());

    // ── HTTP server ──────────────────────────────
    webServer = new WebServer(80);
    webServer->on("/",     HTTP_GET,  handleRoot);
    webServer->on("/save", HTTP_POST, handleSave);
    webServer->onNotFound(handleNotFound);
    webServer->begin();

    LOG_I("Captive portal ready");
}

bool wifiPortalLoop() {
    if (dnsServer) dnsServer->processNextRequest();
    if (webServer) webServer->handleClient();
    return portalDone;
}

void wifiStopPortal() {
    if (webServer) { webServer->stop();  delete webServer;  webServer = nullptr; }
    if (dnsServer) { dnsServer->stop();  delete dnsServer;  dnsServer = nullptr; }

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    LOG_I("Captive portal stopped");
}

// ═══════════════════════════════════════════════════════
//  Runtime — Reconnect
// ═══════════════════════════════════════════════════════

void wifiReconnect() {
    unsigned long now = millis();
    if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
    lastReconnectAttempt = now;

    if (WiFi.status() != WL_CONNECTED) {
        loadCacheIfNeeded();
        if (cachedSSID.length() > 0) {
            LOG_W("Reconnecting WiFi to %s...", cachedSSID.c_str());
            WiFi.disconnect();
            WiFi.begin(cachedSSID.c_str(), cachedPass.c_str());
        }
    }
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}
