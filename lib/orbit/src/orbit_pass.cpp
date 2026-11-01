#include "orbit_pass.h"

#include <math.h>

namespace orbit {

Sgp4Error satLookAngles(const Sgp4& sat, const Observer& obs, double jd,
                        LookAngles& out, double rTeme[3]) {
    double r[3], v[3];
    Sgp4Error err = sgp4PropagateJd(sat, jd, r, v);
    if (err != SGP4_OK) return err;
    double ecef[3];
    temeToEcef(r, jd, ecef);
    lookAnglesEcef(obs, ecef, out);
    if (rTeme) { rTeme[0] = r[0]; rTeme[1] = r[1]; rTeme[2] = r[2]; }
    return SGP4_OK;
}

namespace {

// Longest pass we are prepared to follow (LEO passes last < 15 min).
const double kMaxPassSec = 30.0 * 60.0;

struct Search {
    const Sgp4& sat;
    const Observer& obs;
    const PassOptions& opt;
    YieldFn yieldFn;
    void* yieldCtx;
    int evals;
    Sgp4Error err;

    // Elevation at unix time t. Returns false on SGP4 failure.
    bool look(double t, LookAngles& la, double rTeme[3] = nullptr) {
        if (++evals % 64 == 0 && yieldFn) yieldFn(yieldCtx);
        Sgp4Error e = satLookAngles(sat, obs, unixToJd(t), la, rTeme);
        if (e != SGP4_OK) { err = e; return false; }
        return true;
    }

    bool el(double t, double& out) {
        LookAngles la;
        if (!look(t, la)) return false;
        out = la.elDeg;
        return true;
    }

    // Visible = satellite sunlit and observer's sky dark. Returns false on error.
    bool visibleAt(double t, bool& vis) {
        LookAngles la;
        double r[3];
        if (!look(t, la, r)) return false;
        const double jd = unixToJd(t);
        double sun[3];
        sunPositionEci(jd, sun);
        vis = la.elDeg >= opt.minElevationDeg &&
              isSunlit(r, sun) &&
              sunElevationDeg(obs, jd) < opt.maxSunAltDeg;
        return true;
    }

    // Bisect the elevation threshold crossing between ta and tb, where
    // aboveA says whether ta is above the threshold (tb is the opposite).
    bool crossing(double ta, double tb, bool aboveA, double& tOut) {
        while (tb - ta > opt.refineSec) {
            const double tm = 0.5 * (ta + tb);
            double e;
            if (!el(tm, e)) return false;
            if ((e >= opt.minElevationDeg) == aboveA) ta = tm; else tb = tm;
        }
        tOut = 0.5 * (ta + tb);
        return true;
    }

    // Bisect a visibility transition between ta (state visA) and tb.
    bool visEdge(double ta, double tb, bool visA, double& tOut) {
        while (tb - ta > 0.5) {
            const double tm = 0.5 * (ta + tb);
            bool v;
            if (!visibleAt(tm, v)) return false;
            if (v == visA) ta = tm; else tb = tm;
        }
        tOut = 0.5 * (ta + tb);
        return true;
    }

    // Golden-section search for the elevation maximum on [a, b].
    bool maxElevation(double a, double b, double& tMax, double& elMax) {
        const double gr = 0.6180339887498949;
        double c = b - gr * (b - a);
        double d = a + gr * (b - a);
        double fc, fd;
        if (!el(c, fc) || !el(d, fd)) return false;
        while (b - a > 0.5) {
            if (fc > fd) {
                b = d; d = c; fd = fc;
                c = b - gr * (b - a);
                if (!el(c, fc)) return false;
            } else {
                a = c; c = d; fc = fd;
                d = a + gr * (b - a);
                if (!el(d, fd)) return false;
            }
        }
        tMax = 0.5 * (a + b);
        return el(tMax, elMax);
    }

