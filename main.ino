#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <time.h>

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

Ticker dimTicker;
uint8_t dimOnMs = DAY_ON_MS;                    // Current lit time, set from loop()

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

void draw_time(uint8_t hour, uint8_t minute) {
  uint8_t a = hour / 10;
  uint8_t b = hour % 10;
  uint8_t c = minute / 10;
  uint8_t d = minute % 10;
  for (uint8_t i = 0; i < 8; i++) {
    maxTransferDATA(i + 1, NUMBER[i + a * 16], NUMBER[i + b * 16],
                    NUMBER[i + c * 16], NUMBER[i + d * 16],
                    NUMBER[i + 8 + a * 16], NUMBER[i + 8 + b * 16],
                    NUMBER[i + 8 + c * 16], NUMBER[i + 8 + d * 16]);
  }
}

// Get the current local time (time zone and DST applied by the ESP8266)
// Returns false until the first NTP sync
bool getLocalTime(uint8_t &localHour, uint8_t &localMinute) {
  time_t now = time(nullptr);
  if (now < 1700000000) {
    // NTP hasn't synced yet (clock still counts from 1970)
    localHour = 0;
    localMinute = 0;
    return false;
  }

  struct tm local;
  localtime_r(&now, &local);
  localHour = local.tm_hour;
  localMinute = local.tm_min;
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Clock Starting ---");

  pinMode(SS_PIN, OUTPUT);  

  SPI.setBitOrder(MSBFIRST);
  SPI.begin();

  maxTransferCMD(MAX7219_TEST, 0x00);
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial.print(".");
    wifiAttempts++;
    if (wifiAttempts > 30) {
      Serial.println("\nWiFi connection FAILED!");
      // Display error pattern: flash all LEDs
      for (uint8_t i = 0; i < 8; i++) {
        maxTransferDATA(i + 1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
      }
      while (true) delay(500);
    }
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
  
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
  ArduinoOTA.begin();
  Serial.println("OTA ready: " OTA_HOSTNAME ".local");

  maxTransferCMD(MAX7219_DECODE_MODE, 0x00); // Disable BCD mode. 
  maxTransferCMD(MAX7219_BRIGHTNESS, BRIGHTNESS); // Set brightness (0x00 -> 0x0F)
  maxTransferCMD(MAX7219_SCAN_LIMIT, 0x0f);  // Scan all digits.
  maxTransferCMD(MAX7219_SHUTDOWN, 0x01);    // Turn on chip.
  Serial.println("MAX7219 initialized.");

  dimTicker.attach_ms(1, dimTick);
}

void loop() {      
  ArduinoOTA.handle();

  // Redraw once per second; loop() itself runs continuously so OTA stays responsive
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 1000) {
    delay(10);
    return;
  }
  lastDraw = millis();

  static int debugCount = 0;
  uint8_t localHour, localMinute;
  if (getLocalTime(localHour, localMinute)) {
    bool night = localHour >= NIGHT_START_HOUR || localHour < NIGHT_END_HOUR;
    dimOnMs = night ? NIGHT_ON_MS : DAY_ON_MS;
    draw_time(localHour, localMinute);
    if (debugCount < 5) {
      char buf[12];
      sprintf(buf, "%02d:%02d", localHour, localMinute);
      Serial.print("Time: ");
      Serial.print(buf);
      Serial.print(" | WiFi: ");
      Serial.print(WiFi.localIP().toString());
      Serial.print(" | Epoch: ");
      Serial.println((uint32_t)time(nullptr));
      debugCount++;
    }
  } else {
    if (debugCount < 10) {
      Serial.println("NTP not ready yet...");
      debugCount++;
    }
  }
}
