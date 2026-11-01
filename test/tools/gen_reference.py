#!/usr/bin/env python3
"""Generate host-test reference data for lib/orbit from trusted libraries.

  * SGP4 state vectors from python-sgp4 (Vallado's reference implementation)
  * ISS passes (rise / culminate / set, visibility) from Skyfield

Usage (from the repo root):
    pip install sgp4 skyfield skyfield-data
    python3 test/tools/gen_reference.py > test/test_orbit/reference_data.h

skyfield-data bundles the de421 ephemeris so no network access is needed.
The output is committed so the native tests run without Python.
"""

import math
import os
import sys

import sgp4
import sgp4.propagation
from sgp4.api import Satrec, WGS72
from skyfield.api import EarthSatellite, load, wgs84

try:
    from skyfield_data import get_skyfield_data_path
    LOADER = load.__class__(get_skyfield_data_path(), expire=False)
except ImportError:  # fall back to downloading into ./skyfield-cache
    LOADER = load.__class__('skyfield-cache')

MIN_EL = 10.0
SUN_MAX_ALT = -6.0
SEARCH_DAYS = 3.0

# Real ISS element sets taken from the Skyfield / python-sgp4 test suites,
# plus one synthetic set (2019 elements re-stamped with a 2026 epoch, new
# checksums) so the tests also exercise current-day dates.
ISS_TLES = [
    ("ISS 2018-07-03 (skyfield tests)",
     "1 25544U 98067A   18184.80969102  .00001614  00000-0  31745-4 0  9993",
     "2 25544  51.6414 295.8524 0003435 262.6267 204.2868 15.54005638121106"),
    ("ISS 2019-12-09 (python-sgp4 README)",
     "1 25544U 98067A   19343.69339541  .00001764  00000-0  38792-4 0  9991",
     "2 25544  51.6439 211.2001 0007417  17.6667  85.6398 15.50103472202482"),
]

OBSERVERS = [
    ("Melbourne", -37.8136, 144.9631, 0.0),
    ("London", 51.5072, -0.1276, 0.0),
]


def checksum(line):
    s = 0
    for c in line[:68]:
        if c.isdigit():
            s += int(c)
        elif c == '-':
            s += 1
    return s % 10


def with_epoch(line1, epoch_field):
    l1 = line1[:18] + epoch_field + line1[32:68]
    return l1 + str(checksum(l1))


def synthetic_2026():
    _, l1, l2 = ISS_TLES[1]
    l1 = with_epoch(l1, "26305.50000000")  # 2026-11-01 12:00 UTC
    return ("ISS synthetic 2026-11-01 epoch", l1, l2)


def near_earth_vectors():
    """(name, line1, line2, [(tsince, r, v)]) for near-earth SGP4-VER sats."""
    path = os.path.join(os.path.dirname(sgp4.__file__), 'SGP4-VER.TLE')
    lines = [l.rstrip('\n') for l in open(path)]
    wanted = {'00005', '06251', '28057', '28350', '29238', '88888'}
    out = []
    for i in range(len(lines) - 1):
        a, b = lines[i], lines[i + 1]
        if a.startswith('1 ') and b.startswith('2 ') and a[2:7] in wanted:
            l1, l2 = a[:69], b[:69]
            sat = Satrec.twoline2rv(l1, l2, WGS72)
            rows = []
            for tsince in (0.0, 120.0, 360.0, 720.0, 1440.0, 2880.0, -720.0):
                e, r, v = sat.sgp4_tsince(tsince)
                if e == 0:
                    rows.append((tsince, r, v))
            out.append((a[2:7], l1, l2, rows))
    return out


def deep_space_tle():
    path = os.path.join(os.path.dirname(sgp4.__file__), 'SGP4-VER.TLE')
    lines = [l.rstrip('\n') for l in open(path)]
    for i in range(len(lines) - 1):
        if lines[i].startswith('1 04632'):
            return lines[i][:69], lines[i + 1][:69]
    raise RuntimeError('04632 not found')


