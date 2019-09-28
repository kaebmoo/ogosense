/*
  Blink without Delay

  Turns on and off a light emitting diode (LED) connected to a digital pin,
  without using the delay() function. This means that other code can run at the
  same time without being interrupted by the LED code.

  The circuit:
  - Use the onboard LED.
  - Note: Most Arduinos have an on-board LED you can control. On the UNO, MEGA
    and ZERO it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN
    is set to the correct LED pin independent of which board is used.
    If you want to know what pin the on-board LED is connected to on your
    Arduino model, check the Technical Specs of your board at:
    https://www.arduino.cc/en/Main/Products

  created 2005
  by David A. Mellis
  modified 8 Feb 2010
  by Paul Stoffregen
  modified 11 Nov 2013
  by Scott Fitzgerald
  modified 9 Jan 2017
  by Arturo Guadalupi

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/BlinkWithoutDelay
*/

/*
MIT License

Copyright (c) 2019 Pornthep Nivatyakul

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Hardware: Wemos D1, Battery shield, Relay shield, Switching Power Supply 220VAC-12VDC, LiPo Battery 3.7V, Voltage Sensor Module 0-24VDC
*/

// #define THINGSBOARD
#define MQTT
#define THINGSPEAK
// #define ENVIRONMENT

#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <Time.h>
#include <TimeAlarms.h>
#include "Timer.h"
#include <ThingSpeak.h>
#include <Adafruit_ADS1015.h>
#include <PubSubClient.h>
//Install [Adafruit_NeoPixel_Library](https://github.com/adafruit/Adafruit_NeoPixel) first.
#include <Adafruit_NeoPixel.h>
#include <WEMOS_SHT3X.h>

SHT3X sht30(0x45);

#define PIN            D4
#define RELAYON        LOW
#define RELAYOFF       HIGH

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);


const int RELAY1 = D0;
const int BATT = A0;
const int POWER = D7;
//flag for saving data
bool shouldSaveConfig = false;

#define TOKEN "7yV3UaIeYZS62GbBCg6v"  // device token 
#define MQTTPORT  1883 // 1883 or 1888
char thingsboardServer[] = "thingsboard.ogonan.com";           // 
char mqtt_server[] = "mqtt.ogonan.com";
char *clientID = "sensor/power/867076";


// constants won't change. Used here to set a pin number:
const int ledPin =  LED_BUILTIN;// the number of the LED pin

// Variables will change:
int ledState = LOW;             // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)
int powerPin;
int powerLine;
float dif;
int countOn = 0;
int countOff = 0;
int idTimer;
float batteryVoltage = 0.0;

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 1;
char* readAPIKey = "YBDBDH7FOD0NTLID";
char* writeAPIKey = "LU07OLP4TQ5XVPOH";

// const char* server = "mqtt.ogonan.com"; 
char mqttUserName[] = "kaebmoo";  // Can be any name.
char mqttPass[] = "sealwiththekiss";  // Change this your MQTT API Key from Account > MyProfile.
// String subscribeTopic = "channels/" + String( channelID ) + "/subscribe/fields/field3";
// String subscribeTopic = "channels/" + String( channelID ) + "/subscribe/fields/field5";
String subscribeTopic = "node/" + String( channelID ) + "/control/messages";
String publishTopic = "node/" + String( channelID ) + "/status/messages";


const unsigned long postingInterval = 300L * 1000L;
long lastUpdateTime = 0; 
static const char alphanum[] ="0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.
                              
WiFiClient client; 
PubSubClient mqttClient(client); 

AlarmId idAlarm;
Timer timer, timerCheckBattery, timerSwitch;
int idTimerSwitch;

Adafruit_ADS1015 ads1015;

void setup() 
{
  // set the digital pin as output:
  pinMode(ledPin, OUTPUT);
  pinMode(POWER, INPUT);
  pinMode(BATT, INPUT);
  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1, HIGH);
  
  Serial.begin(115200);
  Serial.println();
  pixels.begin(); // This initializes the NeoPixel library.
  pixels.setBrightness(64);
  pixels.show(); // This sends the updated pixel color to the hardware.

  // ads1015.begin();
  setupWifi();
  ThingSpeak.begin( client );
  setupMqtt();
  checkPowerLine();
  checkBattery();
  #ifdef THINGSPEAK
  write2ThingSpeak();
  #endif
  timerCheckBattery.every(15000, checkBattery);
  // Serial.println("Delay Start 15 sec.");
  // delay(15000);
}

