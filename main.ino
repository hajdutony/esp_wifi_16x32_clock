#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <coredecls.h>                      // settimeofday_cb()

// ============================================================================
// Configuration - edit these for your location
// ============================================================================

const char* ssid         = "tony_NET";        // WiFi SSID
const char* password     = "REMOVED";         // WiFi password

#define OTA_HOSTNAME "wifi-clock"               // Network name for wireless updates
#define OTA_PASSWORD ""                         // Set to require a password for updates

#define SS_PIN 15                              // ESP8266 GPIO pin for MAX7219 CS

// MAX7219 SPI Driver commands
#define MAX7219_TEST     0x0f
#define MAX7219_BRIGHTNESS 0x0a
#define MAX7219_SCAN_LIMIT 0x0b
#define MAX7219_DECODE_MODE 0x09
#define MAX7219_SHUTDOWN   0x0C

// Brightness: hardware intensity (0x00 = lowest the MAX7219 can do), further
// dimmed in software by lighting the display only DAY_ON_MS / NIGHT_ON_MS out
// of every DIM_PERIOD_MS milliseconds (3/10 = 30% of the lowest hardware
// brightness). Set an *_ON_MS equal to DIM_PERIOD_MS for no software dimming.
#define BRIGHTNESS    0x00
#define DIM_PERIOD_MS 10
#define DAY_ON_MS     3                         // 30% during the day
#define NIGHT_ON_MS   1                         // 10% at night
#define NIGHT_START_HOUR 22                     // Night mode from 22:00...
#define NIGHT_END_HOUR   7                      // ...until 07:00 (local time)

// Time settings
#define NTP_SERVER "europe.pool.ntp.org"

// POSIX time zone string; the ESP8266 handles summer/winter time from it.
// Europe/Bratislava: CET (UTC+1), CEST (UTC+2) from the last Sunday of March
// 02:00 until the last Sunday of October 03:00. Other examples:
//   UTC:            "UTC0"
//   UK:             "GMT0BST,M3.5.0/1,M10.5.0"
//   US Eastern:     "EST5EDT,M3.2.0,M11.1.0"
//   US Pacific:     "PST8PDT,M3.2.0,M11.1.0"
//   Japan:          "JST-9"
//   Sydney:         "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define TIME_ZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// Status dot (bottom-right pixel): blinks while waiting for the first time
// sync, stays on if the time has not been synced for STALE_AFTER_HOURS
// (the ESP8266 normally re-syncs every hour).
#define STALE_AFTER_HOURS 3

// Date: shown as DD.MM for DATE_SHOW_SECONDS every minute, from second
// DATE_AT_SECOND. Set DATE_SHOW_SECONDS to 0 to never show the date.
#define DATE_AT_SECOND    30
#define DATE_SHOW_SECONDS 3

// If the clock has never synced the time this many minutes after power-up
// (e.g. WiFi stack stuck), restart it. Once the time is known the clock keeps
// running on its own and WiFi reconnects automatically in the background.
#define RESTART_IF_NO_SYNC_MIN 10

Ticker dimTicker;
uint8_t dimOnMs = DAY_ON_MS;                    // Current lit time, set from loop()
unsigned long lastSyncMillis = 0;               // millis() of the last NTP sync
bool everSynced = false;

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

// Called every 1 ms: switch the display on for dimOnMs, off for the rest
// of the period. Ticker callbacks run between loop() yields, so they never
// interrupt an SPI transfer in progress.
void dimTick() {
  static uint8_t tick = 0;
  if (tick == 0) maxTransferCMD(MAX7219_SHUTDOWN, 0x01);
  else if (tick == dimOnMs) maxTransferCMD(MAX7219_SHUTDOWN, 0x00);
  if (++tick >= DIM_PERIOD_MS) tick = 0;
}

