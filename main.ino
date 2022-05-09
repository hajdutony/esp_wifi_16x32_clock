#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WifiUDP.h>
#include <NTPClient.h>

const char* ssid     = "tony_NET";                       
const char* password = "46237057";

#define SS_PIN 15

// MAX7219 SPI LED Driver
#define MAX7219_TEST 0x0f // in real code put into a .h file
#define MAX7219_BRIGHTNESS 0x0a // in real code put into a .h file
#define MAX7219_SCAN_LIMIT 0x0b // in real code put into a .h file
#define MAX7219_DECODE_MODE 0x09 // in real code put into a .h file
#define MAX7219_SHUTDOWN 0x0C // in real code put into a .h file

#define NTP_OFFSET  7200 // In seconds - summer time = 7200, winter time = 3600
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "1.asia.pool.ntp.org"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);



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
uint8_t i;

  digitalWrite(SS_PIN, LOW);   
  SPI.transfer(address);      // Send address.
  SPI.transfer(value);        // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(value);        // Send the value.
  
  SPI.transfer(address);      // Send address.
  SPI.transfer(value);        // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(value);        // Send the value.

  SPI.transfer(address);      // Send address.
  SPI.transfer(value);        // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(value);        // Send the value.
  
  SPI.transfer(address);      // Send address.
  SPI.transfer(value);        // Send the value.
  SPI.transfer(address);      // Send address. 
  SPI.transfer(value);        // Send the value.
  
  digitalWrite(SS_PIN, HIGH); // Finish transfer.
}

void maxTransferDATA(uint8_t address, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t v5, uint8_t v6, uint8_t v7, uint8_t v8) {  
uint8_t i;

  digitalWrite(SS_PIN, LOW);   

  SPI.transfer(address);      // Send address.
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
  uint8_t i;
  uint8_t a = hour / 10;
  uint8_t b = hour % 10;
  uint8_t c = minute / 10;
  uint8_t d = minute % 10;
  for (i=0;i<8;i++) {
          maxTransferDATA(i+1, NUMBER[i+a*16], NUMBER[i+b*16], NUMBER[i+c*16], NUMBER[i+d*16], 
                          NUMBER[i+8+a*16], NUMBER[i+8+b*16], NUMBER[i+8+c*16], NUMBER[i+8+d*16]);
  }
}

void setup() {
  pinMode(SS_PIN, OUTPUT);  

  SPI.setBitOrder(MSBFIRST);   // Reverse the SPI Data o/p. 
  SPI.begin();                 // Start SPI

  //Serial.begin(9600);
  //Serial.println();
  //Serial.println();
  //Serial.print("Connecting to: ");
  //Serial.println(ssid);  
  maxTransferCMD(MAX7219_TEST, 0x00);
  WiFi.begin("tony_NET", "46237057");
  while (WiFi.status() != WL_CONNECTED)
  {
    //Serial.print(".");
    delay(200);
    //maxTransferCMD(MAX7219_TEST, 0x01);    
    //maxTransferCMD(MAX7219_TEST, 0x00);        
    //Finish test mode.
  }
  //Serial.println("");
  //Serial.println("WiFi connected.");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  timeClient.begin();

  maxTransferCMD(MAX7219_DECODE_MODE, 0x00); // Disable BCD mode. 
  maxTransferCMD(MAX7219_BRIGHTNESS, 0x00);  // Use lower intensity.  00 -> 0F
  maxTransferCMD(MAX7219_SCAN_LIMIT, 0x0f);  // Scan all digits.
  maxTransferCMD(MAX7219_SHUTDOWN, 0x01);    // Turn on chip.
}

void loop() {      
  timeClient.update(); 
  String formattedTime = timeClient.getFormattedTime(); 
  draw_time(formattedTime.substring(0, 2).toInt(), formattedTime.substring(3, 5).toInt());
  //Serial.print(formattedTime.substring(0, 2).toInt());
  //Serial.println(formattedTime.substring(3, 5).toInt());
      delay(1000); 
}
