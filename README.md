# esp_wifi_16x32_clock

WiFi-connected NTP clock driving eight cascaded MAX7219 8×8 LED modules (16×32 pixels) via an ESP8266.

## Features

- **NTP time sync** via `europe.pool.ntp.org` (hourly, in the background)
- **Automatic summer/winter time** using the ESP8266's built-in POSIX time zone support
  - Default: Europe/Bratislava — CET (UTC+1) / CEST (UTC+2), EU rule (last Sunday of March / October)
- **Day/night brightness** — 30% of the lowest hardware level by day, 10% from 22:00 to 07:00
- **Wireless (OTA) updates** — the clock is reachable as `wifi-clock.local`

## Hardware

- **ESP8266** (NodeMCU / WeMos D1 Mini)
- **8× MAX7219** 8×8 LED dot-matrix modules, daisy-chained (4 wide × 2 high)
- **GPIO 15 (D8)** — SPI chip select

## Configuration

All settings are at the top of `main.ino`.

### Time zone

```cpp
#define TIME_ZONE "CET-1CEST,M3.5.0,M10.5.0/3"
```

| Region     | TIME_ZONE                         |
|------------|-----------------------------------|
| UTC        | `UTC0`                            |
| CET (EU)   | `CET-1CEST,M3.5.0,M10.5.0/3`      |
| UK         | `GMT0BST,M3.5.0/1,M10.5.0`        |
| US Eastern | `EST5EDT,M3.2.0,M11.1.0`          |
| US Pacific | `PST8PDT,M3.2.0,M11.1.0`          |
| Japan      | `JST-9`                           |
| Sydney     | `AEST-10AEDT,M10.1.0,M4.1.0/3`    |

### Brightness

```cpp
#define DAY_ON_MS        3    // 30% (1 step = 10%, max 10)
#define NIGHT_ON_MS      1    // 10%
#define NIGHT_START_HOUR 22
#define NIGHT_END_HOUR   7
```

## Building and uploading

The sketch is built from the `esp_wifi_16x32_clock/` folder (Arduino requires the main file to match its folder name).

First upload over USB:

```sh
arduino-cli compile --fqbn esp8266:esp8266:d1_mini esp_wifi_16x32_clock
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp8266:esp8266:d1_mini:baud=115200 esp_wifi_16x32_clock
```

Later updates over WiFi:

```sh
arduino-cli upload -p wifi-clock.local --protocol network --upload-field password= \
  --fqbn esp8266:esp8266:d1_mini esp_wifi_16x32_clock
```

## Files

| File | Description |
|------|-------------|
| `main.ino` | Main sketch — WiFi, NTP/time zone, OTA, MAX7219 control |
