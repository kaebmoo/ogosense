/**
 * This example demonstrates how to read digital signals
 * It assumes there are push buttons with pullup resistors
 * connected to the 16 channels of the 74HC4067 mux/demux
 * 
 * For more about the interface of the library go to
 * https://github.com/pAIgn10/MUX74HC4067
 */
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <ArduinoJson.h> // https://arduinojson.org/v5/assistant/
//Install [Adafruit_NeoPixel_Library](https://github.com/adafruit/Adafruit_NeoPixel) first.
#include <Adafruit_NeoPixel.h>

#define PIN   D4
#define LED_NUM 1
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_NUM, PIN, NEO_GRB + NEO_KHZ800);

#include "MUX74HC4067.h"
#include "Timer.h"
Timer timer;

// Creates a MUX74HC4067 instance
// 1st argument is the Arduino PIN to which the EN pin connects
// 2nd-5th arguments are the Arduino PINs to which the S0-S3 pins connect
MUX74HC4067 mux(D1, D2, D5, D6, D7);

#define LED D4

unsigned long channelID = 803;


const char* server = "mqtt.ogonan.com"; 
char mqttUserName[] = "kaebmoo";  // Can be any name.
char mqttPass[] = "sealwiththekiss";  // Change this your MQTT API Key from Account > MyProfile.
String subscribeTopic = "channels/" + String( channelID ) + "/subscribe/fields/field3";
String publishTopic = "/rooms/" + String( channelID ) + "";

WiFiClient client; 
PubSubClient mqttClient(client); 
static const char alphanum[] ="0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.

bool shouldSaveConfig = false;

void setup()
{
  Serial.begin(115200);  // Initializes serial port
    // Waits for serial port to connect. Needed for Leonardo only
    while ( !Serial ) ;
  
  // Configures how the SIG pin will be interfaced
  // e.g. The SIG pin connects to PIN 3 on the Arduino,
  //      and PIN 3 is a digital input
  mux.signalPin(D3, INPUT, DIGITAL);

  
  leds.begin(); // This initializes the NeoPixel library.
  leds.setBrightness(64);
  led_set(255, 0, 0);//red
  // led_set(0, 0, 0);
  setupWifi();

  timer.after(120000, gotoSleep);
}

// Reads the 16 channels and reports on the serial monitor
// if the corresponding push button is pressed
void loop()
{
  byte data;
  
  if (!mqttClient.connected()) 
  {
    reconnect();
  }
  mqttClient.loop();   // Call the loop continuously to establish connection to the server.

  timer.update();

  for (byte i = 0; i < 16; ++i)
  {
    // Reads from channel i and returns HIGH or LOW
    data = mux.read(i);

    Serial.print("Push button at channel ");
    Serial.print(i);
    Serial.print(" is ");
    if ( data == HIGH ) {
      Serial.println("not pressed");
      
    }
    else if ( data == LOW ) {
      Serial.println("pressed");
      if (i == 0) {
        led_set(255, 255, 0);//green
        delay(200);
        led_set(0, 0, 0);
        delay(200);
        led_set(255, 255, 0);//green
        delay(200);
        led_set(0, 0, 0);
      }
      else {
        led_set(0, 255, 0);//green
        delay(200);
        led_set(0, 0, 0);
        delay(200);
        led_set(0, 255, 0);//green
        delay(200);
        led_set(0, 0, 0);
      }
      String payload = "{";
      payload += "\"Room\":"; payload += channelID; payload += ",";
      payload += "\"id\":"; payload += i;
      payload += "}";
      const char *msgBuffer;
      msgBuffer = payload.c_str();
      
      const char *topicBuffer;
      topicBuffer = publishTopic.c_str();

      Serial.print(topicBuffer); 
      Serial.print(" ");
      Serial.println(msgBuffer);
      
      mqttClient.publish(topicBuffer, msgBuffer);
    }
  }
  Serial.println();
  
  delay(300);
}

void led_set(uint8 R, uint8 G, uint8 B) {
  for (int i = 0; i < LED_NUM; i++) {
    leds.setPixelColor(i, leds.Color(R, G, B));
    leds.show();
    delay(50);
  }
}

void gotoSleep()
{
  Serial.println("Going to sleep in 10 second.");
  delay(10000);
  Serial.println("Deep sleep");
  led_set(0, 0, 0);
  ESP.deepSleep(0);
}

void setupWifi()
{
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.setBreakAfterConfig(true);
  wifiManager.setConfigPortalTimeout(60);
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  String alias = "ogosense-"+String(ESP.getChipId());
  if (!wifiManager.autoConnect(alias.c_str(), "")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  led_set(255, 0, 0);
  delay(200);
  led_set(0, 0, 0);
  delay(200);
  led_set(255, 0, 0);
  delay(200);
  led_set(0, 0, 0);
  delay(200);
  led_set(255, 0, 0);
  Serial.println("connected...yeey :)");
  // ThingSpeak.begin( client );
  mqttConnect();
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
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
