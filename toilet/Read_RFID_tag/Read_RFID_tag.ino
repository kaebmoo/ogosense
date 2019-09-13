/*
 * RFID ESP8266 Wemos D1 mini
    RST GPIO05 (free GPIO)  D1
    SS  GPIO4 (free GPIO) D2
    MOSI  GPIO13 (HW SPI) D7
    MISO  GPIO12 (HW SPI) D6
    SCK GPIO14 (HW SPI) D5
    GND GND G
    3.3V  3.3V  3V3
 * 
 * 
 */

#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include "MFRC522.h"
#define RST_PIN  D1 // RST-PIN for RC522 - RFID - SPI - Modul GPIO5 
#define SS_PIN  D2 // SDA-PIN for RC522 - RFID - SPI - Modul GPIO4 
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);    // Initialize serial communications
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  wifiConnect();
}

void loop() {
  // put your main code here, to run repeatedly:
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
}




// Helper routine to dump a byte array as hex values to Serial
void dump_byte_array(byte *buffer, byte bufferSize) 
{
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void wifiConnect()
{
  int try2Connect = 0;
  
  Serial.println();
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.psk());
  String SSID = WiFi.SSID();
  String PSK = WiFi.psk();
  WiFi.begin("Red2", "ogonan2019");
  Serial.print("Connecting");
  Serial.println();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
      Serial.print(".");
      try2Connect++;
      if (try2Connect >= 30) {
        try2Connect = 0;
        ondemandWiFi();
      }
  }
  
}

void ondemandWiFi()
{
    WiFiManager wifiManager;

    if (!wifiManager.startConfigPortal("RFID")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    
}
