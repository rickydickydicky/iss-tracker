#pragma once
#include <WiFi.h>
#include "config.h"

/// Check if WiFi credentials are saved in NVS.
bool wifiHasSavedCredentials();

/// Get the saved SSID (for display during connection).
String wifiGetSavedSSID();

/// Connect using credentials saved in NVS.
/// Blocks up to WIFI_TIMEOUT_MS, then returns.
bool wifiConnectSaved();

/// Clear all saved WiFi credentials from NVS.
void wifiClearCredentials();

/// Start the captive portal: AP + DNS redirect + web config page.
/// After calling this, loop on wifiPortalLoop() until it returns true.
void wifiStartPortal();

/// Process portal DNS/HTTP requests. Call in a tight loop.
/// Returns true when the user has submitted valid credentials
/// and the ESP32 has connected to their network.
bool wifiPortalLoop();

/// Stop the captive portal and switch to STA-only mode.
void wifiStopPortal();

/// Non-blocking reconnect attempt using saved credentials.
/// Call periodically from the network task.
void wifiReconnect();

/// Returns true if WiFi STA is currently connected.
bool wifiIsConnected();
