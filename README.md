# ISS Tracker

Real-time International Space Station tracker running on a Waveshare ESP32-S3-Touch-LCD-4.3. Displays the station's position on a world map, crew information with photos, orbital stats, upcoming ISS passes over your home (with optional alerts for naked-eye passes), and configurable display sleep — all on a 4.3" touchscreen.

## Hardware

**Required:** [Waveshare ESP32-S3-Touch-LCD-4.3](https://www.waveshare.com/esp32-s3-touch-lcd-4.3.htm)

- ESP32-S3 dual-core, 16MB QIO flash, 8MB Octal PSRAM
- 800x480 RGB565 parallel LCD (DMA-driven, always scanning)
- GT911 capacitive touch (I2C, address 0x5D)
- CH422G I/O expander for backlight and touch reset (I2C, shared bus)
- USB-C for power and programming

This project is built specifically for this board. The display driver, touch configuration, CH422G I/O expander code, and pin mappings are all tailored to this hardware. It won't work on other ESP32 boards without significant modification to `src/display_config.h`.

## What It Does

**Map** — World map with gold continent outlines, real-time ISS position (pulsing cyan dot), ground track trail showing the last ~15 minutes of orbital path, and a coordinate/altitude overlay. Your home location is marked with a red diamond.

**Stats** — Orbital speed, altitude, visibility status (daylight/eclipsed/penumbra), and current coordinates in a dashboard layout. Underneath, the **next visible pass** over your home location: when and where it appears, where it disappears, its maximum elevation, and how long it is visible.

**Crew** — Adaptive grid of everyone currently in space, grouped by spacecraft (ISS, Tiangong, etc.). Tap any crew member to see their photo, biography, nationality, and agency. Avatars load progressively — initials appear first, then real photos download in the background.

**Setup** — Display sleep toggle with configurable hours (auto-off at night, touch to wake), ISS pass alerts (shows the next visible pass), home location display, NTP sync status, and local time.

**Pass alerts** — With *ISS Pass Alerts* on, a gold banner appears across the top of every screen 5 minutes before a naked-eye visible pass ("ISS IN 4 MIN: look NW, max 54 deg, at 19:42") and stays up until the ISS fades. If the display is asleep for the night it wakes for the pass and goes back to sleep afterwards.

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/install) (CLI or IDE plugin)
- USB-C cable (data-capable, not charge-only)
- 2.4GHz WiFi network

### Clone and Build

```bash
git clone <repo-url>
cd iss-tracker
```

**Set your upload port** in `platformio.ini`. The default is `/dev/cu.usbmodem1101` (macOS). On Linux it's typically `/dev/ttyACM0`, on Windows `COM3` or similar. Find yours with:

```bash
# macOS/Linux
ls /dev/cu.usb* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null

# Windows (PowerShell)
Get-WmiObject Win32_SerialPort | Select DeviceID, Description
```

Update the `upload_port` line in `platformio.ini`, then:

```bash
pio run              # Build
pio run -t upload    # Flash to device
pio device monitor   # Serial output (115200 baud)
```

The first build takes a few minutes — PlatformIO downloads the ESP32 toolchain, Arduino framework, and libraries automatically.

### WiFi Setup

On first boot (or after a factory reset), the device shows a landing page with an ISS wireframe. **Touch the screen within 3 seconds** to enter WiFi setup mode. This launches a captive portal:

1. Connect your phone/laptop to the `ISS-Tracker` WiFi network
2. A configuration page opens automatically (or navigate to `192.168.4.1`)
3. Enter your 2.4GHz WiFi credentials
4. The device connects, syncs time via NTP, and starts tracking

Credentials are saved in NVS (non-volatile storage) and persist across reboots. To reconfigure WiFi later, touch the screen during the 3-second boot window.

### Configuration

Edit `src/config.h` before building:

```c
// Home location — used for the map marker and pass predictions
#define HOME_LAT  -37.8136   // Your latitude
#define HOME_LON  144.9631   // Your longitude
#define HOME_ALT_M  0.0      // Height above sea level (metres, optional)

// Timezone — POSIX TZ string for your location
// See: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE_POSIX  "AEST-10AEDT,M10.1.0,M4.1.0/3"
```

API poll intervals, display sleep defaults, and pin mappings are also in `config.h` but shouldn't need changing for normal use. Pass prediction tunables live there too: minimum elevation (`PASS_MIN_ELEVATION_DEG`, 10°), how dark the sky must be (`PASS_SUN_MAX_ALT_DEG`, −6° = end of civil twilight), look-ahead (`PASS_PREDICT_HOURS`, 48 h) and alert lead time (`PASS_ALERT_LEAD_S`, 5 min).

## Architecture

### Dual-Core Design

The ESP32-S3 has two cores, and this project uses both:

- **Core 0** — Network task (FreeRTOS). Polls APIs, downloads crew enrichment data and avatar images, fetches the ISS TLE and runs pass prediction. Writes to shadow buffers protected by a mutex.
- **Core 1** — Main loop. Handles touch input, merges network data into app state, renders screens. All timing is millis()-based with no blocking delays in the render path.

### Module Layout

```
src/
  main.cpp                  Setup, loop, network task, sleep/wake state machine
  config.h                  All tunables: WiFi, APIs, timing, pins, timezone
  iss_data.h                Shared data structures (position, crew, settings, app state)
  colors.h                  RGB565 colour palette (dark theme)
  display_config.h          LovyanGFX hardware config: Bus_RGB, GT911, CH422G
  wifi_manager.cpp/h        WiFi connection, captive portal, NVS credential storage
  api_client.cpp/h          HTTP client: ISS position, TLE, crew list, SpaceDevs enrichment, avatars
  settings.cpp/h            NVS persistence for user settings
  pass_text.cpp/h           Human-readable pass/alert strings for the screens
  map_data.h                World map coastline coordinates (PROGMEM)
  portal_html.h             Captive portal HTML (embedded)
  screens/
    screen_manager.cpp/h    Tab bar, touch routing, backlight, GT911 reset
    map_screen.cpp/h        World map, ISS dot, ground track, coordinate overlay
    stats_screen.cpp/h      Orbital statistics dashboard
    crew_screen.cpp/h       Crew grid with modal detail view
    settings_screen.cpp/h   Settings UI with toggles and NVS save

lib/orbit/                  Framework-free C++ (also built for host tests)
  orbit_tle                 Bounds-checked TLE parser (checksum, ranges)
  orbit_sgp4                Near-earth SGP4 (port of Vallado's reference code)
  orbit_geo                 Sidereal time, frames, observer look angles, Sun, shadow
  orbit_pass                Pass search: rise/culmination/set + visibility window
  orbit_alert               Next-pass / alert-window logic, local time labels

test/
  test_orbit/               Unity tests (pio test -e native)
  tools/gen_reference.py    Regenerates reference data with python-sgp4 + Skyfield
```

### APIs

