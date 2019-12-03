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

V0.1 init on PN Board
V0.2 One thingspeak channel for all node

*/

#define THINGSBOARD
// #define MQTT
#define THINGSPEAK
#define ENVIRONMENT

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
#include <EEPROM.h>
#include <assert.h>

SHT3X sht30(0x45);

#define KEY            D3
#define PIN            D4
#define RELAYON        LOW
#define RELAYOFF       HIGH

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);


const int RELAY1 = D0;
const int BATT = A0;
const int POWER = D7;
//flag for saving data
bool shouldSaveConfig = false;

// #define TOKEN "HMrHsdYzNC6TRShI3Nch"  // device token node 
char TOKEN[] = "HMrHsdYzNC6TRShI3Nch";
#define MQTTPORT  1883 // 1883 or 1888
char thingsboardServer[] = "thingsboard.ogonan.com";           // 
char mqtt_server[] = "mqtt.ogonan.com";
char clientID[64] = "sensor/power/";
unsigned long nodeID = 104;


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
float temperature = 0.0;
int humidity = 0;
int key = 1; // switch on board
int buttonState = HIGH; //this variable tracks the state of the button, high if not pressed, low if pressed
int maintenanceState = -1; //this variable tracks the state of the Switch, negative if off, positive if on
bool exitState = true;

long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 250;    // the debounce time; increase if the output flickers

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 867076;
char readAPIKey[17] = "YBDBDH7FOD0NTLID";
char writeAPIKey[17] = "LU07OLP4TQ5XVPOH";

// const char* server = "mqtt.ogonan.com"; 
char mqttUserName[] = "kaebmoo";  // Can be any name.
char mqttPass[] = "sealwiththekiss";  // Change this your MQTT API Key from Account > MyProfile.
// String subscribeTopic = "channels/" + String( channelID ) + "/subscribe/fields/field3";
// String subscribeTopic = "channels/" + String( channelID ) + "/subscribe/fields/field5";
String subscribeTopic = "node/" + String( nodeID ) + "/control/messages";
String publishTopic = "node/" + String( nodeID ) + "/status/messages";

const unsigned long timerRepeat = 60; // second
const unsigned long alarmInterval = timerRepeat * 1000L;
const unsigned long postingInterval = 300L * 1000L;
long lastUpdateTime = 0; 
static const char alphanum[] ="0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.

char c_writeapikey[17] = "";    // write api key thingspeak
char c_readapikey[17] = "";     // read api key thingspeak
char c_token[21] = "";           // authen token blynk
char c_channelid[8] = "";       // channel id thingspeak
char c_nodeid[8] = "";          // node id mqtt

                              
WiFiClient client; 
PubSubClient mqttClient(client); 

AlarmId idAlarm;
Timer timer, timerCheckBattery, timerSwitch, timerMaintenance;
int idTimerSwitch, idTimerMaintenance;

Adafruit_ADS1015 ads1015;

void setup() 
{
  int saved;
  
  EEPROM.begin(512);
  
  // set the digital pin as output:
  pinMode(ledPin, OUTPUT);
  pinMode(KEY, INPUT);
  pinMode(POWER, INPUT);
  pinMode(BATT, INPUT);
  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1, HIGH);
  randomSeed(analogRead(A0));
  
  Serial.begin(115200);

  EEPROM.get(28, writeAPIKey); // 28 + 17
  EEPROM.get(45, readAPIKey);
  // EEPROM.get(62, auth);
  EEPROM.get(95, channelID);
  EEPROM.get(103, nodeID);
  EEPROM.get(112, TOKEN);
  EEPROM.get(500, saved);
  
  Serial.println();
  Serial.println("Configuration Read");
  Serial.print("Saved = ");
  Serial.println(saved);
  
  if (saved == 6550) {
    strcpy(c_writeapikey,  writeAPIKey);
    strcpy(c_readapikey, readAPIKey);
    strcpy(c_token, TOKEN);
    ltoa(channelID, c_channelid, 10); 
    ltoa(nodeID, c_nodeid, 10);
    

    const int n = snprintf(NULL, 0, "%lu", nodeID);
    assert(n > 0);
    char buf[n+1];
    int c = snprintf(buf, n+1, "%lu", nodeID);
    assert(buf[n] == '\0');
    assert(c == n);
    strcat(clientID, buf);

    Serial.print("Write API Key : ");
    Serial.println(writeAPIKey);
    Serial.print("Read API Key : ");
    Serial.println(readAPIKey);
    Serial.print("Channel ID : ");
    Serial.println(channelID);
    Serial.print("Node ID : ");
    Serial.println(nodeID);
    Serial.print("Device token : ");
    Serial.println(TOKEN);
    Serial.print("Client ID");
    Serial.println(clientID);
  }
  
  Serial.println();
  pixels.begin(); // This initializes the NeoPixel library.
  pixels.setBrightness(64);
  pixels.show(); // This sends the updated pixel color to the hardware.
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(255, 255, 0)); 
  pixels.show(); // This sends the updated pixel color to the hardware.

  // ads1015.begin();
  setupWifi();
  ThingSpeak.begin( client );
  #if defined(MQTT) || defined(THINGSBOARD)
  setupMqtt();
  #endif 
  checkPowerLine();
  checkBattery();
  #ifdef THINGSPEAK
  write2ThingSpeak();
  #endif
  timerCheckBattery.every(300000, checkBattery);
  // Serial.println("Delay Start 15 sec.");
  // delay(15000);
}

