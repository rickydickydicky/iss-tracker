# ISS Tracker

Real-time International Space Station tracker running on a Waveshare ESP32-S3-Touch-LCD-4.3. Displays the station's position on a world map, crew information with photos, orbital stats, and configurable display sleep — all on a 4.3" touchscreen.

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

**Stats** — Orbital speed, altitude, visibility status (daylight/eclipsed/penumbra), and current coordinates in a dashboard layout.

**Crew** — Adaptive grid of everyone currently in space, grouped by spacecraft (ISS, Tiangong, etc.). Tap any crew member to see their photo, biography, nationality, and agency. Avatars load progressively — initials appear first, then real photos download in the background.

**Setup** — Display sleep toggle with configurable hours (auto-off at night, touch to wake), ISS pass alert placeholder, home location display, NTP sync status, and local time.

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
// Home location — used for the map marker and future pass predictions
#define HOME_LAT  -37.8136   // Your latitude
#define HOME_LON  144.9631   // Your longitude

// Timezone — POSIX TZ string for your location
// See: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE_POSIX  "AEST-10AEDT,M10.1.0,M4.1.0/3"
```

API poll intervals, display sleep defaults, and pin mappings are also in `config.h` but shouldn't need changing for normal use.

## Architecture

### Dual-Core Design

The ESP32-S3 has two cores, and this project uses both:

- **Core 0** — Network task (FreeRTOS). Polls APIs, downloads crew enrichment data and avatar images. Writes to shadow buffers protected by a mutex.
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
  api_client.cpp/h          HTTP client: ISS position, crew list, SpaceDevs enrichment, avatars
  settings.cpp/h            NVS persistence for user settings
  map_data.h                World map coastline coordinates (PROGMEM)
  portal_html.h             Captive portal HTML (embedded)
  screens/
    screen_manager.cpp/h    Tab bar, touch routing, backlight, GT911 reset
    map_screen.cpp/h        World map, ISS dot, ground track, coordinate overlay
    stats_screen.cpp/h      Orbital statistics dashboard
    crew_screen.cpp/h       Crew grid with modal detail view
    settings_screen.cpp/h   Settings UI with toggles and NVS save
```

### APIs

| Source | Data | Interval |
|--------|------|----------|
| [Where the ISS At](https://wheretheiss.at) | Position, altitude, velocity, visibility | 10s |
| [corquaid/people-in-space](https://github.com/corquaid/international-space-station-APIs) | Crew names and spacecraft | 6h |
| [Open Notify](http://api.open-notify.org/astros.json) | Crew fallback | 6h |
| [TheSpaceDevs](https://thespacedevs.com) | Bios, nationalities, agencies, photos | Once per crew refresh |

Crew enrichment is progressive: the crew list loads first (names and spacecraft), then SpaceDevs data fills in bios and nationalities, then avatar JPEGs download one at a time into PSRAM. The screen updates as each piece arrives.

### Display Sleep

When enabled via the Setup tab, the backlight turns off during configured night hours. Touch the screen to wake — the GT911 touch controller gets a full hardware reset via CH422G to ensure reliable input after prolonged sleep. The display stays on while you're interacting and returns to sleep after 30 seconds of inactivity (if still within the sleep window).

Sleep is **off by default**. Toggle it on in Setup and choose from preset schedules (10PM-7AM, 11PM-6AM, 9PM-7AM, or 12AM-6AM).

### Build System Notes

This project uses [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino ESP32 core 3.x with ESP-IDF 5.4) rather than the stock PlatformIO ESP32 platform. The `platform` URL in `platformio.ini` pulls a specific release.

The `extra_script.py` pre-build script generates wrapper headers in `include/` so that LovyanGFX's `Bus_RGB.hpp` can find ESP-IDF LCD panel headers via `__has_include()`. These wrappers redirect to the real SDK headers by absolute path. You'll see them generated on first build — they're gitignored and regenerated automatically.

### Memory

- **Flash (16MB):** Partitioned with `default_16MB.csv`. Firmware + map data fits comfortably with room for OTA.
- **PSRAM (8MB):** Used for the RGB framebuffer (LovyanGFX allocates this automatically) and crew avatar JPEG buffers (~5-15KB each, allocated with `ps_malloc()`).

## Easter Eggs

There may or may not be something worth looking at on December 25.

## License

This is a personal project built as a gift. Do whatever you want with it.
