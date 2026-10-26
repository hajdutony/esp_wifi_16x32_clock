#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>                        // https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <coredecls.h>                      // settimeofday_cb()
#include "sun.h"
#include "weather.h"

// ============================================================================
// Configuration
//
// WiFi is set up from a phone: if the clock cannot join a known network it
// opens its own network SETUP_AP_NAME; connect to it and pick your WiFi.
// Brightness, night hours, time zone, the date and weather display are changed
// on the settings page at http://wifi-clock.local and saved in flash. The values
// below are only the defaults used on first boot.
// ============================================================================

#define OTA_HOSTNAME "wifi-clock"               // Network name (OTA updates, settings page)
#define OTA_PASSWORD ""                         // Set to require a password for updates
#define SETUP_AP_NAME "wifi-clock-setup"        // WiFi network opened for setup
#define SETUP_PORTAL_SECONDS 300                // Close the setup network after 5 min

#define SS_PIN 15                              // ESP8266 GPIO pin for MAX7219 CS

// MAX7219 SPI Driver commands
#define MAX7219_TEST     0x0f
#define MAX7219_BRIGHTNESS 0x0a
#define MAX7219_SCAN_LIMIT 0x0b
#define MAX7219_DECODE_MODE 0x09
#define MAX7219_SHUTDOWN   0x0C

// Brightness: hardware intensity (0x00 = lowest the MAX7219 can do), further
// dimmed in software by lighting the display only dimOnMs out of every
// DIM_PERIOD_MS milliseconds, timed by the hardware timer (timer1) so WiFi
// activity can't stretch the lit time. Brightness levels on the settings page are
// 0-10, i.e. 0-100% of the lowest hardware brightness in 10% steps.
#define BRIGHTNESS    0x00
#define DIM_PERIOD_MS 10

// Defaults (first boot / after a settings reset)
#define DEFAULT_DAY_LEVEL       3               // 30% during the day
#define DEFAULT_NIGHT_LEVEL     1               // 10% at night
#define DEFAULT_AUTO_NIGHT      1               // Night = civil dusk to dawn (sun 6 deg below horizon)
#define DEFAULT_LATITUDE        48.7164         // Location for dusk/dawn: Kosice city centre
#define DEFAULT_LONGITUDE       21.2611
#define DEFAULT_NIGHT_START     22              // Fixed night hours, used when automatic is off
#define DEFAULT_NIGHT_END       7               // (or the sun never sets/rises that day)
#define DEFAULT_DATE_AT_SECOND  30              // Show the date from second 30...
#define DEFAULT_DATE_SECONDS    3               // ...for 3 seconds (0 = never)
#define DEFAULT_SHOW_WEATHER    1               // Weather (by day only)...
#define DEFAULT_WEATHER_AT_SECOND 45            // ...from second 45...
#define DEFAULT_WEATHER_SECONDS 3               // ...for 3 seconds (0 = never)

// POSIX time zone string; the ESP8266 handles summer/winter time from it.
// Europe/Bratislava: CET (UTC+1), CEST (UTC+2) from the last Sunday of March
// 02:00 until the last Sunday of October 03:00.
#define DEFAULT_TIME_ZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// Time settings
#define NTP_SERVER "europe.pool.ntp.org"

// Status dot (bottom-right pixel): blinks while waiting for the first time
// sync, stays on if the time has not been synced for STALE_AFTER_HOURS
// (the ESP8266 normally re-syncs every hour).
#define STALE_AFTER_HOURS 3

// Weather from Open-Meteo (free, no account) for the saved location
#define WEATHER_HOST "api.open-meteo.com"
#define WEATHER_EVERY_MIN   15                  // Refresh interval
#define WEATHER_RETRY_MIN   2                   // Retry after a failed request
#define WEATHER_MAX_AGE_MIN 60                  // Don't show older data
#define WEATHER_TIMEOUT_MS  3000

// If the clock has never synced the time this many minutes after power-up
// (e.g. WiFi stack stuck), restart it. Once the time is known the clock keeps
// running on its own and WiFi reconnects automatically in the background.
#define RESTART_IF_NO_SYNC_MIN 10