void loop() {
  // here is where you'd put code that needs to be running all the time.
  // key = digitalRead(KEY);
  readKey();
  timerMaintenance.update();
  if (maintenanceState > 0) {
    // MA Mode
    if (exitState == true) {
      idTimerMaintenance = timerMaintenance.after(60000, exitMaintenance);
      exitState = false;  
      Serial.println("timer maintenance started.");
    }
    
    if (digitalRead(POWER)) {
      pixels.clear();
      pixels.setPixelColor(0, pixels.Color(0, 255, 0)); 
      pixels.show(); // This sends the updated pixel color to the hardware.  
    }
    else {
      pixels.clear();
      pixels.setPixelColor(0, pixels.Color(255, 0, 0)); 
      pixels.show(); // This sends the updated pixel color to the hardware.  
    }
    return;
  }
  
  blink();

  checkPowerLine();
  
  Alarm.delay(200);
  timer.update();
  timerCheckBattery.update();
  timerSwitch.update();

  #if defined(MQTT) || defined(THINGSBOARD)
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
        idTimer = timer.after(alarmInterval, write2ThingSpeakAgain);
      }
      #endif
    }    
    
    if (millis() - lastUpdateTime >=  postingInterval) {
      lastUpdateTime = millis();
      #ifdef THINGSPEAK
      writeSuccess = write2ThingSpeak();
      if (writeSuccess != 200) {    
        idTimer = timer.after(random(3000, 7000), write2ThingSpeakAgain);
      }
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

void readKey()
{
  //sample the state of the button - is it pressed or not?
  buttonState = digitalRead(KEY);

  //filter out any noise by setting a time buffer
  if ( (millis() - lastDebounceTime) > debounceDelay) {

    //if the button has been pressed, lets toggle the LED from "off to on" or "on to off"
    if ( (buttonState == LOW) && (maintenanceState < 0) ) {

      // digitalWrite(ledPin, HIGH); //turn LED on
      maintenanceState = -maintenanceState; //now the LED is on, we need to change the state
      lastDebounceTime = millis(); //set the current time
    }
    else if ( (buttonState == LOW) && (maintenanceState > 0) ) {

      // digitalWrite(ledPin, LOW); //turn LED off
      maintenanceState = -maintenanceState; //now the LED is off, we need to change the state
      lastDebounceTime = millis(); //set the current time
    }//close if/else

  }//close if(time buffer)
}

void exitMaintenance()
{
  maintenanceState = -1;
  timerMaintenance.stop(idTimerMaintenance);
  exitState = true;
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
  int saved = 6550;

  wifiManager.setBreakAfterConfig(true);
  wifiManager.setConfigPortalTimeout(120);

  WiFiManagerParameter custom_c_writeapikey("c_writeapikey", "Write API Key : ThingSpeak", c_writeapikey, 17);
  WiFiManagerParameter custom_c_readapikey("c_readapikey", "Read API Key : ThingSpeak", c_readapikey, 17);
  WiFiManagerParameter custom_c_channelid("c_channelid", "Channel ID", c_channelid, 8);
  WiFiManagerParameter custom_c_nodeid("c_nodeid", "MQTT Node ID", c_nodeid, 8);
  WiFiManagerParameter custom_c_token("c_token", "ThingsBoard Token", c_token, 21);
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_c_writeapikey);
  wifiManager.addParameter(&custom_c_readapikey);
  wifiManager.addParameter(&custom_c_channelid);
  wifiManager.addParameter(&custom_c_nodeid);
  wifiManager.addParameter(&custom_c_token);

  String alias = "ogoPowerLine-"+String(ESP.getChipId());
  if (!wifiManager.autoConnect(alias.c_str(), "")) {
    Serial.println("failed to connect and hit timeout");
    Alarm.delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    Alarm.delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  Serial.print("Write API Key : ");
  Serial.println(writeAPIKey);
  Serial.print("Read API Key : ");
  Serial.println(readAPIKey);
  Serial.print("Channel ID : ");
  Serial.println(channelID);
  Serial.print("Node ID : ");
  Serial.println(nodeID);
  Serial.print("Device token : ");
  Serial.println(TOKEN);

  if (shouldSaveConfig) { //shouldSaveConfig
    Serial.println("Saving config...");
    strcpy(c_writeapikey, custom_c_writeapikey.getValue());
    strcpy(c_readapikey, custom_c_readapikey.getValue());
    strcpy(c_token, custom_c_token.getValue());
    strcpy(c_channelid, custom_c_channelid.getValue());
    strcpy(c_nodeid, custom_c_nodeid.getValue());

    strcpy(writeAPIKey, c_writeapikey);
    strcpy(readAPIKey, c_readapikey);
    strcpy(TOKEN, c_token);
    channelID = (unsigned long) atol(c_channelid);
    nodeID = (unsigned long) atol(c_nodeid);

    
    const int n = snprintf(NULL, 0, "%lu", nodeID);
    assert(n > 0);
    char buf[n+1];
    int c = snprintf(buf, n+1, "%lu", nodeID);
    assert(buf[n] == '\0');
    assert(c == n);
    strcat(clientID, buf);

    Serial.print("Write API Key : ");
    Serial.println(writeAPIKey);
    Serial.print("Read API Key : ");
    Serial.println(readAPIKey);
    Serial.print("Channel ID : ");
    Serial.println(channelID);
    Serial.print("Node ID : ");
    Serial.println(nodeID);
    Serial.print("Device token : ");
    Serial.println(TOKEN);
    Serial.print("Client ID : ");
    Serial.println(clientID);

    EEPROM.put(28, writeAPIKey);
    EEPROM.put(45, readAPIKey);
    EEPROM.put(95, channelID);
    EEPROM.put(103, nodeID);
    EEPROM.put(112, TOKEN);
    EEPROM.put(500, saved);
    
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    } else {
      Serial.println("ERROR! EEPROM commit failed");
    }

    shouldSaveConfig = false;
  }
  
}


