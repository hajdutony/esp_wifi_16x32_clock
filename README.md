# esp_wifi_16x32_clock

WiFi-connected NTP clock driving eight cascaded MAX7219 8×8 LED modules (16×32 pixels) via an ESP8266.

## Features

- **WiFi setup from a phone** — no credentials in the code; if the clock can't join a known network it opens a `wifi-clock-setup` network for setup
- **Settings page** at <http://wifi-clock.local> — brightness, night hours, time zone and date display, saved in flash
- **NTP time sync** via `europe.pool.ntp.org` (hourly, in the background)
- **Automatic summer/winter time** using the ESP8266's built-in POSIX time zone support (default: Central Europe, CET/CEST)
- **Day/night brightness** — default 30% of the lowest hardware level by day, 10% at night
- **Automatic night mode** — night runs from civil dusk to dawn (sun 6° below the horizon), calculated on the clock for your location and following summer/winter time; or use fixed hours
- **Blinking colon** between hours and minutes (once per second)
- **Date** shown as `DD.MM` for 3 seconds every minute (from second 30)
- **Status dot** (bottom-right pixel) — blinking: waiting for the first time sync; steady: no sync for over 3 hours
- **WiFi recovery** — reconnects in the background; restarts itself if it never gets the time within 10 minutes of power-up
- **Wireless (OTA) updates** — upload new firmware over WiFi to `wifi-clock.local`

## Hardware

- **ESP8266** (NodeMCU / WeMos D1 Mini)
- **8× MAX7219** 8×8 LED dot-matrix modules, daisy-chained (4 wide × 2 high)
- **GPIO 15 (D8)** — SPI chip select

## First-time WiFi setup

1. Power the clock. If it has no saved network (or can't reach it), it opens a WiFi network called **`wifi-clock-setup`** for 5 minutes.
2. Connect to it from your phone; a setup page opens (or go to <http://192.168.4.1>).
3. Pick your WiFi network, enter the password and save. The clock joins it and remembers it.

To move the clock to another network later, use **Change WiFi network** on the settings page.

## Settings page

Open <http://wifi-clock.local> (or the clock's IP address) from any device on the same WiFi.

| Setting | Default | Notes |
|---|---|---|
| Day brightness | 3 | 0–10 = 0–100% of the lowest hardware brightness |
| Night brightness | 1 | 0 turns the display off at night |
| Automatic night (dusk to dawn) | on | Civil twilight for the location below; the page shows today's dawn and dusk |
| Latitude / longitude | 48.7164 / 21.2611 | Košice; any point within ~20 km is accurate enough |
| Fixed night starts / ends at hour | 22 / 7 | Used when automatic is off (or in polar regions on days the sun doesn't reach dusk/dawn); same hour for both = no night mode |
| Show date at second / for seconds | 30 / 3 | 0 seconds = never show the date |
| Time zone | `CET-1CEST,M3.5.0,M10.5.0/3` | POSIX TZ string; the field suggests common zones |

Common time zones:

| Region     | Time zone                         |
|------------|-----------------------------------|
| UTC        | `UTC0`                            |
| Central EU | `CET-1CEST,M3.5.0,M10.5.0/3`      |
| UK         | `GMT0BST,M3.5.0/1,M10.5.0`        |
| Eastern EU | `EET-2EEST,M3.5.0/3,M10.5.0/4`    |
| US Eastern | `EST5EDT,M3.2.0,M11.1.0`          |
| US Pacific | `PST8PDT,M3.2.0,M11.1.0`          |
| Japan      | `JST-9`                           |
| Sydney     | `AEST-10AEDT,M10.1.0,M4.1.0/3`    |

Defaults for a fresh clock are the `DEFAULT_*` values at the top of `esp_wifi_16x32_clock.ino`.

## Building and uploading

Requires the ESP8266 Arduino core and the **WiFiManager** library (tzapu):

```sh
arduino-cli core install esp8266:esp8266
arduino-cli lib install WiFiManager
```

Compile (from the repository folder):

```sh
arduino-cli compile --fqbn esp8266:esp8266:d1_mini .
```

First upload over USB:

```sh
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp8266:esp8266:d1_mini:baud=115200 .
```

Later updates over WiFi:

```sh
arduino-cli upload -p wifi-clock.local --protocol network --upload-field password= \
  --fqbn esp8266:esp8266:d1_mini .
```

## Files

| File | Description |
|------|-------------|
| `esp_wifi_16x32_clock.ino` | The sketch — display, WiFi setup, NTP/time zone, settings page, OTA |
| `sun.h`, `sun.cpp` | Civil dawn/dusk calculation (Almanac for Computers algorithm, ~1 min accuracy) |