void loop() {
  // here is where you'd put code that needs to be running all the time.
  
  blink();

  checkPowerLine();
  
  Alarm.delay(500);
  timer.update();
  timerCheckBattery.update();
  timerSwitch.update();

  #ifdef MQTT
  // Reconnect if MQTT client is not connected.
  if (!mqttClient.connected()) 
  {
    reconnect();
  }
  mqttClient.loop();   // Call the loop continuously to establish connection to the server.
  
  #endif
 
}

void checkPowerLine()
{
  int writeSuccess;
  
  // powerPin = analogRead(A0);
  powerPin = digitalRead(POWER);
  Serial.print("Digital Read: ");
  Serial.println(powerPin);
  // dif = analogData * (3.07/1000);       // input 12 VDC, Voltage drop = 2.4VDC
  // analogData = (int) dif;
  // analogConvert = (analogData % 100) / 10.0 ;
  // Serial.print("Analog Convert: ");
  // Serial.println(dif);

  if (powerPin == 0) {
    Serial.println("WTF!!!");
    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); 
    pixels.show(); // This sends the updated pixel color to the hardware.
    
    powerLine = 0;
    if (countOff == 0) {
      powerLineOff();
    }
    countOff++;
  }
  else if (powerPin == 1) {                // without battery = 2.29, with battery 2.4
    Serial.println("Power line OK");
    // pixels.setPixelColor(0, pixels.Color(0, 255, 0)); 
    // pixels.show(); // This sends the updated pixel color to the hardware.
    powerLine = 1;
    if (countOff > 0) {
      Alarm.free(idAlarm);
      idAlarm = dtINVALID_ALARM_ID;
      countOff = 0;

      #ifdef THINGSPEAK
      writeSuccess = write2ThingSpeak();
      
      if (writeSuccess != 200) {                
        // Thingspeak
        idTimer = timer.after(15000, write2ThingSpeakAgain);
      }
      #endif
    }    
    
    if (millis() - lastUpdateTime >=  postingInterval) {
      lastUpdateTime = millis();
      #ifdef THINGSPEAK
      writeSuccess = write2ThingSpeak();
      #endif
    }
  }
}

void blink()
{
    
  // check to see if it's time to blink the LED; that is, if the difference
  // between the current time and last time you blinked the LED is bigger than
  // the interval at which you want to blink the LED.
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
      pixels.clear(); 

    } else {
      ledState = LOW;
      pixels.setPixelColor(0, pixels.Color(0, 0, 255)); 
    }

    // set the LED with the ledState of the variable:
    // digitalWrite(ledPin, ledState);
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
}


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
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
    Alarm.delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    Alarm.delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  
  
}


int write2ThingSpeak()
{
  float temperature;
  int humidity;
  
  Serial.println("Sending data to ThingSpeak");
  Serial.print("Power Line Status:");
  Serial.println(powerLine);
  
  ThingSpeak.setField( 1, powerLine );
  ThingSpeak.setField( 2, batteryVoltage );
  
  #ifdef ENVIRONMENT
  if (sht30.get() == 0) {
    temperature = sht30.cTemp;
    humidity = sht30.humidity;
    Serial.print("Temperature: ");
    Serial.println(temperature);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    ThingSpeak.setField( 3, temperature );
    ThingSpeak.setField( 4, humidity );
  }
  else {
    Serial.println("Temperature Sensor Error!");
  }
  #endif
  
  int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
  Serial.print("Send to Thingspeak status: ");
  Serial.println(writeSuccess);
  
  return writeSuccess;
}

void powerLineOff()
{
  int writeSuccess;
  #ifdef THINGSPEAK
  writeSuccess = write2ThingSpeak();
  if (writeSuccess != 200) {    
    idTimer = timer.after(15000, write2ThingSpeakAgain);
  }
  #endif THINGSPEAK
  
  Serial.println("Power Line Down : Trigger");
  idAlarm = Alarm.timerRepeat(15, sendStatus);
  Alarm.timerOnce(300, clearCounting);
}

void sendStatus()
{
  int writeSuccess;
  
  Alarm.disable(idAlarm);
  writeSuccess = write2ThingSpeak();
  Alarm.enable(idAlarm);
  if (writeSuccess != 200) {
    Alarm.disable(idAlarm);
    idTimer = timer.after(15000, write2ThingSpeakAgain);   
    Alarm.enable(idAlarm);
  }
}

void clearCounting()
{
  Serial.println("Clear Power Line Off Counting");
  Alarm.free(idAlarm);
  idAlarm = dtINVALID_ALARM_ID;
  countOff = 0;
}

