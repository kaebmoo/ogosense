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


const int RELAY1 = D1;
const int BATT = A0;
const int POWER = D7;
//flag for saving data
bool shouldSaveConfig = false;


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
const char* server = "mqtt.thingspeak.com"; 
unsigned long channelID = 793986;
char* readAPIKey = "MYB70MLRK0HV274L";
char* writeAPIKey = "VGLY9POK9GGK4WID";

char mqttUserName[] = "chang";  // Can be any name.
char mqttPass[] = "";  // Change this your MQTT API Key from Account > MyProfile.
String subscribeTopic = "channels/" + String( channelID ) + "/subscribe/fields/field3";

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

void setup() {
  // set the digital pin as output:
  pinMode(ledPin, OUTPUT);
  pinMode(POWER, INPUT);
  pinMode(BATT, INPUT);
  pinMode(RELAY1, OUTPUT);
  
  Serial.begin(115200);
  Serial.println();

  // ads1015.begin();
  setupWifi();
  checkPowerLine();
  checkBattery();
  write2ThingSpeak();
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

  /*
   * 
  // Reconnect if MQTT client is not connected.
  if (!mqttClient.connected()) 
  {
    reconnect();
  }
  mqttClient.loop();   // Call the loop continuously to establish connection to the server.

  */
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
    powerLine = 0;
    if (countOff == 0) {
      powerLineOff();
    }
    countOff++;
  }
  else if (powerPin == 1) {                // without battery = 2.29, with battery 2.4
    Serial.println("Power line OK");
    powerLine = 1;
    if (countOff > 0) {
      Alarm.free(idAlarm);
      idAlarm = dtINVALID_ALARM_ID;
      countOff = 0;
      
      writeSuccess = write2ThingSpeak();
      if (writeSuccess != 200) {                
        idTimer = timer.after(15000, write2ThingSpeakAgain);
      }
    }    
    
    if (millis() - lastUpdateTime >=  postingInterval) {
      lastUpdateTime = millis();
      writeSuccess = write2ThingSpeak();
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
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
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
  ThingSpeak.begin( client );
  mqttConnect();
}

void mqttConnect()
{
  mqttClient.setServer(server, 1883);   // Set the MQTT broker details.
  mqttClient.setCallback(callback);  
}

int write2ThingSpeak()
{
  Serial.println("Sending data to ThingSpeak");
  Serial.print("Power Line Status:");
  Serial.println(powerLine);
  
  ThingSpeak.setField( 1, powerLine );
  ThingSpeak.setField( 2, batteryVoltage );
  int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
  Serial.print("Send to Thingspeak status: ");
  Serial.println(writeSuccess);
  
  return writeSuccess;
}

void powerLineOff()
{
  int writeSuccess;
  writeSuccess = write2ThingSpeak();
  if (writeSuccess != 200) {    
    idTimer = timer.after(15000, write2ThingSpeakAgain);
  }
  
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
  batteryVoltage = ((float) adc0 * 3) / 1000;
  Serial.print("Analog read A0: ");
  Serial.println(adc0);

  Serial.print("Battery voltage: ");
  Serial.println(batteryVoltage);
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

  if (!strncmp(p, "on", 2) || !strncmp(p, "1", 1)) {
    digitalWrite(RELAY1, HIGH);
    idTimerSwitch = timerSwitch.after(10000, turnOn);
  }
  else if (!strncmp(p, "off", 3) || !strncmp(p, "0", 1) ) {
    digitalWrite(RELAY1, LOW);
  }
}

void turnOn()
{
  digitalWrite(RELAY1, LOW);
  timerSwitch.stop(idTimerSwitch);
}