| Source | Data | Interval |
|--------|------|----------|
| [Where the ISS At](https://wheretheiss.at) | Position, altitude, velocity, visibility | 10s |
| [Where the ISS At](https://wheretheiss.at) `/tles` | ISS two-line elements (pass prediction) | 12h |
| [CelesTrak](https://celestrak.org) | TLE fallback | when the above fails |
| [corquaid/people-in-space](https://github.com/corquaid/international-space-station-APIs) | Crew names and spacecraft | 6h |
| [Open Notify](http://api.open-notify.org/astros.json) | Crew fallback | 6h |
| [TheSpaceDevs](https://thespacedevs.com) | Bios, nationalities, agencies, photos | Once per crew refresh |

Crew enrichment is progressive: the crew list loads first (names and spacecraft), then SpaceDevs data fills in bios and nationalities, then avatar JPEGs download one at a time into PSRAM. The screen updates as each piece arrives.

### Pass Prediction

Passes are computed on the device — no pass-prediction API is involved:

1. The network task fetches the ISS two-line element set (TLE) every 12 hours (wheretheiss.at, CelesTrak as fallback). Lines are length/format/checksum checked before use, and elements more than 7 days old are refused.
2. SGP4 (WGS-72, near-earth branch of David Vallado's reference implementation) propagates the orbit; positions are rotated into the Earth-fixed frame with Greenwich sidereal time and converted to azimuth/elevation for the home location on the WGS-84 ellipsoid.
3. An adaptive coarse search (big steps when the ISS is far below the horizon, 5 s near it) finds each rise above 10°, refined by bisection to ~0.25 s; the culmination is found by golden-section search.
4. A pass is **visible** while the ISS is in sunlight (not in Earth's shadow) and the Sun is at least 6° below your horizon.

48 hours of passes (~3,000 SGP4 evaluations) are recomputed when new elements arrive, after each predicted pass ends, and every 6 hours. All maths is double precision; the ESP32-S3 emulates doubles in software, so a run takes on the order of a second on core 0 (logged to serial as `[Pass] ... computed in N ms`) and yields to the RTOS while it works. No refraction is modelled, so real rise/set times can be a few seconds earlier/later near the horizon.

### Display Sleep

When enabled via the Setup tab, the backlight turns off during configured night hours. Touch the screen to wake — the GT911 touch controller gets a full hardware reset via CH422G to ensure reliable input after prolonged sleep. The display stays on while you're interacting and returns to sleep after 30 seconds of inactivity (if still within the sleep window).

Sleep is **off by default**. Toggle it on in Setup and choose from preset schedules (10PM-7AM, 11PM-6AM, 9PM-7AM, or 12AM-6AM).

If **ISS Pass Alerts** is also on, a visible pass inside the sleep window wakes the display from 5 minutes before the pass until it ends; it then returns to sleep.

## Testing

The orbit maths in `lib/orbit/` has no Arduino dependencies and is unit-tested on your computer (no board needed):

```bash
pio test -e native
```

The tests check the TLE parser against malformed input, the SGP4 port against python-sgp4 state vectors (to 1 cm), and pass predictions against [Skyfield](https://rhodesmill.org/skyfield/) for ~80 ISS passes over Melbourne and London (rise/set within 3 s, max elevation within 0.1°, visibility window within 5 s). Reference data is committed in `test/test_orbit/reference_data.h`; regenerate it with:

```bash
pip install sgp4 skyfield skyfield-data
python3 test/tools/gen_reference.py > test/test_orbit/reference_data.h
```

`pio run` builds only the firmware (`default_envs = esp32s3`).

### Build System Notes

This project uses [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino ESP32 core 3.x with ESP-IDF 5.4) rather than the stock PlatformIO ESP32 platform. The `platform` URL in `platformio.ini` pulls a specific release.

The `extra_script.py` pre-build script generates wrapper headers in `include/` so that LovyanGFX's `Bus_RGB.hpp` can find ESP-IDF LCD panel headers via `__has_include()`. These wrappers redirect to the real SDK headers by absolute path. You'll see them generated on first build — they're gitignored and regenerated automatically.

### Memory

- **Flash (16MB):** Partitioned with `default_16MB.csv`. Firmware + map data fits comfortably with room for OTA (~1.3 MB used; pass prediction adds ~45 KB).
- **RAM:** Pass predictions use ~1 KB per copy (12 passes); the propagator state is ~260 bytes.
- **PSRAM (8MB):** Used for the RGB framebuffer (LovyanGFX allocates this automatically) and crew avatar JPEG buffers (~5-15KB each, allocated with `ps_malloc()`).

## Easter Eggs

There may or may not be something worth looking at on December 25.

## License

This is a personal project built as a gift. Do whatever you want with it.