void write2ThingSpeakAgain()
{
  Serial.println("Send data to ThingSpeak again");
  write2ThingSpeak();
  timer.stop(idTimer);
}


void checkBattery()
{
  int16_t adc0;
  
  
  // adc0 = ads1015.readADC_SingleEnded(0);
  adc0 = analogRead(BATT);
  batteryVoltage = ((float) adc0 * 4.52) / 1023;
  Serial.print("Analog read A0: ");
  Serial.println(adc0);

  Serial.print("Battery voltage: ");
  Serial.println(batteryVoltage);
}

void setupMqtt() {
  #ifdef THINGSBOARD
  mqttClient.setServer(thingsboardServer, MQTTPORT);  // default port 1883, mqtt_server, thingsboardServer
  #else
  Serial.print(mqtt_server);
  Serial.print(" ");
  Serial.println(MQTTPORT);
  mqttClient.setServer(mqtt_server, MQTTPORT);
  #endif
  mqttClient.setCallback(callback);

  #ifdef THINGSBOARD
  if (mqttClient.connect(clientID, TOKEN, NULL)) {
  #else
  if (mqttClient.connect(clientID,mqttUserName,mqttPass)) {
  #endif  
  
    // mqttClient.publish(roomStatus,"hello world");

    // mqttClient.subscribe(setmaxtemp);
    // mqttClient.subscribe(setmintemp);
    // mqttClient.subscribe(setmaxhumidity);
    // mqttClient.subscribe(setminhumidity);
    mqttClient.subscribe( subscribeTopic.c_str() );

    Serial.print("mqtt connected : ");
    #ifdef THINGSBOARD
    Serial.println(thingsboardServer);  // mqtt_server
    #else
    Serial.println(mqtt_server);  // mqtt_server
    #endif
  }
}

void reconnect() 
{
  char clientID[9];

  // Loop until reconnected.
  // while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");

    /*
    // Generate ClientID
    for (int i = 0; i < 8; i++) {
        clientID[i] = alphanum[random(51)];
    }
    clientID[8]='\0';
    */
    
    // Connect to the MQTT broker
    #ifdef THINGSBOARD
    mqttClient.setServer(thingsboardServer, MQTTPORT);
    if (mqttClient.connect(clientID, TOKEN, NULL)) {
      Serial.print("Connected with Client ID:  ");
      Serial.print(String(clientID));  
      Serial.print(", Token: ");
      Serial.print(TOKEN);
    #else
    mqttClient.setServer(mqtt_server, MQTTPORT);
    if (mqttClient.connect(clientID,mqttUserName,mqttPass)) {
      Serial.print("Connected with Client ID:  ");
      Serial.print(String(clientID));
      Serial.print(", Username: ");
      Serial.print(mqttUserName);
      Serial.print(" , Passwword: ");
      Serial.println(mqttPass);  
    
    #endif  
    
      
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
  // } // Loop
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  // mosquitto_pub -h mqtt.ogonan.com -t "/channels/867076/subscribe/messages" -u "user" -P "pass"  -m "off"
  // mosquitto_sub -h mqtt.ogonan.com -t "/channels/867076/publish/messages" -u "user" -P "pass"
  // mosquitto_sub -h mqtt.ogonan.com -t "/channels/867076/subscribe/messages" -u "user" -P "pass"
  
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

  if (!strncmp(p, "on", 2) || !strncmp(p, "1", 1)) {
    digitalWrite(RELAY1, RELAYON);
    idTimerSwitch = timerSwitch.after(10000, turnOn);
  }
  else if (!strncmp(p, "off", 3) || !strncmp(p, "0", 1) ) {
    digitalWrite(RELAY1, RELAYOFF);
  }

  String data = String(digitalRead(RELAY1), DEC);
  const char *msgBuffer;
  msgBuffer = data.c_str();
  Serial.print("Status Logic: ");
  Serial.println(msgBuffer);
  
  const char *topicBuffer;
  topicBuffer = publishTopic.c_str();
  mqttClient.publish( topicBuffer, msgBuffer );
}

void turnOn()
{
  digitalWrite(RELAY1, RELAYON);
  timerSwitch.stop(idTimerSwitch);
}

void sendThingsBoard(uint16_t power)
{
  // Just debug messages
  Serial.print( "Sending power status : [" );
  Serial.print( power );
  Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"Power Line Detect\":"; payload += power; payload += ",";
  payload += "\"active\":"; payload += true;
  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  mqttClient.publish( "v1/devices/me/telemetry", attributes );
  Serial.println( attributes );
  Serial.println();
}