def passes_for(ts, eph, name, l1, l2, obs):
    sat = EarthSatellite(l1, l2, name, ts)
    oname, lat, lon, alt = obs
    topos = wgs84.latlon(lat, lon, elevation_m=alt * 1000.0)
    t0 = sat.epoch
    t1 = ts.tt_jd(t0.tt + SEARCH_DAYS)
    times, events = sat.find_events(topos, t0, t1, altitude_degrees=MIN_EL)
    earth, sun = eph['earth'], eph['sun']
    diff = sat - topos
    result = []
    i = 0
    # Skip a leading partial pass (we start the C++ search at the same t0)
    while i < len(events) and events[i] != 0:
        i += 1
    while i + 2 < len(events):
        if not (events[i] == 0 and events[i + 1] == 1 and events[i + 2] == 2):
            i += 1
            continue
        tr, tc, tset = times[i], times[i + 1], times[i + 2]
        alt_c, az_c, _ = diff.at(tc).altaz()
        _, az_r, _ = diff.at(tr).altaz()
        _, az_s, _ = diff.at(tset).altaz()
        # Visibility sampled every second
        n = int(round((tset.tt - tr.tt) * 86400.0)) + 1
        tt = ts.tt_jd([tr.tt + k / 86400.0 for k in range(n)])
        sunlit = sat.at(tt).is_sunlit(eph)
        sun_alt, _, _ = (earth + topos).at(tt).observe(sun).apparent().altaz()
        vis = [bool(s) and a < SUN_MAX_ALT for s, a in zip(sunlit, sun_alt.degrees)]
        if any(vis):
            first = vis.index(True)
            last = len(vis) - 1 - vis[::-1].index(True)
            vstart = tt[first].utc_datetime().timestamp()
            vend = tt[last].utc_datetime().timestamp()
        else:
            vstart = vend = 0.0
        result.append(dict(
            aos=tr.utc_datetime().timestamp(), tca=tc.utc_datetime().timestamp(),
            los=tset.utc_datetime().timestamp(), max_el=alt_c.degrees,
            aos_az=az_r.degrees, tca_az=az_c.degrees, los_az=az_s.degrees,
            visible=any(vis), vstart=vstart, vend=vend))
        i += 3
    return t0.utc_datetime().timestamp(), result


def main():
    ts = LOADER.timescale()
    eph = LOADER('de421.bsp')
    w = sys.stdout.write
    w('// AUTO-GENERATED by test/tools/gen_reference.py - do not edit.\n')
    w('// skyfield %s, sgp4 %s\n' % (__import__('skyfield').__version__, sgp4.__version__))
    w('#pragma once\n#include <stdint.h>\n\n')
    w('namespace ref {\n\n')
    w('const double kMinEl = %.1f;\nconst double kSunMaxAlt = %.1f;\n\n' % (MIN_EL, SUN_MAX_ALT))

    # --- propagation vectors ---
    w('struct Vec { double tsince, r[3], v[3]; };\n')
    w('struct PropCase { const char* satnum; const char* l1; const char* l2; int n; Vec rows[8]; };\n')
    cases = near_earth_vectors()
    w('const PropCase kPropCases[] = {\n')
    for satnum, l1, l2, rows in cases:
        w('  { "%s", "%s", "%s", %d, {\n' % (satnum, l1, l2, len(rows)))
        for tsince, r, v in rows:
            w('    { %.1f, {%.9f, %.9f, %.9f}, {%.12f, %.12f, %.12f} },\n'
              % (tsince, r[0], r[1], r[2], v[0], v[1], v[2]))
        w('  } },\n')
    w('};\nconst int kPropCaseCount = %d;\n\n' % len(cases))

    d1, d2 = deep_space_tle()
    w('const char* const kDeepSpaceL1 = "%s";\nconst char* const kDeepSpaceL2 = "%s";\n\n' % (d1, d2))

    # --- gmst samples ---
    w('struct GmstCase { double jd, gmst; };\nconst GmstCase kGmst[] = {\n')
    for jd in (2451545.0, 2458827.19339541, 2461345.5, 2461346.123456):
        w('  { %.8f, %.15f },\n' % (jd, sgp4.propagation.gstime(jd)))
    w('};\n\n')

    # --- passes ---
    w('struct RefPass { double aos, tca, los, maxEl, aosAz, tcaAz, losAz; bool visible; double visStart, visEnd; };\n')
    w('struct PassCase { const char* name; const char* l1; const char* l2;\n'
      '                  const char* observer; double lat, lon, altKm; double start, end;\n'
      '                  int n; RefPass passes[40]; };\n')
    w('const PassCase kPassCases[] = {\n')
    total = 0
    for name, l1, l2 in ISS_TLES + [synthetic_2026()]:
        for obs in OBSERVERS:
            start, passes = passes_for(ts, eph, name, l1, l2, obs)
            assert len(passes) <= 40
            total += 1
            w('  { "%s", "%s", "%s", "%s", %.4f, %.4f, %.3f, %.3f, %.3f, %d, {\n'
              % (name, l1, l2, obs[0], obs[1], obs[2], obs[3], start,
                 start + SEARCH_DAYS * 86400.0, len(passes)))
            for p in passes:
                w('    { %.3f, %.3f, %.3f, %.4f, %.3f, %.3f, %.3f, %s, %.1f, %.1f },\n'
                  % (p['aos'], p['tca'], p['los'], p['max_el'], p['aos_az'],
                     p['tca_az'], p['los_az'], 'true' if p['visible'] else 'false',
                     p['vstart'], p['vend']))
            w('  } },\n')
    w('};\nconst int kPassCaseCount = %d;\n\n' % total)
    w('}  // namespace ref\n')


if __name__ == '__main__':
    main()