int write2ThingSpeak()
{
  
  ThingSpeak.setField( 1, powerLine );
  ThingSpeak.setField( 2, batteryVoltage );
  ThingSpeak.setField( 5, (long ) nodeID );
  
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
    temperature = -1;
    humidity = -1;
  }
  #endif
  
  #ifdef THINGSBOARD
    Serial.println("Sending data to ThingsBoard");
    sendThingsBoard(powerLine);
  #endif
  
  Serial.println("Sending data to ThingSpeak");
  Serial.print("Power Line Status:");
  Serial.println(powerLine);
  
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
    idTimer = timer.after(alarmInterval, write2ThingSpeakAgain);
  }
  #endif THINGSPEAK
  
  Serial.println("Power Line Down : Trigger");
  idAlarm = Alarm.timerRepeat(timerRepeat, sendStatus);
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
    idTimer = timer.after(alarmInterval, write2ThingSpeakAgain);   
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

void setupMqtt() 
{

  #ifdef THINGSBOARD
  mqttClient.setServer(thingsboardServer, MQTTPORT);  // default port 1883, mqtt_server, thingsboardServer
  mqttClient.setCallback(callback);
  if (mqttClient.connect(clientID, TOKEN, NULL)) {
    mqttClient.subscribe("v1/devices/me/rpc/request/+");
    Serial.print("mqtt connected : ");    
    Serial.println(thingsboardServer);  // mqtt_server
    
  }
  #endif
  
  #ifdef MQTT
  Serial.print(mqtt_server); Serial.print(" "); Serial.println(MQTTPORT);
  mqttClient.setCallback(callback);
  mqttClient.setServer(mqtt_server, MQTTPORT);
  if (mqttClient.connect(clientID,mqttUserName,mqttPass)) {
    mqttClient.subscribe( subscribeTopic.c_str() );
    Serial.print("mqtt connected : ");
        Serial.println(mqtt_server);  // mqtt_server
  }
  #endif
}

