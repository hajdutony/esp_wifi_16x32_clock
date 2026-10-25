#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WifiUDP.h>
#include <NTPClient.h>
#include "dst.h"

// ============================================================================
// Configuration - edit these for your location
// ============================================================================

const char* ssid         = "tony_NET";        // WiFi SSID
const char* password     = "46237057";         // WiFi password

#define SS_PIN 15                              // ESP8266 GPIO pin for MAX7219 CS

// MAX7219 SPI Driver commands
#define MAX7219_TEST     0x0f
#define MAX7219_BRIGHTNESS 0x0a
#define MAX7219_SCAN_LIMIT 0x0b
#define MAX7219_DECODE_MODE 0x09
#define MAX7219_SHUTDOWN   0x0C

// NTP settings
#define NTP_INTERVAL 60 * 1000                 // Sync every 60 seconds
#define NTP_ADDRESS  "1.asia.pool.ntp.org"

// Timezone configuration
#define BASE_UTC_OFFSET 3600                   // CET = UTC+1 (winter offset)
#define DST_OFFSET      3600                   // CEST = UTC+2, i.e. +3600 over CET

// To switch to a different timezone, adjust BASE_UTC_OFFSET:
//   UTC:   0
//   EST:  -18000 (UTC-5)
//   PST:  -28800 (UTC-8)
//   JST:  +32400 (UTC+9)
// Then set DST_OFFSET to the extra seconds during DST (usually 3600).
// For regions with no DST, set DST_OFFSET = 0.

WiFiUDP ntpUDP;
// Use 0 offset so NTPClient returns UTC; we apply DST dynamically in loop()
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0, NTP_INTERVAL);

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

// Convert a UTC epoch and apply the DST offset to get local time
// Returns true if NTP has synced
bool getLocalTime(uint8_t &localHour, uint8_t &localMinute) {
  timeClient.update();

  if (!timeClient.isTimeSet()) {
    // NTP hasn't synced yet
    localHour = 0;
    localMinute = 0;
    return false;
  }
  // Get the current UTC epoch
  uint32_t utcEpoch = timeClient.getEpochTime();

  // Get the current UTC offset for DST
  int currentOffset = getDSTOffset(utcEpoch);

  // Apply the offset to get local time
  uint32_t localSeconds = utcEpoch + currentOffset;

  // Handle day wrap-around (in case DST transition crosses midnight)
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  epochToTime(localSeconds, year, month, day, hour, minute, second);

  localHour = hour;
  localMinute = minute;
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
  
  timeClient.begin();
  Serial.println("NTP initialized.");

  maxTransferCMD(MAX7219_DECODE_MODE, 0x00); // Disable BCD mode. 
  maxTransferCMD(MAX7219_BRIGHTNESS, 0x08);  // Set brightness (0x00 -> 0x0F)
  maxTransferCMD(MAX7219_SCAN_LIMIT, 0x0f);  // Scan all digits.
  maxTransferCMD(MAX7219_SHUTDOWN, 0x01);    // Turn on chip.
  Serial.println("MAX7219 initialized.");
}

void loop() {      
  static int debugCount = 0;
  uint8_t localHour, localMinute;
  if (getLocalTime(localHour, localMinute)) {
    draw_time(localHour, localMinute);
    if (debugCount < 5) {
      char buf[12];
      sprintf(buf, "%02d:%02d", localHour, localMinute);
      Serial.print("Time: ");
      Serial.print(buf);
      Serial.print(" | WiFi: ");
      Serial.print(WiFi.localIP().toString());
      Serial.print(" | Epoch: ");
      Serial.println(timeClient.getEpochTime());
      debugCount++;
    }
  } else {
    if (debugCount < 10) {
      Serial.println("NTP not ready yet...");
      debugCount++;
    }
  }
  delay(1000); 
}