// Settings saved in flash (EEPROM emulation)
#define SETTINGS_MAGIC    0xC10C0003            // Change when the struct layout changes
#define SETTINGS_MAGIC_V2 0xC10C0002            // Layout without the weather fields
#define SETTINGS_MAGIC_V1 0xC10C0001            // Layout without the automatic night fields
struct Settings {
  uint32_t magic;
  uint8_t dayLevel;                             // 0-10
  uint8_t nightLevel;                           // 0-10
  uint8_t nightStart;                           // Hour 0-23
  uint8_t nightEnd;                             // Hour 0-23 (same as start = no night mode)
  uint8_t dateAtSecond;                         // 0-59
  uint8_t dateSeconds;                          // 0-10, 0 = never show the date
  char timeZone[64];
  // Added in layout 2
  uint8_t autoNight;                            // 1 = night from civil dusk to dawn
  float latitude;
  float longitude;
  // Added in layout 3
  uint8_t showWeather;                          // 1 = show the weather by day
  uint8_t weatherAtSecond;                      // 0-59
  uint8_t weatherSeconds;                       // 0-10, 0 = never show the weather
};
Settings settings;

// Today's civil dawn/dusk (UTC epochs), recalculated when the date changes
time_t todayDawn = 0, todayDusk = 0;
bool twilightValid = false;
int twilightDay = -1;                           // tm_yday the values are for, -1 = recalculate

#define SPI_FREQUENCY 4000000                   // MAX7219 supports up to 10 MHz

volatile uint8_t dimOnMs = DEFAULT_DAY_LEVEL;   // Current lit time, set from loop()

// Display contents, written by loop() and sent to the MAX7219s by the timer
// interrupt while the display is dark. frame[row][module], module 0 is sent
// first in the chain (same order as the old maxTransferDATA v1..v8).
volatile uint8_t frame[8][8];
volatile bool frameDirty = false;
unsigned long lastSyncMillis = 0;               // millis() of the last NTP sync
bool everSynced = false;

Weather weather;                                // Last weather received
bool weatherValid = false;
unsigned long weatherMillis = 0;                // millis() when it was received
unsigned long lastWeatherTry = 0;
bool weatherDue = true;                         // Fetch on the next chance

WiFiManager wifiManager;
ESP8266WebServer server(80);

uint8_t NUMBER[160] = {
  //0
  B00000000,
  B00111100,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B00111100,
  B00000000,
  //1
  B00000000,
  B00000010,
  B00000110,
  B00001010,
  B00010010,
  B00100010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000000,
  //2
  B00000000,
  B00111100,
  B01000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00001100,
  B00110000,
  B01000000,
  B01000000,
  B01000000,
  B01000000,
  B01000010,
  B00111100,
  B00000000,
  //3
  B00000000,
  B00111100,
  B01000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00001100,
  B00110010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B01000010,
  B00111100,
  B00000000,
  //4
  B00000000,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01001100,
  B00110010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B00000000,
  //5
  B00000000,
  B01111110,
  B01000000,
  B01000000,
  B01000000,
  B01000000,
  B01000000,
  B01001100,
  B00110010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B01000010,
  B00111100,
  B00000000,
  //6
  B00000000,
  B00111100,
  B01000010,
  B01000000,
  B01000000,
  B01000000,
  B01000000,
  B01001100,
  B00110010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B00111100,
  B00000000,
  //7
  B00000000,
  B01111110,
  B00000010,
  B00000010,
  B00000100,
  B00000100,
  B00000100,
  B00001000,
  B00001000,
  B00001000,
  B00010000,
  B00010000,
  B00010000,
  B00100000,
  B00100000,
  B00000000,
  //8
  B00000000,
  B00111100,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01001100,
  B00110010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B00111100,
  B00000000,
  //9
  B00000000,
  B00111100,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B01001100,
  B00110010,
  B00000010,
  B00000010,
  B00000010,
  B00000010,
  B01000010,
  B00111100,
  B00000000,
};


void maxTransferCMD(uint8_t address, uint8_t value) {  
  digitalWrite(SS_PIN, LOW);   
  for (uint8_t i = 0; i < 8; i++) { // Send to all 8 daisy-chained modules.
    SPI.transfer(address);    // Send address.
    SPI.transfer(value);      // Send the value.
  }
  digitalWrite(SS_PIN, HIGH); // Finish transfer.
}

// ---- Timer-driven display output ---------------------------------------------
// All SPI traffic after setup() happens in the timer1 interrupt, so it can't
// collide with anything in loop(). The interrupt only touches IRAM code and
// hardware registers, so it is safe while flash is busy (OTA, settings save).

