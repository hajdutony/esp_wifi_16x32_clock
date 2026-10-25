# esp_wifi_16x32_clock

WiFi-connected NTP clock driving two cascaded MAX7219 8×8 LED modules (16×32 pixels) via an ESP8266.

## Features

- **NTP time sync** via `1.asia.pool.ntp.org`
- **Automatic DST handling** for Europe/Bratislava (CET/CEST)
  - EU rule: last Sunday of March 01:00 UTC → last Sunday of October 01:00 UTC
  - Winter offset: UTC+1 (CET)
  - Summer offset: UTC+2 (CEST)
- **Configurable timezone** — see `main.ino` lines 27–37

## Hardware

- **ESP8266** (NodeMCU / WeMos D1 Mini)
- **8× MAX7219** 8×8 LED dot-matrix modules, daisy-chained (4 wide × 2 high)
- **GPIO 15 (D8)** — SPI chip select

## Timezone Configuration

Edit `main.ino` lines 27–37 to switch timezones:

```cpp
#define BASE_UTC_OFFSET 3600   // CET = UTC+1 (winter)
#define DST_OFFSET      3600   // CEST = +3600 over CET
```

| Region    | BASE_UTC_OFFSET | DST_OFFSET |
|-----------|-----------------|------------|
| UTC       | 0               | 0          |
| EST (US)  | -18000          | 3600       |
| PST (US)  | -28800          | 3600       |
| CET (EU)  | 3600            | 3600       |
| JST (JP)  | 32400           | 0          |
| AEST (AU) | 36000           | 3600       |

## Files

| File | Description |
|------|-------------|
| `main.ino` | Main sketch — WiFi, NTP, MAX7219 control |
| `dst.h` | DST handler header |
| `dst.cpp` | DST handler — EU rules, Zeller's congruence |
