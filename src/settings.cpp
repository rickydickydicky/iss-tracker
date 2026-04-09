#include "settings.h"
#include "config.h"
#include <Preferences.h>

static const char* NVS_NS = "settings";

void settingsLoad(UserSettings &s) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);  // read-only

    s.sleepEnabled    = prefs.getBool("sleepOn",    DEFAULT_SLEEP_ENABLED);
    s.sleepStartHour  = prefs.getUChar("sleepStart", DEFAULT_SLEEP_START);
    s.sleepEndHour    = prefs.getUChar("sleepEnd",   DEFAULT_SLEEP_END);
    s.passNotifyEnabled = prefs.getBool("passNotify", false);
    s.homeLat         = prefs.getFloat("homeLat",    HOME_LAT);
    s.homeLon         = prefs.getFloat("homeLon",    HOME_LON);

    prefs.end();

    LOG_I("Settings loaded: sleep=%s %02d:00-%02d:00, pass=%s, home=(%.2f,%.2f)",
          s.sleepEnabled ? "ON" : "OFF",
          s.sleepStartHour, s.sleepEndHour,
          s.passNotifyEnabled ? "ON" : "OFF",
          s.homeLat, s.homeLon);
}

void settingsSave(const UserSettings &s) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);  // read-write

    prefs.putBool("sleepOn",     s.sleepEnabled);
    prefs.putUChar("sleepStart", s.sleepStartHour);
    prefs.putUChar("sleepEnd",   s.sleepEndHour);
    prefs.putBool("passNotify",  s.passNotifyEnabled);
    prefs.putFloat("homeLat",    s.homeLat);
    prefs.putFloat("homeLon",    s.homeLon);

    prefs.end();

    LOG_I("Settings saved");
}
