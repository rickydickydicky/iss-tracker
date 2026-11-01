# Backlog

Ordered by value / risk. Top item is the recommended next step.

1. **Validate TLS certificates on every HTTPS request.** All calls use
   `HTTPClient::begin(url)` with no CA, which in arduino-esp32 means
   `setInsecure()`: anyone on the network path can feed fake ISS data, TLEs,
   crew text or JPEGs to the decoder. Switch to `begin(NetworkClientSecure&, url)`
   with the IDF certificate bundle (or pinned roots for the few fixed hosts),
   make sure NTP has synced before the first fetch (cert dates), and decide how
   to treat avatar CDN redirects to arbitrary hosts. Open Notify is plain HTTP
   only — consider dropping it.
2. **Fix the network-task data races.** `shadowCrew` / `shadowPosition` are
   written by core 0 without `stateMutex` while core 1 copies them (Arduino
   `String`s → possible heap corruption), and on a crew refresh the avatar
   PSRAM buffers are freed while `appState.crew` still points at them
   (use-after-free until the next merge). Copy under the mutex and hand
   avatar ownership over explicitly. (Pass predictions already do this.)
3. **Escape the captive portal HTML.** Scanned SSIDs and the echoed SSID on
   the success page are inserted unescaped, so a nearby AP named with markup
   can inject script into the setup page. Also consider a WPA2 password or a
   timeout on the `ISS-Tracker` AP.
4. **Persist the last good TLE in NVS** so passes show immediately after a
   reboot or while offline (still subject to the 7-day age limit).
5. **Live pointing during a pass**: compute az/el once a second on core 1
   (one SGP4 call, ~0.2 ms) and show "look here" on the banner or a small
   sky-plot; a pass list (tap the Stats strip) for the next 48 h.
6. **Future ground track on the map** from SGP4 (next orbit), instead of
   only the 15-minute history.
7. **Edit home location on the device** (portal field or Setup screen).
   Note `map_screen` draws the home marker from `HOME_LAT/HOME_LON`, not the
   NVS settings the predictor uses — unify them when this lands.
8. **CI**: GitHub Action running `pio test -e native` and `pio run`.
9. Nice-to-haves: brightness (magnitude) estimate per pass, refraction
   correction near the horizon, a "pass-only" wake mode that ignores the
   rest of the night.
