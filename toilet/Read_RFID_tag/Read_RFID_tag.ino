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
#include <PubSubClient.h>
#include <ArduinoJson.h> // https://arduinojson.org/v5/assistant/

#include "MFRC522.h"
#define RST_PIN  D1 // RST-PIN for RC522 - RFID - SPI - Modul GPIO5 
#define SS_PIN  D2 // SDA-PIN for RC522 - RFID - SPI - Modul GPIO4 
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

unsigned long channelID = 793986;
const char* server = "mqtt.ogonan.com"; 
char mqttUserName[] = "kaebmoo";  // Can be any name.
char mqttPass[] = "sealwiththekiss";  // Change this your MQTT API Key from Account > MyProfile.
String subscribeTopic = "/channels/" + String( channelID ) + "/subscribe/fields/field3";
String publishTopic = "/channels/" + String( channelID ) + "/subscribe/fields/field3";

WiFiClient client; 
PubSubClient mqttClient(client); 
static const char alphanum[] ="0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);    // Initialize serial communications
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  wifiConnect();
  mqttConnect();
}

String payloadString = "";
String compareString = "";

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
  payloadString = String( (char *) mfrc522.uid.uidByte);
  Serial.println(payloadString);
  if (payloadString != compareString) {
    compareString = payloadString;
    String payload = "{";
    payload += "\"id\":"; payload += "\""; payload += payloadString; payload += "\"";
    payload += "}";
    // char attributes[100];
    // payload.toCharArray( attributes, 100 );
    mqttClient.publish(publishTopic.c_str(), payload.c_str());
    Serial.println(payload);
    Serial.println();
    
  }

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
  Serial.println("Connected");
  
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


void mqttConnect()
{
  mqttClient.setServer(server, 1883);   // Set the MQTT broker details.
  mqttClient.setCallback(callback);  
}

void reconnect() 
{
  char clientID[9];

  // Loop until reconnected.
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Generate ClientID
    for (int i = 0; i < 8; i++) {
        clientID[i] = alphanum[random(51)];
    }
    clientID[8]='\0';
    // Connect to the MQTT broker
    if (mqttClient.connect(clientID,mqttUserName,mqttPass)) {
      Serial.print("Connected with Client ID:  ");
      Serial.print(String(clientID));
      Serial.print(", Username: ");
      Serial.print(mqttUserName);
      Serial.print(" , Passwword: ");
      Serial.println(mqttPass);
      mqttClient.subscribe( subscribeTopic.c_str() );
    } 
    else {
      Serial.print("failed, rc=");
      // Print to know why the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  int i;
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  /*
  if (!strncmp(p, "on", 2) || !strncmp(p, "1", 1)) {
    digitalWrite(RELAY1, HIGH);
    idTimerSwitch = timerSwitch.after(10000, turnOn);
  }
  else if (!strncmp(p, "off", 3) || !strncmp(p, "0", 1) ) {
    digitalWrite(RELAY1, LOW);
  }
  */
}