// Send 8 (address, value) pairs - one per module - in a single 128-bit SPI
// transfer, then latch them with a rising CS edge.
static inline void IRAM_ATTR spiSendChain(uint8_t address, const volatile uint8_t *values) {
  while (SPI1CMD & SPIBUSY) {}
  const uint32_t bits = 8 * 16 - 1;
  SPI1U1 = (SPI1U1 & ~((SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO))) |
           (bits << SPILMOSI) | (bits << SPILMISO);
  // The FIFO sends the lowest byte of SPI1W0 first
  volatile uint32_t *fifo = &SPI1W0;
  for (uint8_t i = 0; i < 4; i++) {
    fifo[i] = address | (values[2 * i] << 8) | (address << 16) | ((uint32_t)values[2 * i + 1] << 24);
  }
  GPOC = 1 << SS_PIN;                           // CS low
  SPI1CMD |= SPIBUSY;
  while (SPI1CMD & SPIBUSY) {}
  GPOS = 1 << SS_PIN;                           // CS high: latch
}

static inline void IRAM_ATTR spiSendAll(uint8_t address, uint8_t value) {
  uint8_t values[8] = {value, value, value, value, value, value, value, value};
  spiSendChain(address, values);
}

static void IRAM_ATTR sendFrame() {
  frameDirty = false;
  for (uint8_t row = 0; row < 8; row++) spiSendChain(row + 1, frame[row]);
}

// Every 1 ms: display on for dimOnMs, off for the rest of DIM_PERIOD_MS.
// New digits are sent while the display is dark so they don't add lit time.
void IRAM_ATTR dimIsr() {
  static uint8_t tick = 0;
  uint8_t onMs = dimOnMs;
  if (tick == 0 && onMs > 0) spiSendAll(MAX7219_SHUTDOWN, 0x01);
  else if (tick == onMs) spiSendAll(MAX7219_SHUTDOWN, 0x00);
  bool dark = tick >= onMs;
  if (frameDirty && (dark || onMs >= DIM_PERIOD_MS)) sendFrame();
  if (++tick >= DIM_PERIOD_MS) tick = 0;
}

void startDimTimer() {
  timer1_attachInterrupt(dimIsr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); // 80 MHz / 16 = 5 MHz
  timer1_write(5000);                           // 5000 ticks = 1 ms
}

void stopDimTimer() {
  timer1_disable();
  timer1_detachInterrupt();
}

// Store one row for all 8 modules; the draw functions set frameDirty when done
void setRow(uint8_t row, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4,
            uint8_t v5, uint8_t v6, uint8_t v7, uint8_t v8) {
  frame[row][0] = v1; frame[row][1] = v2; frame[row][2] = v3; frame[row][3] = v4;
  frame[row][4] = v5; frame[row][5] = v6; frame[row][6] = v7; frame[row][7] = v8;
}

// Separator drawn in the 2 middle columns between the digit pairs
#define SEP_NONE   0
#define SEP_COLON  1                            // Time, HH:MM
#define SEP_PERIOD 2                            // Date, DD.MM

// One row (0-15) of a digit. The left pair is shifted 1 px left and the
// right pair 1 px right, leaving a 4 px gap in the middle for the separator.
uint8_t digitRow(uint8_t digit, uint8_t row, bool leftPair) {
  uint8_t v = NUMBER[row + digit * 16];
  return leftPair ? v << 1 : v >> 1;
}

bool sepPixel(uint8_t sep, uint8_t row) {
  if (sep == SEP_COLON) return row == 4 || row == 5 || row == 10 || row == 11;
  if (sep == SEP_PERIOD) return row == 13 || row == 14;
  return false;
}

// Draw two 2-digit numbers (e.g. hour/minute or day/month).
// dot: light the bottom-right pixel (status indicator)
// rain: light the top-right pixel (rain expected within the hour)
void draw_digits(uint8_t left, uint8_t right, uint8_t sep, bool dot, bool rain) {
  uint8_t a = left / 10;
  uint8_t b = left % 10;
  uint8_t c = right / 10;
  uint8_t d = right % 10;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t top = i, bottom = i + 8;
    setRow(i, digitRow(a, top, true),
                    digitRow(b, top, true) | (sepPixel(sep, top) ? 0x01 : 0x00),
                    digitRow(c, top, false) | (sepPixel(sep, top) ? 0x80 : 0x00),
                    digitRow(d, top, false) | ((rain && i == 0) ? 0x01 : 0x00),
                    digitRow(a, bottom, true),
                    digitRow(b, bottom, true) | (sepPixel(sep, bottom) ? 0x01 : 0x00),
                    digitRow(c, bottom, false) | (sepPixel(sep, bottom) ? 0x80 : 0x00),
                    digitRow(d, bottom, false) | ((dot && i == 7) ? 0x01 : 0x00));
  }
  frameDirty = true;
}