void reconnect() 
{
  
  // Loop until reconnected.
  // while (!mqttClient.connected()) {
  // }
    Serial.print("Attempting MQTT connection...");

    /*
    // Generate ClientID
    char clientID[9];
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
      
      // Subscribing to receive RPC requests
      mqttClient.subscribe("v1/devices/me/rpc/request/+");
    }
    else {
      pixels.clear();
      pixels.setPixelColor(0, pixels.Color(255, 255, 0)); 
      pixels.show(); // This sends the updated pixel color to the hardware.
      Serial.print("failed, rc=");
      // Print to know why the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      Alarm.delay(2000);
    }
    
    #endif
    
    #ifdef MQTT
    mqttClient.setServer(mqtt_server, MQTTPORT);
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
      Serial.println(" try again in 2 seconds");
      Alarm.delay(2000);
    }
    #endif  
   
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  // mosquitto_pub -h mqtt.ogonan.com -t "/channels/867076/subscribe/messages" -u "user" -P "pass"  -m "off"
  // mosquitto_sub -h mqtt.ogonan.com -t "/channels/867076/publish/messages" -u "user" -P "pass"
  // mosquitto_sub -h mqtt.ogonan.com -t "/channels/867076/subscribe/messages" -u "user" -P "pass"
  #ifdef THINGSBOARD
  
  
  Serial.println("On message");
  
  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  String relayStatus;
  String responseTopic;
  
  // Check request method
  String methodName = String((const char*)data["method"]);
  String valueName = String((const char *) data["params"]);

  if (methodName.equals("turnOff")) 
  {
    // Update GPIO status and reply
    digitalWrite(RELAY1, RELAYOFF);
    
  }
  else if (methodName.equals("setRelay")) 
  {
    // Update GPIO status and reply
    if (valueName.equals("1")) {
      Serial.println("Turn On");
      digitalWrite(RELAY1, RELAYON);
    }
    else if (valueName.equals("0")) {
      Serial.println("Turn Off");
      digitalWrite(RELAY1, RELAYOFF);
    }
    else {
      Serial.println("Invalid value");
    }
    relayStatus = String(digitalRead(RELAY1) ? 0 : 1, DEC);
    responseTopic = String(topic);
    
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), relayStatus.c_str());
    String message = "{\"Relay Status\":" + relayStatus + "}";
    mqttClient.publish("v1/devices/me/telemetry",  message.c_str());
    Serial.print("topic : ");
    Serial.print("v1/devices/me/telemetry");
    Serial.print(" : message ");
    Serial.println(message);
  }
  else if (methodName.equals("getValue")) {
    relayStatus = String(digitalRead(RELAY1) ? 0 : 1, DEC);
    responseTopic = String(topic);
    
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), relayStatus.c_str());
    String message = "{\"Relay Status\":" + relayStatus + "}";
    mqttClient.publish("v1/devices/me/telemetry",  message.c_str());
    Serial.print("topic : ");
    Serial.print("v1/devices/me/telemetry");
    Serial.print(" : message ");
    Serial.println(message);
  }
  
  #endif

  #ifdef MQTT
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
    // idTimerSwitch = timerSwitch.after(10000, turnOn);
  }
  else if (!strncmp(p, "off", 3) || !strncmp(p, "0", 1) ) {
    digitalWrite(RELAY1, RELAYOFF);
  }

  String relayStatus = String(digitalRead(RELAY1), DEC);
  const char *msgBuffer;
  msgBuffer = relayStatus.c_str();
  Serial.print("Status Logic: ");
  Serial.println(msgBuffer);
  
  const char *topicBuffer;
  topicBuffer = publishTopic.c_str();
  mqttClient.publish( topicBuffer, msgBuffer );
  
  #endif
}

void turnOn()
{
  digitalWrite(RELAY1, RELAYON);
  // timerSwitch.stop(idTimerSwitch);
}

void sendThingsBoard(uint16_t power)
{
  // char buf[32];

  // ltoa(channelID, buf, 10);
  
  // Just debug messages
  Serial.print( "Sending power status : [" );
  Serial.print( power );
  Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"Power Line Detect\":"; payload += power; payload += ",";
  payload += "\"Node\":"; payload += nodeID; payload += ",";
  payload += "\"Battery\":"; payload += batteryVoltage; payload += ",";
  payload += "\"Temperature\":"; payload += temperature; payload += ",";
  payload += "\"Humidity\":"; payload += humidity;
  payload += "}";

  // Send payload
  char attributes[128];
  payload.toCharArray( attributes, 128 );
  mqttClient.publish( "v1/devices/me/telemetry", attributes );
  Serial.println( attributes );
  Serial.println();
}