    double stepFor(double degFromThreshold) const {
        double s = fabs(degFromThreshold) * opt.secPerDeg;
        if (s < opt.minStepSec) s = opt.minStepSec;
        if (s > opt.maxStepSec) s = opt.maxStepSec;
        return s;
    }
};

inline float az(const LookAngles& la) { return (float)la.azDeg; }

}  // namespace

int predictPasses(const Sgp4& sat, const Observer& obs,
                  int64_t startUnix, int64_t endUnix,
                  const PassOptions& opt, Pass* out, int maxOut,
                  YieldFn yieldFn, void* yieldCtx, Sgp4Error* errOut) {
    Search s{sat, obs, opt, yieldFn, yieldCtx, 0, SGP4_OK};
    const double thr = opt.minElevationDeg;
    const double end = (double)endUnix;
    int count = 0;

    auto fail = [&]() -> int {
        if (errOut) *errOut = s.err;
        return -1;
    };

    double t = (double)startUnix;
    double e;
    if (!s.el(t, e)) return fail();

    while (count < maxOut && t < end) {
        Pass p = {};
        double aos, tAbove, eAbove;

        if (e >= thr) {
            // Already above the threshold (only possible at the search start)
            aos = t;
            tAbove = t;
            eAbove = e;
            p.aosClipped = true;
        } else {
            // ── Coarse search for the next rise ──
            bool found = false;
            while (t < end) {
                const double tn = t + s.stepFor(thr - e);
                double en;
                if (!s.el(tn, en)) return fail();
                if (en >= thr) {
                    if (!s.crossing(t, tn, false, aos)) return fail();
                    tAbove = tn;
                    eAbove = en;
                    found = true;
                    break;
                }
                t = tn;
                e = en;
            }
            if (!found) break;
            if (aos >= end) break;
        }

        // ── Follow the pass until it sets ──
        double los = 0.0;
        for (;;) {
            const double tn = tAbove + s.stepFor(eAbove - thr);
            double en;
            if (!s.el(tn, en)) return fail();
            if (en < thr) {
                if (!s.crossing(tAbove, tn, true, los)) return fail();
                break;
            }
            tAbove = tn;
            eAbove = en;
            if (tAbove - aos > kMaxPassSec) { los = tAbove; break; }
        }

        // ── Culmination ──
        double tca, maxEl;
        if (!s.maxElevation(aos, los, tca, maxEl)) return fail();

        LookAngles la;
        if (!s.look(aos, la)) return fail();
        p.aosAzDeg = az(la);
        if (!s.look(tca, la)) return fail();
        p.tcaAzDeg = az(la);
        if (!s.look(los, la)) return fail();
        p.losAzDeg = az(la);

        p.aos = (int64_t)llround(aos);
        p.tca = (int64_t)llround(tca);
        p.los = (int64_t)llround(los);
        p.maxElDeg = (float)maxEl;

        // ── Visibility window ──
        // The ISS enters/leaves Earth's shadow at most once per pass and the
        // Sun's altitude is monotonic over a few minutes, so the visible part
        // is a single contiguous interval.
        double visStart = -1.0, visEnd = -1.0;
        bool prevVis = false;
        double prevT = aos;
        for (double ts = aos;; ts += opt.visSampleSec) {
            if (ts > los) ts = los;
            bool v;
            if (!s.visibleAt(ts, v)) return fail();
            if (v && visStart < 0.0) {
                if (ts == aos) visStart = aos;
                else if (!s.visEdge(prevT, ts, prevVis, visStart)) return fail();
            }
            if (!v && prevVis && visEnd < 0.0) {
                if (!s.visEdge(prevT, ts, prevVis, visEnd)) return fail();
            }
            prevVis = v;
            prevT = ts;
            if (ts >= los) break;
        }
        if (visStart >= 0.0) {
            if (visEnd < 0.0) visEnd = los;
            p.visible  = true;
            p.visStart = (int64_t)llround(visStart);
            p.visEnd   = (int64_t)llround(visEnd);
            if (!s.look(visStart, la)) return fail();
            p.visStartAzDeg = az(la);
            p.visStartElDeg = (float)la.elDeg;
            if (!s.look(visEnd, la)) return fail();
            p.visEndAzDeg = az(la);
            p.visEndElDeg = (float)la.elDeg;
        }

        out[count++] = p;

        // Resume just after LOS
        t = los + opt.minStepSec;
        if (!s.el(t, e)) return fail();
    }

    if (errOut) *errOut = SGP4_OK;
    return count;
}

}  // namespace orbit