// Blank display with only the status dot (before the first time sync)
void draw_blank(bool dot) {
  for (uint8_t i = 0; i < 8; i++) {
    setRow(i, 0, 0, 0, 0, 0, 0, 0, (dot && i == 7) ? 0x01 : 0x00);
  }
  frameDirty = true;
}

// ---- Weather screen ---------------------------------------------------------
// Drawn on a 32x16 pixel canvas (bit 7 of each byte = leftmost pixel), then
// copied to the frame: modules 0-3 are the top row, 4-7 the bottom row.

uint8_t canvas[16][4];

void canvasPixel(int x, int y) {
  if (x >= 0 && x < 32 && y >= 0 && y < 16) canvas[y][x / 8] |= 0x80 >> (x % 8);
}

void canvasToFrame() {
  for (uint8_t y = 0; y < 16; y++)
    for (uint8_t m = 0; m < 4; m++) frame[y % 8][(y / 8) * 4 + m] = canvas[y][m];
  frameDirty = true;
}

// Temperature characters: the clock digits (6 px wide), minus and degree sign
#define CHAR_MINUS  10
#define CHAR_DEGREE 11

uint8_t charWidth(uint8_t ch) {
  return ch == CHAR_MINUS ? 4 : ch == CHAR_DEGREE ? 3 : 6;
}

void drawChar(uint8_t ch, int x) {
  for (uint8_t y = 0; y < 16; y++) {
    for (uint8_t col = 0; col < charWidth(ch); col++) {
      bool on;
      if (ch == CHAR_MINUS) on = y == 8;
      else if (ch == CHAR_DEGREE) on = (y == 1 || y == 3) ? col == 1 : y == 2 && col != 1;
      else on = NUMBER[ch * 16 + y] & (0x40 >> col);   // Digits use bits 6..1
      if (on) canvasPixel(x + col, y);
    }
  }
}

// Icon in columns 0-13, temperature centred in columns 14-31
void draw_weather(const Weather &w, bool dot, bool rain) {
  memset(canvas, 0, sizeof(canvas));
  const uint16_t *icon = WEATHER_ICONS[weatherIcon(w.code)];
  for (uint8_t y = 0; y < 16; y++)
    for (uint8_t x = 0; x < 14; x++)
      if (icon[y] & (0x8000 >> x)) canvasPixel(x, y);

  uint8_t chars[5], n = 0;
  int t = constrain(w.temperature, -99, 99);
  if (t < 0) chars[n++] = CHAR_MINUS;
  if (abs(t) >= 10) chars[n++] = abs(t) / 10;
  chars[n++] = abs(t) % 10;
  chars[n++] = CHAR_DEGREE;
  int width = -1;
  for (uint8_t i = 0; i < n; i++) width += charWidth(chars[i]) + 1;
  if (width > 18) width -= charWidth(chars[--n]) + 1;    // -10 and colder: no room for the degree sign
  int x = 14 + (18 - width) / 2;
  for (uint8_t i = 0; i < n; i++) {
    drawChar(chars[i], x);
    x += charWidth(chars[i]) + 1;
  }
  if (dot) canvasPixel(31, 15);
  if (rain) canvasPixel(31, 0);
  canvasToFrame();
}

// ---- Weather data -----------------------------------------------------------

// Blocks for up to WEATHER_TIMEOUT_MS; the display keeps running from the timer
bool fetchWeather() {
  char path[200];
  weatherRequestPath(path, sizeof(path), settings.latitude, settings.longitude);
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(WEATHER_TIMEOUT_MS);
  if (!http.begin(client, WEATHER_HOST, 80, path)) return false;
  int status = http.GET();
  bool ok = false;
  if (status == HTTP_CODE_OK) {
    Weather w;
    if (parseWeather(http.getString().c_str(), w)) {
      weather = w;
      weatherValid = true;
      weatherMillis = millis();
      ok = true;
    }
  }
  http.end();
  if (ok) Serial.printf("Weather: %d C, %s%s\n", weather.temperature, weatherDescription(weather.code),
                        weather.rainSoon ? ", rain soon" : "");
  else Serial.printf("Weather request failed (HTTP %d)\n", status);
  return ok;
}

