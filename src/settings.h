#pragma once
#include "iss_data.h"

// Load settings from NVS into the provided struct.
// If no saved settings exist, fills with defaults from config.h.
void settingsLoad(UserSettings &s);

// Save settings to NVS. Only writes keys that changed.
void settingsSave(const UserSettings &s);
