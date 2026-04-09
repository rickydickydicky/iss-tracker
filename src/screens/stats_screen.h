#pragma once
#include "../display_config.h"
#include "../iss_data.h"

/// Render the orbital statistics screen.
void renderStatsScreen(LGFX &lcd, const AppState &state, bool fullRedraw);