bool weatherFresh() {
  return settings.showWeather && weatherValid &&
         millis() - weatherMillis < WEATHER_MAX_AGE_MIN * 60000UL;
}

// Get the current local time (time zone and DST applied by the ESP8266)
// Returns false until the first NTP sync
bool readLocalTime(struct tm &local) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;           // Not synced yet (still counting from 1970)
  localtime_r(&now, &local);
  return true;
}

// ---- Settings ---------------------------------------------------------------

// Fields added in settings layout 2
void defaultV2Settings() {
  settings.autoNight = DEFAULT_AUTO_NIGHT;
  settings.latitude = DEFAULT_LATITUDE;
  settings.longitude = DEFAULT_LONGITUDE;
}

// Fields added in settings layout 3
void defaultV3Settings() {
  settings.showWeather = DEFAULT_SHOW_WEATHER;
  settings.weatherAtSecond = DEFAULT_WEATHER_AT_SECOND;
  settings.weatherSeconds = DEFAULT_WEATHER_SECONDS;
}

void defaultSettings() {
  settings.magic = SETTINGS_MAGIC;
  settings.dayLevel = DEFAULT_DAY_LEVEL;
  settings.nightLevel = DEFAULT_NIGHT_LEVEL;
  settings.nightStart = DEFAULT_NIGHT_START;
  settings.nightEnd = DEFAULT_NIGHT_END;
  settings.dateAtSecond = DEFAULT_DATE_AT_SECOND;
  settings.dateSeconds = DEFAULT_DATE_SECONDS;
  strlcpy(settings.timeZone, DEFAULT_TIME_ZONE, sizeof(settings.timeZone));
  defaultV2Settings();
  defaultV3Settings();
}

void saveSettings();

void loadSettings() {
  EEPROM.begin(sizeof(Settings));
  EEPROM.get(0, settings);
  settings.timeZone[sizeof(settings.timeZone) - 1] = '\0';
  if (settings.magic == SETTINGS_MAGIC_V1 || settings.magic == SETTINGS_MAGIC_V2) {
    // Keep the saved settings, add defaults for the new fields
    Serial.println("Upgrading saved settings.");
    if (settings.magic == SETTINGS_MAGIC_V1) defaultV2Settings();
    defaultV3Settings();
    settings.magic = SETTINGS_MAGIC;
    saveSettings();
  } else if (settings.magic != SETTINGS_MAGIC) {
    Serial.println("No saved settings, using defaults.");
    defaultSettings();
  }
}

void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}

void applyTimeZone() {
  setenv("TZ", settings.timeZone, 1);
  tzset();
}

// Night period may cross midnight (22 -> 7) or not (1 -> 6)
bool isNight(uint8_t hour) {
  if (settings.nightStart == settings.nightEnd) return false;
  if (settings.nightStart < settings.nightEnd)
    return hour >= settings.nightStart && hour < settings.nightEnd;
  return hour >= settings.nightStart || hour < settings.nightEnd;
}

// Recalculate civil dawn/dusk when the local date changes
void updateTwilight(const struct tm &local) {
  if (local.tm_yday == twilightDay) return;
  twilightDay = local.tm_yday;
  twilightValid = civilTwilight(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                                settings.latitude, settings.longitude, todayDawn, todayDusk);
}

// Night: between dusk and dawn in automatic mode, else the fixed hours
bool isNightNow(time_t now, const struct tm &local) {
  if (settings.autoNight) {
    updateTwilight(local);
    if (twilightValid) return now < todayDawn || now >= todayDusk;
  }
  return isNight(local.tm_hour);
}

String formatLocal(time_t t) {
  struct tm tmv;
  localtime_r(&t, &tmv);
  char buf[8];
  strftime(buf, sizeof(buf), "%H:%M", &tmv);
  return buf;
}

// POSIX TZ strings only use these characters; rejecting anything else also
// keeps the value safe to put back into the settings page
bool validTimeZone(const String &tz) {
  if (tz.length() == 0 || tz.length() >= sizeof(settings.timeZone)) return false;
  for (char ch : tz) {
    if (!isalnum(ch) && !strchr(",./+-<>:", ch)) return false;
  }
  return true;
}

// ---- Settings web page ------------------------------------------------------

String numberInput(const char *name, int value, int min, int max) {
  return String("<input type=number name=") + name + " value=" + value +
         " min=" + min + " max=" + max + " required>";
}

