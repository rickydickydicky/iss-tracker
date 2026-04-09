#pragma once
#include "../iss_data.h"

/// Initialize the display hardware (LovyanGFX + GT911 touch).
void screenManagerInit();

/// Show the branded landing page with ISS line art and title.
void screenShowLanding();

/// Show a one- or two-line status message (used during boot/setup).
/// line2 can be nullptr for single-line display.
void screenShowStatus(const char* line1, const char* line2 = nullptr);

/// Show a simple centered text splash (legacy, used for quick messages).
void screenShowSplash(const char* message);

/// Check if the user is touching the screen right now.
/// Returns true if a touch is detected. Non-blocking.
bool screenCheckTouch();

/// Read touch input from the tab bar and update active screen in appState.
/// Includes debounce logic. Returns true if a touch was processed.
bool screenManagerHandleTouch(AppState &state);

/// Render the currently active screen with tab bar.
void screenManagerRender(const AppState &state);

/// Control the display backlight (0 = off, >0 = on).
void screenSetBacklight(bool on);

/// Force a full redraw on the next render cycle.
/// Call after waking from sleep to refresh the display.
void screenForceRedraw();

/// Hard-reset the GT911 touch controller via CH422G I/O expander.
/// Call when waking from display sleep to ensure reliable touch input.
void screenResetTouch();
