#pragma once
#include "../display_config.h"
#include "../iss_data.h"

/// Render the world map screen with ISS position and ground track.
/// @param fullRedraw  true on first call or after tab switch — redraws ocean, grid, coastlines.
///                    false on incremental updates — only redraws dynamic elements.
void renderMapScreen(LGFX &lcd, const AppState &state, bool fullRedraw);