void handleRoot() {
  struct tm local;
  char timeStr[24] = "not synced yet";
  if (readLocalTime(local)) strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M:%S", &local);
  String lastSync = everSynced ? String((millis() - lastSyncMillis) / 60000) + " min ago" : "never";

  String page;
  page.reserve(4000);
  page += F("<!doctype html><html><head><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>WiFi Clock</title><style>"
            "body{font-family:sans-serif;max-width:420px;margin:0 auto;padding:16px;background:#fff;color:#222}"
            "label{display:block;margin:12px 0 4px}input{width:100%;padding:8px;box-sizing:border-box;font-size:16px}"
            "button{margin-top:16px;padding:10px 16px;font-size:16px}small{color:#666}"
            "@media(prefers-color-scheme:dark){body{background:#111;color:#eee}small{color:#999}}"
            "</style></head><body><h1>WiFi Clock</h1>");
  page += "<p>Time: <b>" + String(timeStr) + "</b><br><small>Last time sync: " + lastSync +
          " &middot; WiFi: " + WiFi.SSID() + " (" + WiFi.RSSI() + " dBm) &middot; IP: " +
          WiFi.localIP().toString() + "</small></p>";
  if (server.hasArg("saved")) page += F("<p><b>Saved.</b></p>");
  if (server.hasArg("badtz")) page += F("<p><b>Saved, but the time zone was not valid and was not changed.</b></p>");
  page += F("<form method=post action=/save>");
  page += "<label>Day brightness (0&ndash;10)</label>" + numberInput("day", settings.dayLevel, 0, 10);
  page += "<label>Night brightness (0&ndash;10, 0 = off)</label>" + numberInput("night", settings.nightLevel, 0, 10);
  page += String("<label><input type=checkbox name=auto style='width:auto'") +
          (settings.autoNight ? " checked" : "") +
          "> Automatic night: from dusk to dawn (civil twilight)</label>";
  if (settings.autoNight) {
    struct tm today;
    if (readLocalTime(today)) {
      updateTwilight(today);
      page += twilightValid
                  ? "<small>Today: dawn " + formatLocal(todayDawn) + ", dusk " + formatLocal(todayDusk) + "</small>"
                  : String("<small>The sun doesn't reach dusk/dawn today; using the fixed hours.</small>");
    }
  }
  page += "<label>Latitude <small>(north +)</small></label><input type=number name=lat step=0.0001 min=-90 max=90 required value=" +
          String(settings.latitude, 4) + ">";
  page += "<label>Longitude <small>(east +)</small></label><input type=number name=lon step=0.0001 min=-180 max=180 required value=" +
          String(settings.longitude, 4) + ">";
  page += "<label>Fixed night starts at hour <small>(when automatic is off)</small></label>" +
          numberInput("nstart", settings.nightStart, 0, 23);
  page += "<label>Fixed night ends at hour <small>(same as start = no night mode)</small></label>" +
          numberInput("nend", settings.nightEnd, 0, 23);
  page += "<label>Show date at second</label>" + numberInput("dat", settings.dateAtSecond, 0, 59);
  page += "<label>Show date for seconds (0 = never)</label>" + numberInput("dsec", settings.dateSeconds, 0, 10);
  page += String("<label><input type=checkbox name=weather style='width:auto'") +
          (settings.showWeather ? " checked" : "") +
          "> Show the weather (by day, for the location above)</label>";
  if (settings.showWeather) {
    page += weatherValid
                ? "<small>Now: " + String(weather.temperature) + " &deg;C, " + weatherDescription(weather.code) +
                      (weather.rainSoon ? ", rain expected within the hour" : "") + " &middot; updated " +
                      String((millis() - weatherMillis) / 60000) + " min ago</small>"
                : String("<small>No weather data yet.</small>");
  }
  page += "<label>Show weather at second</label>" + numberInput("wat", settings.weatherAtSecond, 0, 59);
  page += "<label>Show weather for seconds (0 = never)</label>" + numberInput("wsec", settings.weatherSeconds, 0, 10);
  page += "<label>Time zone <small>(POSIX TZ string)</small></label>"
          "<input name=tz list=zones required value=\"" + String(settings.timeZone) + "\">";
  page += F("<datalist id=zones>"
            "<option value='CET-1CEST,M3.5.0,M10.5.0/3'>Central Europe</option>"
            "<option value='GMT0BST,M3.5.0/1,M10.5.0'>UK</option>"
            "<option value='EET-2EEST,M3.5.0/3,M10.5.0/4'>Eastern Europe</option>"
            "<option value='EST5EDT,M3.2.0,M11.1.0'>US Eastern</option>"
            "<option value='PST8PDT,M3.2.0,M11.1.0'>US Pacific</option>"
            "<option value='JST-9'>Japan</option>"
            "<option value='AEST-10AEDT,M10.1.0,M4.1.0/3'>Sydney</option>"
            "<option value='UTC0'>UTC</option></datalist>");
  page += F("<button type=submit>Save</button></form>"
            "<form method=post action=/reset-wifi onsubmit=\"return confirm("
            "'Forget the WiFi network? The clock will open the " SETUP_AP_NAME " network for setup.')\">"
            "<button type=submit>Change WiFi network</button></form>"
            "</body></html>");
  server.send(200, "text/html", page);
}

