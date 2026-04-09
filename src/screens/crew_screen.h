#pragma once
#include "../display_config.h"
#include "../iss_data.h"

/// Render the crew in space screen.
void renderCrewScreen(LGFX &lcd, const AppState &state, bool fullRedraw);

/// Handle a touch event on the crew screen content area.
/// Returns true if the touch was consumed (opened/closed modal).
bool handleCrewTouch(LGFX &lcd, const AppState &state, int16_t x, int16_t y);