void maxTransferDATA(uint8_t address, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t v5, uint8_t v6, uint8_t v7, uint8_t v8) {  
  digitalWrite(SS_PIN, LOW);   

  SPI.transfer(address);      // Send the value.
  SPI.transfer(v1);           // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(v2);           // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(v3);           // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(v4);           // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(v5);           // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(v6);           // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(v7);           // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(v8);           // Send the value.

  digitalWrite(SS_PIN, HIGH); // Finish transfer.
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
void draw_digits(uint8_t left, uint8_t right, uint8_t sep, bool dot) {
  uint8_t a = left / 10;
  uint8_t b = left % 10;
  uint8_t c = right / 10;
  uint8_t d = right % 10;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t top = i, bottom = i + 8;
    maxTransferDATA(i + 1, digitRow(a, top, true),
                    digitRow(b, top, true) | (sepPixel(sep, top) ? 0x01 : 0x00),
                    digitRow(c, top, false) | (sepPixel(sep, top) ? 0x80 : 0x00),
                    digitRow(d, top, false),
                    digitRow(a, bottom, true),
                    digitRow(b, bottom, true) | (sepPixel(sep, bottom) ? 0x01 : 0x00),
                    digitRow(c, bottom, false) | (sepPixel(sep, bottom) ? 0x80 : 0x00),
                    digitRow(d, bottom, false) | ((dot && i == 7) ? 0x01 : 0x00));
  }
}

// Blank display with only the status dot (before the first time sync)
void draw_blank(bool dot) {
  for (uint8_t i = 0; i < 8; i++) {
    maxTransferDATA(i + 1, 0, 0, 0, 0, 0, 0, 0, (dot && i == 7) ? 0x01 : 0x00);
  }
}

// Get the current local time (time zone and DST applied by the ESP8266)
// Returns false until the first NTP sync
bool readLocalTime(struct tm &local) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;           // Not synced yet (still counting from 1970)
  localtime_r(&now, &local);
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Clock Starting ---");

  pinMode(SS_PIN, OUTPUT);  

  SPI.setBitOrder(MSBFIRST);
  SPI.begin();

  maxTransferCMD(MAX7219_TEST, 0x00);
  maxTransferCMD(MAX7219_DECODE_MODE, 0x00); // Disable BCD mode. 
  maxTransferCMD(MAX7219_BRIGHTNESS, BRIGHTNESS); // Set brightness (0x00 -> 0x0F)
  maxTransferCMD(MAX7219_SCAN_LIMIT, 0x0f);  // Scan all digits.
  draw_blank(false);
  maxTransferCMD(MAX7219_SHUTDOWN, 0x01);    // Turn on chip.
  Serial.println("MAX7219 initialized.");

  dimTicker.attach_ms(1, dimTick);

  // Connect in the background; the ESP8266 keeps retrying and reconnects
  // automatically if the connection drops later.
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  settimeofday_cb([](bool fromSntp) {
    if (!fromSntp) return;
    lastSyncMillis = millis();
    everSynced = true;
    Serial.println("Time synced.");
  });
  configTime(TIME_ZONE, NTP_SERVER);           // Syncs in the background (hourly)
  Serial.println("NTP initialized.");

  // Wireless (OTA) updates. Stop the dimming ticker while flashing so it
  // does not compete with the update for CPU time.
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    dimTicker.detach();
    Serial.println("OTA update starting...");
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA update done, rebooting."); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error %u\n", error);
    dimTicker.attach_ms(1, dimTick);
  });
}

void loop() {      
  // Start OTA once WiFi is up for the first time
  static bool otaStarted = false;
  if (!otaStarted && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
    ArduinoOTA.begin();
    otaStarted = true;
    Serial.println("OTA ready: " OTA_HOSTNAME ".local");
  }
  if (otaStarted) ArduinoOTA.handle();

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
    bool night = local.tm_hour >= NIGHT_START_HOUR || local.tm_hour < NIGHT_END_HOUR;
    dimOnMs = night ? NIGHT_ON_MS : DAY_ON_MS;
    bool stale = millis() - lastSyncMillis > STALE_AFTER_HOURS * 3600UL * 1000UL;
    bool showDate = local.tm_sec >= DATE_AT_SECOND &&
                    local.tm_sec < DATE_AT_SECOND + DATE_SHOW_SECONDS;
    if (showDate) {
      draw_digits(local.tm_mday, local.tm_mon + 1, SEP_PERIOD, stale);
    } else {
      // Colon blinks: on during even seconds
      draw_digits(local.tm_hour, local.tm_min,
                  local.tm_sec % 2 == 0 ? SEP_COLON : SEP_NONE, stale);
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
}