void handleSave() {
  auto num = [](const char *name, int min, int max, uint8_t current) -> uint8_t {
    if (!server.hasArg(name)) return current;
    int v = server.arg(name).toInt();
    return constrain(v, min, max);
  };
  settings.dayLevel = num("day", 0, 10, settings.dayLevel);
  settings.nightLevel = num("night", 0, 10, settings.nightLevel);
  settings.nightStart = num("nstart", 0, 23, settings.nightStart);
  settings.nightEnd = num("nend", 0, 23, settings.nightEnd);
  settings.dateAtSecond = num("dat", 0, 59, settings.dateAtSecond);
  settings.dateSeconds = num("dsec", 0, 10, settings.dateSeconds);
  settings.weatherAtSecond = num("wat", 0, 59, settings.weatherAtSecond);
  settings.weatherSeconds = num("wsec", 0, 10, settings.weatherSeconds);
  settings.autoNight = server.hasArg("auto") ? 1 : 0;   // Unticked checkboxes aren't sent
  settings.showWeather = server.hasArg("weather") ? 1 : 0;
  if (server.hasArg("lat")) settings.latitude = constrain(server.arg("lat").toFloat(), -90.0f, 90.0f);
  if (server.hasArg("lon")) settings.longitude = constrain(server.arg("lon").toFloat(), -180.0f, 180.0f);
  twilightDay = -1;                             // Recalculate dusk/dawn
  weatherDue = true;                            // Location may have changed
  String tz = server.arg("tz");
  tz.trim();
  bool tzOk = validTimeZone(tz);
  if (tzOk) strlcpy(settings.timeZone, tz.c_str(), sizeof(settings.timeZone));
  saveSettings();
  applyTimeZone();
  Serial.println("Settings saved.");
  server.sendHeader("Location", tzOk ? "/?saved=1" : "/?badtz=1");
  server.send(303);
}

void handleResetWifi() {
  server.send(200, "text/html",
              "<meta name=viewport content='width=device-width'><p>WiFi settings cleared. "
              "Connect to the <b>" SETUP_AP_NAME "</b> network to set up a new one.</p>");
  delay(500);
  wifiManager.resetSettings();
  ESP.restart();
}

// ---- Setup / loop -----------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Clock Starting ---");

  loadSettings();

  pinMode(SS_PIN, OUTPUT);  

  SPI.setBitOrder(MSBFIRST);
  SPI.begin();
  SPI.setFrequency(SPI_FREQUENCY);

  maxTransferCMD(MAX7219_TEST, 0x00);
  maxTransferCMD(MAX7219_DECODE_MODE, 0x00); // Disable BCD mode. 
  maxTransferCMD(MAX7219_BRIGHTNESS, BRIGHTNESS); // Set brightness (0x00 -> 0x0F)
  maxTransferCMD(MAX7219_SCAN_LIMIT, 0x0f);  // Scan all digits.
  draw_blank(false);
  sendFrame();                               // Timer isn't running yet
  maxTransferCMD(MAX7219_SHUTDOWN, 0x01);    // Turn on chip.
  Serial.println("MAX7219 initialized.");

  dimOnMs = settings.dayLevel;
  startDimTimer();

  settimeofday_cb([](bool fromSntp) {
    if (!fromSntp) return;
    lastSyncMillis = millis();
    everSynced = true;
    Serial.println("Time synced.");
  });
  configTime(settings.timeZone, NTP_SERVER);   // Syncs in the background (hourly)
  Serial.println("NTP initialized.");

  // Join the saved WiFi network. If that fails, open the setup network
  // (non-blocking: the clock keeps running while it is open).
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  wifiManager.setHostname(OTA_HOSTNAME);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(SETUP_PORTAL_SECONDS);
  if (wifiManager.autoConnect(SETUP_AP_NAME)) {
    Serial.println("WiFi connected to " + WiFi.SSID());
  } else {
    Serial.println("WiFi not connected, setup network open: " SETUP_AP_NAME);
  }

  // Wireless (OTA) updates. Stop the dimming ticker while flashing so it
  // does not compete with the update for CPU time.
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    stopDimTimer();
    Serial.println("OTA update starting...");
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA update done, rebooting."); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error %u\n", error);
    startDimTimer();
  });

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset-wifi", HTTP_POST, handleResetWifi);
}

