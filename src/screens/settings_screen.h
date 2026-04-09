#pragma once
#include "../display_config.h"
#include "../iss_data.h"

void renderSettingsScreen(LGFX &lcd, const AppState &state, bool fullRedraw);

// Returns true if touch was consumed (setting changed → needs redraw + save)
bool handleSettingsTouch(LGFX &lcd, AppState &state, int16_t x, int16_t y);
