#pragma once
// ─── Pass prediction ────────────────────────────────────
// Finds the times a satellite rises above / sets below a minimum elevation
// for a ground observer, the culmination (max elevation), and whether the
// pass is naked-eye visible (satellite sunlit while the observer's sky is
// dark).

#include <stdint.h>

#include "orbit_geo.h"

namespace orbit {

struct PassOptions {
    double minElevationDeg = 10.0;  // rise/set threshold
    double maxSunAltDeg    = -6.0;  // sky "dark enough" (civil twilight)
    // Search tuning. The coarse step is adaptive: far below the threshold we
    // jump further. kSecPerDeg must stay below 1 / (max elevation rate near
    // the horizon) — ~0.35 deg/s for the ISS — or short passes can be missed.
    double secPerDeg       = 2.5;
    double minStepSec      = 5.0;
    double maxStepSec      = 300.0;
    double refineSec       = 0.25;  // bisection tolerance for AOS/LOS
    double visSampleSec    = 5.0;   // sampling for the visibility window
};

struct Pass {
    int64_t aos;           // unix time the satellite rises above minElevation
    int64_t tca;           // unix time of maximum elevation
    int64_t los;           // unix time it sets below minElevation
    float   maxElDeg;
    float   aosAzDeg, tcaAzDeg, losAzDeg;
    bool    aosClipped;    // pass already in progress at search start
    bool    visible;       // naked-eye visible for at least part of the pass
    int64_t visStart, visEnd;          // visible window (if visible)
    float   visStartAzDeg, visStartElDeg;
    float   visEndAzDeg,   visEndElDeg;
};

// Satellite look angles from the observer at Julian Date `jd`.
// Optionally returns the TEME position. Returns the SGP4 error code.
Sgp4Error satLookAngles(const Sgp4& sat, const Observer& obs, double jd,
                        LookAngles& out, double rTeme[3] = nullptr);

// Called periodically during long searches so firmware can yield to the
// RTOS (feed watchdogs). May be null.
typedef void (*YieldFn)(void* ctx);

// Predict passes starting in [startUnix, endUnix). A pass that is in progress
// at startUnix is reported with aosClipped = true and aos = startUnix. A pass
// that rises before endUnix is followed to its LOS even past endUnix.
// Returns the number of passes written to `out` (<= maxOut), or -1 if SGP4
// failed (e.g. stale/invalid elements); passes found before the failure are
// still in `out` in that case, and *errOut (if given) holds the error.
int predictPasses(const Sgp4& sat, const Observer& obs,
                  int64_t startUnix, int64_t endUnix,
                  const PassOptions& opt, Pass* out, int maxOut,
                  YieldFn yieldFn = nullptr, void* yieldCtx = nullptr,
                  Sgp4Error* errOut = nullptr);

}  // namespace orbit