void loop() {      
  wifiManager.process();                        // Runs the setup network while it is open

  bool connected = WiFi.status() == WL_CONNECTED;

  // Start OTA and the settings page once WiFi is up for the first time
  static bool servicesStarted = false;
  if (!servicesStarted && connected && !wifiManager.getConfigPortalActive()) {
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
    ArduinoOTA.begin();                         // Also starts mDNS (wifi-clock.local)
    server.begin();
    MDNS.addService("http", "tcp", 80);
    servicesStarted = true;
    Serial.println("Ready: http://" OTA_HOSTNAME ".local");
  }
  if (servicesStarted) {
    ArduinoOTA.handle();
    server.handleClient();
  }

  // After the setup network has closed, keep retrying the saved network
  static unsigned long lastRetry = 0;
  if (!connected && !wifiManager.getConfigPortalActive() && millis() - lastRetry > 60000UL) {
    lastRetry = millis();
    WiFi.begin();
  }

  // Redraw when the second changes; loop() itself runs continuously so OTA
  // stays responsive and the colon blinks in step with the seconds
  static time_t lastSecond = 0;
  time_t nowSecond = time(nullptr);
  if (nowSecond == lastSecond) {
    delay(10);
    return;
  }
  lastSecond = nowSecond;

  if (!everSynced && millis() > RESTART_IF_NO_SYNC_MIN * 60UL * 1000UL) {
    Serial.println("No time sync since power-up, restarting.");
    ESP.restart();
  }

  static int debugCount = 0;
  struct tm local;
  if (readLocalTime(local)) {
    bool night = isNightNow(nowSecond, local);
    dimOnMs = night ? settings.nightLevel : settings.dayLevel;
    bool stale = millis() - lastSyncMillis > STALE_AFTER_HOURS * 3600UL * 1000UL;
    bool fresh = weatherFresh();
    bool rain = fresh && weather.rainSoon;
    bool showDate = local.tm_sec >= settings.dateAtSecond &&
                    local.tm_sec < settings.dateAtSecond + settings.dateSeconds;
    bool showWeather = !night && fresh && local.tm_sec >= settings.weatherAtSecond &&
                       local.tm_sec < settings.weatherAtSecond + settings.weatherSeconds;
    if (showDate) {
      draw_digits(local.tm_mday, local.tm_mon + 1, SEP_PERIOD, stale, rain);
    } else if (showWeather) {
      draw_weather(weather, stale, rain);
    } else {
      // Colon blinks: on during even seconds
      draw_digits(local.tm_hour, local.tm_min,
                  local.tm_sec % 2 == 0 ? SEP_COLON : SEP_NONE, stale, rain);
    }
    if (debugCount < 5) {
      char buf[12];
      sprintf(buf, "%02d:%02d", local.tm_hour, local.tm_min);
      Serial.print("Time: ");
      Serial.print(buf);
      Serial.print(" | WiFi: ");
      Serial.print(WiFi.localIP().toString());
      Serial.print(" | Epoch: ");
      Serial.println((uint32_t)time(nullptr));
      debugCount++;
    }
  } else {
    static bool blink = false;
    blink = !blink;
    draw_blank(blink);                          // Blinking dot: waiting for time
    if (debugCount < 10) {
      Serial.println("NTP not ready yet...");
      debugCount++;
    }
  }

  // Refresh the weather right after a redraw, so the request delays nothing
  static unsigned long weatherWait = 0;
  if (connected && settings.showWeather && (weatherDue || millis() - lastWeatherTry > weatherWait)) {
    weatherDue = false;
    lastWeatherTry = millis();
    weatherWait = (fetchWeather() ? WEATHER_EVERY_MIN : WEATHER_RETRY_MIN) * 60000UL;
  }
}
