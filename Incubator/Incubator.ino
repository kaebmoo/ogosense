/*
  MIT License
Version 1.0 2019-03-19
Copyright (c) 2019 kaebmoo gmail com

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
*/

/*
 * Hardware
 * Wemos D1 mini, Pro                     https://wiki.wemos.cc/products:d1:d1_mini
 * Wemos SHT30 Shield use D1, D2 pin      https://wiki.wemos.cc/products:d1_mini_shields:sht30_shield
 * Wemos Relay Shield use D6              https://wiki.wemos.cc/products:d1_mini_shields:relay_shield
 * dot matrix LED // use D5, D7           https://wiki.wemos.cc/products:d1_mini_shields:matrix_led_shield
 * Lolin Motor Shield I2C use D1, D2 pin  https://wiki.wemos.cc/products:d1_mini_shields:motor_shield
 *
 *
 */

/* Comment this out to disable prints and save space */
// #define BLYNK_DEBUG // Optional, this enables lots of prints
// #define BLYNK_PRINT Serial

// #define SLEEP
#define MATRIXLED
// #define SOILMOISTURE
// #define EXTERNALSENSE

#define BLYNKLOCAL
// #define THINGSPEAK
// #define
// #define SECONDRELAY

#ifdef MATRIXLED
  #include <MLEDScroll.h>
  MLEDScroll matrix;
#endif

#define RELAYACTIVELOW

#include "ogoswitch.h"
#include <SPI.h>
#include <SD.h>

#include <ThingSpeak.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <BlynkSimpleEsp8266.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <WEMOS_SHT3X.h>
#include <TM1637Display.h>

#include <EEPROM.h>
#include <Timer.h>
#ifdef NETPIE
  #include <MicroGear.h>
#endif
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <Wire.h>
#include <LOLIN_I2C_MOTOR.h>

LOLIN_I2C_MOTOR motor; //I2C address 0x30
// LOLIN_I2C_MOTOR motor(DEFAULT_I2C_MOTOR_ADDRESS); //I2C address 0x30
// LOLIN_I2C_MOTOR motor(your_address); //using customize I2C address


const char* host = "ogosense-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "ogosense";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const int FW_VERSION = 10;  // 2018 11 13 version 10 fixed bug
const char* firmwareUrlBase = "http://www.ogonan.com/ogoupdate/";
#ifdef ARDUINO_ESP8266_WEMOS_D1MINI
  String firmware_name = "Incubator.ino.d1_mini";
#elif ARDUINO_ESP8266_WEMOS_D1MINILITE
  String firmware_name = "Incubator.ino.d1_minilite";
#elif ARDUINO_ESP8266_WEMOS_D1MINIPRO
  String firmware_name = "Incubator.ino.d1_minipro";
#endif

#ifdef RELAYACTIVELOW
  #define OFF  HIGH
  #define ON  LOW
#else
  #define OFF  LOW
  #define ON  HIGH
#endif

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 470917;
char *readAPIKey = "OVKF7VWLWY3VIST1";
char *writeAPIKey = "UMJJIQOM7ZTQCMJ5";

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "XXXXXXXXXXed4061bb4e0dXXXXXXXXXX";
bool blynkConnectedResult = false;
int blynkreconnect = 0;


WidgetLED led1(13); // On led RELAY1
WidgetLED led2(11); // Auto status
WidgetLED led3(12); // ON LED RELAY2

int gauge1Push_reset;
int gauge2Push_reset;

int ledStatus = LOW;                        // ledStatus used to set the LED
const int chipSelect = D8;                  // SD CARD


#if defined(MATRIXLED) && !defined(SECONDRELAY)
  // dot matrix LED use D5, D7 pin
  const int RELAY1 = D6;                      // ouput for control relay
#endif

#if defined(TM1637DISPLAY) && !defined(SECONDRELAY)
  const int RELAY1 = D7;                      // ouput for control relay // const int RELAY2   = D7;                   // ouput for second relay
  const int CLK = D6;                         // D6 Set the CLK pin connection to the display 7 segment
  const int DIO = D3;                         // Set the DIO pin connection to the display 7 segment
  TM1637Display display(CLK, DIO);            //set up the 4-Digit Display.
#endif


#ifdef SECONDRELAY && !defined(MATRIXLED)
const int RELAY1 = D7;
const int RELAY2 = D6;
// #else
// const int RELAY1 = D6;
#endif


const int buzzer=D5;                        // Buzzer control port, default D5
const int analogReadPin = A0;               // read for set options use R for voltage divide
const int LED = D4;                         // output for LED (toggle) buildin LED board




float maxtemp = TEMPERATURE_SETPOINT;
float mintemp = TEMPERATURE_SETPOINT - TEMPERATURE_RANGE;
float maxhumidity = HUMIDITY_SETPOINT;
float minhumidity = HUMIDITY_SETPOINT - HUMIDITY_RANGE;

float temperature_setpoint = TEMPERATURE_SETPOINT;  // 30.0 set point
float temperature_range = TEMPERATURE_RANGE;      // +- 4.0 from set point

float humidity_setpoint = HUMIDITY_SETPOINT;     // 60 set point RH %
float humidity_range = HUMIDITY_RANGE;          // +- 20 from set point

// set for wifimanager to get value from user
char t_setpoint[6] = "37.5";
char t_range[6] = "0.5";
char h_setpoint[6] = "60";
char h_range[6] = "5";
char c_options[6] = "1";
char c_cool[6] = "1";
char c_moisture[6] = "0";
char c_writeapikey[17] = "";    // write api key thingspeak
char c_readapikey[17] = "";     // read api key thingspeak
char c_auth[33] = "";           // authen token blynk
char c_channelid[8] = "";       // channel id thingspeak

// SHT30 -40 - 125 C ; 0.2-0.6 +-

int SAVE = 6550;            // Configuration save : if 6550 = saved
int COOL = 0;               // true > set point Cool mode, false < set point = HEAT mode
int MOISTURE = 0;           // true < set point moisture mode ; false > set point Dehumidifier mode
bool tempon = false;     // flag ON/OFF
bool humion = false;     // flag ON/OFF
bool AUTO = true;        // AUTO or Manual Mode ON/OFF relay, AUTO is depend on temperature, humidity; Manual is depend on Blynk command
int options = 1;            // option : 0 = humidity only, 1 = temperature only, 2 = temperature & humidiy


WiFiClient client;
SHT3X sht30(0x45);                          // address sensor use D1, D2 pin


const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;

const unsigned long onPeriod = 60L * 60L * 1000L;       // ON relay period minute * second * milli second
const unsigned long standbyPeriod = 1L * 1000L;       // delay start timer for relay

// flag for saving data
bool shouldSaveConfig = false;

BlynkTimer blynkTimer, checkConnectionTimer;
Timer t_relay, t_delayStart, t_readSensor, t_checkFirmware;         // timer for ON period and delay start
Timer t_relay2, t_delayStart2;
Timer timerMotor;
bool RelayEvent = false;
int afterStart = -1;
int afterStop = -1;
bool Relay2Event = false;
int afterStart2 = -1;
int afterStop2 = -1;
long motorDelayTime = 21600000;   // 6 hours 21600000 ms
bool flagToggleMotor = false;

#ifdef SLEEP
  // sleep for this many seconds
  const int sleepSeconds = 300;
#endif




void setup()
{
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  // WiFiManager wifiManager;
  // String APName;

  Serial.begin(115200);
  Serial.println();
  Serial.println("starting");

  Serial.println("Motor Shield Testing...");

  while (motor.PRODUCT_ID != PRODUCT_ID_I2C_MOTOR) //wait motor shield ready.
  {
    motor.getInfo();
  }

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(analogReadPin, INPUT);
  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1, OFF);
  #ifdef SECONDRELAY
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY2, OFF);
  #endif


  #ifdef TM1637DISPLAY
  display.setBrightness(0x0a); //set the diplay to maximum brightness
  #endif
  // options = analogRead(analogReadPin);
  // Serial.println(options);

  // put your setup code here, to run once:

  #ifdef MATRIXLED
    matrix.begin();
    matrix.setIntensity(1);
    #ifndef SLEEP
      matrix.message("Starting...", 50);
      while (matrix.scroll()!=SCROLL_ENDED) {
        yield();
      }
      matrix.clear();
    #endif
  #endif

  #ifdef NETPIE
    /* setup netpie call back */
    microgear.on(MESSAGE,onMsghandler);
    microgear.on(CONNECTED,onConnected);
    microgear.setEEPROMOffset(512);


    ALIAS = "ogosense-"+String(ESP.getChipId());
    Serial.print(me);
    Serial.print("\t");
    Serial.print(ALIAS);
    Serial.print("\t");
    Serial.println(relayStatus1);
    #ifdef SECONDRELAY
    Serial.println(relayStatus2);
    #endif
  #endif


  // read config from eeprom
  EEPROM.begin(512);
  humidity_setpoint = (float) eeGetInt(0);
  humidity_range = (float) eeGetInt(4);
  // temperature_setpoint = (float) EEPROMReadlong(8);
  // temperature_range = (float) EEPROMReadlong(12);
  EEPROM.get(8, temperature_setpoint);
  EEPROM.get(12, temperature_range);
  options = eeGetInt(16);
  COOL = eeGetInt(20);
  MOISTURE = eeGetInt(24);
  readEEPROM(writeAPIKey, 28, 16);
  readEEPROM(readAPIKey, 44, 16);
  readEEPROM(auth, 60, 32);
  channelID = (unsigned long) EEPROMReadlong(92);
  EEPROM.get(400, flagToggleMotor);

  Serial.println();
  Serial.println("Reading config.");
  Serial.print("Temperature : ");
  Serial.println(temperature_setpoint);
  Serial.print("Temperature Range : ");
  Serial.println(temperature_range);

  int saved = eeGetInt(500);
  if (saved == 6550) {
    dtostrf(humidity_setpoint, 2, 0, h_setpoint);
    dtostrf(humidity_range, 2, 0, h_range);
    dtostrf(temperature_setpoint, 2, 2, t_setpoint);
    dtostrf(temperature_range, 2, 2, t_range);
    itoa(options, c_options, 10);
    itoa(COOL, c_cool, 10);
    itoa(MOISTURE, c_moisture, 10);
    strcpy(c_writeapikey,  writeAPIKey);
    strcpy(c_readapikey, readAPIKey);
    strcpy(c_auth, auth);
    ltoa(channelID, c_channelid, 10);
  }
  autoWifiConnect();

  // web update OTA
  String host_update_name;
  host_update_name = "ogoswitch-"+String(ESP.getChipId());
  MDNS.begin(host_update_name.c_str());
  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and password '%s'\n", host_update_name.c_str(), update_path, update_username, update_password);

  Serial.print("blynk auth token: ");
  Serial.println(auth);
  #ifdef BLYNKLOCAL
    Blynk.config(auth, "blynk.ogonan.com", 80);
  #endif
  #ifdef BLYNK
    Blynk.config(auth);  // in place of Blynk.begin(auth, ssid, pass);
  #endif

  #if defined(BLYNKLOCAL) || defined(BLYNK)
    blynkConnectedResult = Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk, 3333 is 10 seconds because Blynk.connect is in 3ms units.
    Serial.print("Blynk connect : ");
    Serial.println(blynkConnectedResult);
    if (blynkConnectedResult) {
      Serial.println("Connected to Blynk server");
    }
    else {
      Serial.println("Not connected to Blynk server");
    }
  #endif

  #ifdef THINGSPEAK
  ThingSpeak.begin( client );
  #endif

  // microgear.useTLS(true);
  #ifdef NETPIE
    microgear.init(KEY,SECRET, (char *) ALIAS.c_str());
    microgear.connect(APPID);
  #endif


  // Setup a function to be called every second
  // gauge1Push_reset = blynkTimer.setInterval(4000L, displayTemperature);
  // gauge2Push_reset = blynkTimer.setInterval(4000L, displayHumidity);

  // ตั้งการส่งให้เหลื่อมกัน 2000ms
  // blynkTimer.setTimeout(2000, OnceOnlyTask1); // Guage V5 temperature
  // blynkTimer.setTimeout(4000, OnceOnlyTask2); // Guage v6 humidity



  digitalWrite(LED, HIGH);
  delay(500);
  digitalWrite(LED, LOW);
  buzzer_sound();


  #ifdef SOILMOISTURE
    blynkTimer.setInterval(5000, soilMoistureSensor);
  #endif


  timerMotor.every(motorDelayTime, fliptheEgg);
  t_readSensor.every(5000, readSensor);                       // read sensor data and make decision
  #ifdef THINGSPEAK
  blynkTimer.setInterval(60000L, sendThingSpeak);                   // send data to thingspeak
  #endif
  #if defined(BLYNKLOCAL) || defined(BLYNK)
  checkConnectionTimer.setInterval(60000L, checkBlynkConnection);   // check blynk connection
  #endif

  t_checkFirmware.every(86400000L, upintheAir);                     // check firmware update every 24 hrs
  upintheAir();

  #ifdef SLEEP
    #ifdef THINGSPEAK
    sendThingSpeak();
    #endif
    readSensor();
    displayHumidity();
    // checkBattery();
    Serial.println("I'm going to sleep.");
    delay(15000);
    Serial.println("Goodnight folks!");
    ESP.deepSleep(sleepSeconds * 1000000);
  #else
    displayTemperature();
  #endif
}

void loop() {

  httpServer.handleClient();
  blink();  // flash LED

  /** netpie connect **/
  #ifdef NETPIE
  if (microgear.connected()) {
    microgear.loop();
    Serial.println("publish to netpie");
    microgear.publish(relayStatus1, digitalRead(RELAY1), true);
    #ifdef SECONDRELAY
    microgear.publish(relayStatus2, digitalRead(RELAY2), true);
    #endif
  }
  else {
    Serial.println("connection lost, reconnect...");
    microgear.connect(APPID);
  }
  #endif

  #if defined(BLYNKLOCAL) || defined(BLYNK)
  Blynk.run();
  #endif

  blynkTimer.run();
  checkConnectionTimer.run();
  t_relay.update();
  t_delayStart.update();
  t_relay2.update();
  t_delayStart2.update();
  t_readSensor.update();
  t_checkFirmware.update();
  //t_displayTemperature.update();
  //t_displayHumidity.update();
  timerMotor.update();

}

void autoWifiConnect()
{
  WiFiManager wifiManager;
  String APName;

  wifiManager.setBreakAfterConfig(true);

  WiFiManagerParameter custom_t_setpoint("temperature", "temperature setpoint : 0-100", t_setpoint, 6);
  WiFiManagerParameter custom_t_range("t_range", "temperature range : 0-50", t_range, 6);
  WiFiManagerParameter custom_h_setpoint("humidity", "humidity setpoint : 0-100", h_setpoint, 6);
  WiFiManagerParameter custom_h_range("h_range", "humidity range : 0-50", h_range, 6);
  WiFiManagerParameter custom_c_options("c_options", "0,1,2,3 : 0-Humidity 1-Temperature 2-Both 3-Soil Moisture", c_options, 6);
  WiFiManagerParameter custom_c_cool("c_cool", "0,1 : 0-Heat 1-Cool", c_cool, 6);
  WiFiManagerParameter custom_c_moisture("c_moisture", "0,1 : 0-Dehumidifier 1-Moisture", c_moisture, 6);
  WiFiManagerParameter custom_c_writeapikey("c_writeapikey", "Write API Key : ThingSpeak", c_writeapikey, 17);
  WiFiManagerParameter custom_c_readapikey("c_readapikey", "Read API Key : ThingSpeak", c_readapikey, 17);
  WiFiManagerParameter custom_c_channelid("c_channelid", "Channel ID", c_channelid, 8);
  WiFiManagerParameter custom_c_auth("c_auth", "Blynk Auth Token", c_auth, 37);


  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_t_setpoint);
  wifiManager.addParameter(&custom_t_range);
  wifiManager.addParameter(&custom_h_setpoint);
  wifiManager.addParameter(&custom_h_range);
  wifiManager.addParameter(&custom_c_options);
  wifiManager.addParameter(&custom_c_cool);
  wifiManager.addParameter(&custom_c_moisture);
  wifiManager.addParameter(&custom_c_writeapikey);
  wifiManager.addParameter(&custom_c_readapikey);
  wifiManager.addParameter(&custom_c_channelid);
  wifiManager.addParameter(&custom_c_auth);

  //reset saved settings
  //wifiManager.resetSettings();

  //set custom ip for portal
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  //wifiManager.autoConnect("OgoSense");
  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  #ifdef SLEEP
  wifiManager.setTimeout(60);
  #else
  wifiManager.setTimeout(300);
  #endif


  APName = "ogoSense-"+String(ESP.getChipId());
  if(!wifiManager.autoConnect(APName.c_str()) ) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  if (temperature_setpoint > 100 || temperature_setpoint < 0) {
    temperature_setpoint = TEMPERATURE_SETPOINT;
    shouldSaveConfig = true;
  }
  if (temperature_range > 100 || temperature_range < 0) {
    temperature_range = TEMPERATURE_RANGE;
    shouldSaveConfig = true;
  }
  if (humidity_setpoint > 100 || humidity_setpoint < 0) {
    humidity_setpoint = HUMIDITY_SETPOINT;
    shouldSaveConfig = true;
  }
  if (humidity_range > 100 || humidity_range < 0) {
    humidity_range = HUMIDITY_RANGE;
    shouldSaveConfig = true;
  }
  if (options > 4 || options < 0) {
    options = OPTIONS;
    shouldSaveConfig = true;
  }
  if (COOL > 1 || COOL < 0) {
    COOL = COOL_MODE;
    shouldSaveConfig = true;
  }
  if (MOISTURE > 1 || MOISTURE < 0) {
    MOISTURE = MOISTURE_MODE;
    shouldSaveConfig = true;
  }

  Serial.println();
  Serial.println("Before save config.");
  Serial.print("Temperature : ");
  Serial.println(temperature_setpoint);
  Serial.print("Temperature Range : ");
  Serial.println(temperature_range);
  Serial.print("Humidity : ");
  Serial.println(humidity_setpoint);
  Serial.print("Humidity Range : ");
  Serial.println(humidity_range);
  Serial.print("Option : ");
  Serial.println(options);
  Serial.print("COOL : ");
  Serial.println(COOL);
  Serial.print("MOISTURE : ");
  Serial.println(MOISTURE);
  Serial.print("Write API Key : ");
  Serial.println(writeAPIKey);
  Serial.print("Read API Key : ");
  Serial.println(readAPIKey);
  Serial.print("Channel ID : ");
  Serial.println(channelID);
  Serial.print("auth token : ");
  Serial.println(auth);


  if (shouldSaveConfig) {
    Serial.println();
    Serial.println("Saving config...");
    strcpy(t_setpoint, custom_t_setpoint.getValue());
    strcpy(t_range, custom_t_range.getValue());
    strcpy(h_setpoint, custom_h_setpoint.getValue());
    strcpy(h_range, custom_h_range.getValue());
    strcpy(c_options, custom_c_options.getValue());
    strcpy(c_cool, custom_c_cool.getValue());
    strcpy(c_moisture, custom_c_moisture.getValue());
    strcpy(c_writeapikey, custom_c_writeapikey.getValue());
    strcpy(c_readapikey, custom_c_readapikey.getValue());
    strcpy(c_auth, custom_c_auth.getValue());
    strcpy(c_channelid, custom_c_channelid.getValue());

    temperature_setpoint = atof(t_setpoint);
    temperature_range = atof(t_range);
    humidity_setpoint = atol(h_setpoint);
    humidity_range = atol(h_range);
    options = atoi(c_options);
    COOL = atoi(c_cool);
    MOISTURE = atoi(c_moisture);
    strcpy(writeAPIKey, c_writeapikey);
    strcpy(readAPIKey, c_readapikey);
    strcpy(auth, c_auth);
    channelID = (unsigned long) atol(c_channelid);


    Serial.print("Temperature : ");
    Serial.println(temperature_setpoint);
    Serial.print("Temperature Range : ");
    Serial.println(temperature_range);
    Serial.print("Humidity : ");
    Serial.println(h_setpoint);
    Serial.print("Humidity Range : ");
    Serial.println(h_range);
    Serial.print("Option : ");
    Serial.println(c_options);
    Serial.print("COOL : ");
    Serial.println(COOL);
    Serial.print("MOISTURE : ");
    Serial.println(MOISTURE);
    Serial.print("Write API Key : ");
    Serial.println(writeAPIKey);
    Serial.print("Read API Key : ");
    Serial.println(readAPIKey);
    Serial.print("Channel ID : ");
    Serial.println(channelID);
    Serial.print("auth token : ");
    Serial.println(auth);

    eeWriteInt(0, atoi(h_setpoint));
    eeWriteInt(4, atoi(h_range));
    // EEPROMWritelong(8, t_setpoint);
    // EEPROMWritelong(12, t_range);
    EEPROM.put(8, temperature_setpoint);
    EEPROM.put(12, temperature_range);
    EEPROM.commit();
    eeWriteInt(16, options);
    eeWriteInt(20, COOL);
    eeWriteInt(24, MOISTURE);
    writeEEPROM(writeAPIKey, 28, 16);
    writeEEPROM(readAPIKey, 44, 16);
    writeEEPROM(auth, 60, 32);
    EEPROMWritelong(92, (long) channelID);
    eeWriteInt(500, 6550);
    shouldSaveConfig = false;
  }
}


void OnceOnlyTask1()
{
  blynkTimer.restartTimer(gauge1Push_reset);
}

void OnceOnlyTask2()
{
  blynkTimer.restartTimer(gauge2Push_reset);
}


float checkBattery()
{
  unsigned int raw = 0;
  float volt = 0.0;

  raw = analogRead(A0);
  volt = raw * (3.7 / 1023.0);
  // volt = volt * 4.2;
  Serial.print("Analog read: ");
  Serial.println(raw);

  // String v = String(volt);
  Serial.print("Battery voltage: ");
  Serial.println(volt);

  return(volt);
}

#ifdef SOILMOISTURE
int keepState = 0;
int minADC = 0;                       // replace with min ADC value read in air
int maxADC = 928;                     // replace with max ADC value read fully submerged in water
int mappedValue = 0;


void soilMoistureSensor()
{
  int _soilMoisture;

  _soilMoisture = analogRead(analogReadPin);
  Serial.print("Analog Read : ");
  Serial.println(_soilMoisture);
  mappedValue = map(soilMoisture, minADC, maxADC, 0, 100);
  // print mapped results to the serial monitor:
  Serial.print("Moisture value = " );
  Serial.println(mappedValue);

  if (mappedValue > soilMoistureLevel) {  // soilMoistureLevel define in ogoswitch.h, default = 50
    Serial.println("High Moisture");
    if (digitalRead(RELAY1) == ON) {
      Serial.println("Soil Moisture mode: Turn Relay Off");
      turnRelayOff();
      delay(300);
      Blynk.virtualWrite(V0, 0);
      // Blynk.syncVirtual(V0);
      RelayEvent = false;
    }
  }
  else {
    Serial.println("Low Moisture");
    if (digitalRead(RELAY1) == OFF) {
      Serial.println("Soil Moisture mode: Turn Relay On");
      turnRelayOn();
      delay(300);
      Blynk.virtualWrite(V0, 1);
      RelayEvent = true;
    }
  }
}
#endif

int humidity_sensor_value;
float temperature_sensor_value, fTemperature;

int getExternalSensor()
{

  return 0;
}

int getInternalSensor()
{
  if (sht30.get() == 0) {
    humidity_sensor_value = (int) sht30.humidity;
    temperature_sensor_value = sht30.cTemp;
    fTemperature = sht30.fTemp;

    return 0;
  }
  else {
    return 1;
  }

}


void readSensor()
{
  /*
   *  read data from temperature & humidity sensor
   *  set action by options
   *  options:
   *  4 = temperature, humidity
   *  3 = soil moisture
   *  2 = temperature & humidity
   *  1 = temperature
   *  0 = humidity
   *
  */

  int sensorStatus;

  #ifdef EXTERNALSENSE
  sensorStatus = getExternalSensor();
  #else
  sensorStatus = getInternalSensor();
  #endif

  Serial.printf("loop heap size: %u\n", ESP.getFreeHeap());
  if(AUTO) {
    Serial.print("\tOptions : ");
    Serial.println(options);

    if(sensorStatus == 0) {


      Serial.print("Temperature in Celsius : ");
      Serial.print(temperature_sensor_value);
      Serial.print("\tset point : ");
      Serial.print(temperature_setpoint);
      Serial.print("\trange : ");
      Serial.println(temperature_range);
      Serial.print("Temperature in Fahrenheit : ");
      Serial.println(fTemperature);
      Serial.print("Relative Humidity : ");
      Serial.print(humidity_sensor_value);
      Serial.print("\tset point : ");
      Serial.print(humidity_setpoint);
      Serial.print("\trange : ");
      Serial.println(humidity_range);



      if (MOISTURE == 1) {
        if (humidity_sensor_value < humidity_setpoint) {

          humion = true;
        }
        else if (humidity_sensor_value > (humidity_setpoint + humidity_range)) {
          humion = false;
        }
      }
      else if (MOISTURE == 0){
        if (humidity_sensor_value > humidity_setpoint) {
          humion = true;
        }
        else if (humidity_sensor_value < (humidity_setpoint - humidity_range)) {

          humion = false;
        }
      }

      if(COOL == 1) {
        if (temperature_sensor_value > temperature_setpoint) {

          tempon = true;
        }
        else if (temperature_sensor_value < (temperature_setpoint - temperature_range)) {

          tempon = false;
        }
      }
      else if (COOL == 0){
        if (temperature_sensor_value < temperature_setpoint) {

          tempon = true;
        }
        else if (temperature_sensor_value > (temperature_setpoint + temperature_range)) {

          tempon = false;
        }
      }

      if (options == 2) {
        Serial.println("Option: Temperature & Humidity");
        if (tempon == true && humion == true) {
          if (RelayEvent == false) {
            afterStart = t_relay.after(onPeriod, turnoff);
            Serial.println("Turn On.");
            RelayEvent = true;
            turnRelayOn();
          }
        }
        else if (tempon == false && humion == false) {
          if (afterStart != -1) {
            t_relay.stop(afterStart);
            afterStart = -1;
          }
          Serial.println("OFF");
          if (digitalRead(RELAY1) == ON) {
            turnRelayOff();
          }

          // delay start
          if (RelayEvent == true && afterStop == -1) {
              afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
              Serial.println("Timer Delay Start");
          }
        }
      }
      if (options == 1 || options == 4) {
        Serial.println("Option: Temperature");
        if (tempon == true) {
          if (options == 1) {
            if (RelayEvent == false) {
              afterStart = t_relay.after(onPeriod, turnoff);
              Serial.println("On Timer Relay #1 Start.");
              RelayEvent = true;
              turnRelayOn();
            }
          }
          else {
            if (Relay2Event == false) {
              afterStart2 = t_relay2.after(onPeriod, turnoffRelay2);
              Serial.println("On Timer Relay #2 Start.");
              Relay2Event = true;
              #ifdef SECONDRELAY
              turnRelay2On();
              #endif
            }
          }

        }
        else if (tempon == false) {
          if (options == 1) {
            if (afterStart != -1) {
              t_relay.stop(afterStart);
              afterStart = -1;
            }
            Serial.println("OFF");
            if (digitalRead(RELAY1) == ON) {
              turnRelayOff();
            }

            // delay start
            if (RelayEvent == true && afterStop == -1) {
                afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
                Serial.println("Timer Delay Relay #1 Start");
            }
          }
          else {
            if (afterStart2 != -1) {
              t_relay2.stop(afterStart2);
              afterStart2 = -1;
            }
            Serial.println("OFF");
            #ifdef SECONDRELAY
            if (digitalRead(RELAY2) == ON) {

              turnRelay2Off();

            }
            #endif

            // delay start
            if (Relay2Event == true && afterStop2 == -1) {
                afterStop2 = t_delayStart2.after(standbyPeriod, delayStart2);   // 10 * 60 * 1000 = 10 minutes
                Serial.println("Timer Delay Relay #2 Start");
            }
          }
        }
      }
      if (options == 0 || options == 4) {
        Serial.println("Option: Humidity");
        if (humion == true) {
          if (RelayEvent == false) {
            afterStart = t_relay.after(onPeriod, turnoff);
            Serial.println("On Timer Start.");
            RelayEvent = true;
            turnRelayOn();
          }
        }
        else if (humion == false) {
          if (afterStart != -1) {
            t_relay.stop(afterStart);
            afterStart = -1;
          }
          Serial.println("OFF");
          if (digitalRead(RELAY1) == ON) {
            turnRelayOff();
          }

          // delay start
          if (RelayEvent == true && afterStop == -1) {
              afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
              Serial.println("Timer Delay Start");
          }
        }
      }

      if (options == 3) {
        #ifdef SOILMOISTURE
        soilMoistureSensor();
        Serial.println("Soil Moisture Mode");
        #endif
      }


      Serial.print("tempon = ");
      Serial.print(tempon);
      Serial.print(" humion = ");
      Serial.print(humion);
      Serial.print(" RelayEvent = ");
      Serial.print(RelayEvent);
      Serial.print(" afterStart = ");
      Serial.print(afterStart);
      Serial.print(" afterStop = ");
      Serial.println(afterStop);
      Serial.println();

      Serial.print("Relay #1 Status ");
      bool value1 = (0!=(*portOutputRegister( digitalPinToPort(RELAY1) ) & digitalPinToBitMask(RELAY1)));
      Serial.println(value1);
      #ifdef SECONDRELAY
      Serial.print("Relay #2 Status ");
      bool value2 = (0!=(*portOutputRegister( digitalPinToPort(RELAY2) ) & digitalPinToBitMask(RELAY2)));
      Serial.println(value2);
      #endif

    } // if sensor OK
    else
    {
      Serial.println("Sensor Error!");
    }
  } // if AUTO

  if (ledStatus == LOW) {
    ledStatus = HIGH;
    displayTemperature();
  }
  else {
    ledStatus = LOW;
    displayHumidity();
  }
}

void upintheAir()
{
  String fwURL = String( firmwareUrlBase );
  fwURL.concat( firmware_name );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" );

  Serial.println( "Checking for firmware updates." );
  // Serial.print( "MAC address: " );
  // Serial.println( mac );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );

  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();

    Serial.print( "Current firmware version: " );
    Serial.println( FW_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if( newVersion > FW_VERSION ) {
      Serial.println( "Preparing to update" );

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  }
  httpClient.end();
  // ESPhttpUpdate.update("www.ogonan.com", 80, "/ogoupdate/ogoswitch_blynk.ino.d1_mini.bin");
}


void displayHumidity()
{
  float tempdisplay;
  uint8_t data[] = { 0x00, 0x00, 0x00, 0x76 };  // 0x76 = H
  static char outstring[8];

  sht30.get();
  #ifdef TM1637DISPLAY
  display.setSegments(data);
  tempdisplay = sht30.humidity * 10;
  display.showNumberDecEx(tempdisplay, (0x80 >> 2), true, 3, 0);
  #endif

  #ifdef MATRIXLED
  // dtostrf(sht30.humidity, 4, 2, outstring);
  memset(outstring, '\0', sizeof(outstring));
  strcpy(outstring, String(sht30.humidity, 0).c_str());
  strcat(outstring, "%");
  matrix.message(outstring, 100);
  while (matrix.scroll()!=SCROLL_ENDED) {
    yield();
  }
  matrix.clear();
  #endif
}



void displayTemperature()
{
  float tempdisplay;
  uint8_t data[] = { 0x00, 0x00, 0x00, 0x39 };  // //  gfedcba 00111001 = 0011 1001 = 0x39 = C
  static char outstring[8];

  sht30.get();
  #ifdef TM1637DISPLAY
  display.setSegments(data);
  tempdisplay = sht30.cTemp * 10;
  display.showNumberDecEx(tempdisplay, (0x80 >> 2), true, 3, 0);
  #endif

    #ifdef MATRIXLED
    // dtostrf(sht30.cTemp, 4, 2, outstring);
    memset(outstring, '\0', sizeof(outstring));
    strcpy(outstring, String(sht30.cTemp, 1).c_str());
    strcat(outstring, "C");
    matrix.message(outstring, 100);
    while (matrix.scroll()!=SCROLL_ENDED) {
      yield();
    }
    matrix.clear();
    #endif
}

void turnRelayOn()
{
  digitalWrite(RELAY1, ON);
  Serial.println("RELAY1 ON");
  digitalWrite(LED_BUILTIN, LOW);  // turn on
  led1.on();
  Blynk.virtualWrite(V0, 1);
  buzzer_sound();
}

void turnRelayOff()
{
  digitalWrite(RELAY1, OFF);
  Serial.println("RELAY1 OFF");
  digitalWrite(LED_BUILTIN, HIGH);  // turn off
  led1.off();
  Blynk.virtualWrite(V0, 0);
  buzzer_sound();
}

#ifdef SECONDRELAY
void turnRelay2On()
{
  digitalWrite(RELAY2, ON);
  Serial.println("RELAY2 ON");
  digitalWrite(LED_BUILTIN, LOW);  // turn on
  led3.on();
  Blynk.virtualWrite(V3, 1);
  buzzer_sound();
}

void turnRelay2Off()
{
  digitalWrite(RELAY2, OFF);
  Serial.println("RELAY2 OFF");
  digitalWrite(LED_BUILTIN, HIGH);  // turn off
  led3.off();
  Blynk.virtualWrite(V3, 0);
  buzzer_sound();
}
#endif

void turnoff()
{
  afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
  t_relay.stop(afterStart);
  if (standbyPeriod >= 5000) {
    turnRelayOff();
    Serial.println("Timer Stop: RELAY1 OFF");
  }
  afterStart = -1;
}

void delayStart()
{
  t_delayStart.stop(afterStop);
  RelayEvent = false;
  afterStop = -1;
  Serial.println("Timer Delay Relay #1 End.");
}

void turnoffRelay2()
{
  afterStop2 = t_delayStart2.after(standbyPeriod, delayStart2);   // 10 * 60 * 1000 = 10 minutes
  t_relay2.stop(afterStart2);
  if (standbyPeriod >= 5000) {
    // turnrelay_onoff(LOW);
    #ifdef SECONDRELAY
    turnRelay2Off();
    #endif
    Serial.println("Timer Stop: RELAY2 OFF");
  }
  afterStart2 = -1;
}

void delayStart2()
{
  t_delayStart2.stop(afterStop2);
  Relay2Event = false;
  afterStop2 = -1;
  Serial.println("Timer Delay Relay #2 End.");
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void eeWriteInt(int pos, int val) {
    byte* p = (byte*) &val;
    EEPROM.write(pos, *p);
    EEPROM.write(pos + 1, *(p + 1));
    EEPROM.write(pos + 2, *(p + 2));
    EEPROM.write(pos + 3, *(p + 3));
    EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}

void EEPROMWritelong(int address, long value)
{
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  Serial.print(four);
  Serial.print(" ");
  Serial.print(three);
  Serial.print(" ");
  Serial.print(two);
  Serial.print(" ");
  Serial.print(one);
  Serial.println();

  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
  EEPROM.commit();
}

long EEPROMReadlong(int address)
{
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  Serial.print(four);
  Serial.print(" ");
  Serial.print(three);
  Serial.print(" ");
  Serial.print(two);
  Serial.print(" ");
  Serial.print(one);
  Serial.println();

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void readEEPROM(char* buff, int offset, int len) {
    int i;
    for (i=0;i<len;i++) {
        buff[i] = (char)EEPROM.read(offset+i);
    }
    buff[len] = '\0';
}

void writeEEPROM(char* buff, int offset, int len) {
    int i;
    for (i=0;i<len;i++) {
        EEPROM.write(offset+i,buff[i]);
    }
    EEPROM.commit();
}


int init_sdcard()
{
  unsigned long dataSize;

  Serial.println("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return -1;
  }
  Serial.println("Card initialized.");

  File dataFile = SD.open("datalog.txt");
  if (dataFile) {
    // check file size if > 1GB delete it
    Serial.println("Checking file size.");
    dataSize = dataFile.size();
    if (dataSize > 1000000000) {
      Serial.println("file size > 1GB, deleting.");
      dataFile.close();
      SD.remove("datalog.txt");
      Serial.println("file deleted.");
    }

  }

  return 0;

}

void write_datalogger(String dataString) {


  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}

void fliptheEgg()
{
  motor.changeFreq(MOTOR_CH_BOTH, 1000); //Change A & B 's Frequency to 1000Hz.
  motor.changeDuty(MOTOR_CH_BOTH, 100);

  Serial.println();
  Serial.println("MOTOR_STATUS_STOP");
  motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_STOP);
  motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_STOP);
  delay(500);

  if (flagToggleMotor == false) {
    // motor CW
    Serial.println("MOTOR_STATUS_CW");
    motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CW);
    motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CCW);
    motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CW);
    motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CCW);
    motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CW);
    motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CCW);
    delay(500);

  }
  else {
    // motor CCW
    Serial.println("MOTOR_STATUS_CCW");
    motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CCW);
    motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CW);
    motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CCW);
    motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CW);
    motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CCW);
    motor.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CW);
    delay(500);

  }
  Serial.println();
  Serial.print("Flag toggle motor: ");
  Serial.println(flagToggleMotor);
  flagToggleMotor = !flagToggleMotor;

  Serial.print("Flag toggle motor (after flip): ");
  Serial.println(flagToggleMotor);
  EEPROM.put(400, flagToggleMotor);
  EEPROM.commit();
}

void blink()
{
  unsigned long currentMillis = millis();

  // if enough millis have elapsed
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // toggle the LED
    ledState = !ledState;
    digitalWrite(LED, ledState);
  }
}

#ifdef THINGSPEAK
void sendThingSpeak()
{
  unsigned int freeheap = ESP.getFreeHeap();
  float fahrenheitTemperature = 0;
  float celsiusTemperature = 0;
  float rhHumidity = 0;


    if(sht30.get()==0) {
      fahrenheitTemperature = sht30.fTemp;
      celsiusTemperature = sht30.cTemp;
      rhHumidity = sht30.humidity;

      Serial.print(celsiusTemperature);
      Serial.print(", ");
      Serial.print(fahrenheitTemperature);
      Serial.print(", ");
      Serial.print(rhHumidity);
      Serial.println();
      Serial.print("Sending data to ThingSpeak : ");
    }

    ThingSpeak.setField( 1, celsiusTemperature );
    ThingSpeak.setField( 2, rhHumidity );
    ThingSpeak.setField( 3, digitalRead(RELAY1));
    ThingSpeak.setField( 4, (float) freeheap );
    ThingSpeak.setField( 5, blynkConnectedResult);
    #ifdef SLEEP
      float volt = checkBattery();
      ThingSpeak.setField(6, volt);
    #endif

    int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
    Serial.println(writeSuccess);
    Serial.println();
}
#endif

#ifdef NETPIE
void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen)
{
    Serial.print("Incoming message --> ");
    msg[msglen] = '\0';
    Serial.println((char *)msg);

    /*
    if(strcmp(topic, me) == 0) {
      if ((char)msg[0] == '1') {

      }
    }
    */
}


void onConnected(char *attribute, uint8_t* msg, unsigned int msglen)
{
    Serial.println("Connected to NETPIE...");
    microgear.setAlias((char *) ALIAS.c_str());
    microgear.subscribe(me);
}
#endif

BLYNK_WRITE(V0)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V0 to a variable

  // process received value
  Serial.print("Pin Value : ");
  Serial.println(pinValue);
  if (!AUTO) {
    if (pinValue == 1) {
      turnRelayOn();
      RelayEvent = true;
    }
    else {
      turnRelayOff();
      if (afterStart != -1) {
            t_relay.stop(afterStart);

      }
      if (afterStop != -1) {
        t_delayStart.stop(afterStop);
      }

      RelayEvent = false;
      afterStart = -1;
      afterStop = -1;

    }
  }
  else {
    Serial.println("auto mode!");
    if(RelayEvent) {
      Blynk.virtualWrite(V0, 1);
    }
    else {
      Blynk.virtualWrite(V0, 0);
    }
  }

  Serial.print(" RelayEvent = ");
  Serial.print(RelayEvent);
  Serial.print(" afterStart = ");
  Serial.print(afterStart);
  Serial.print(" afterStop = ");
  Serial.println(afterStop);

}

BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable

  if(!AUTO) {
    if (pinValue == 1) {
      Blynk.virtualWrite(V1, 1);
    }
    else {
      Blynk.virtualWrite(V1, 0);
    }
  }
  else {
    Serial.println("auto mode!");
    if(RelayEvent) {
      Blynk.virtualWrite(V1, 1);
    }
    else {
      Blynk.virtualWrite(V1, 0);
    }
  }
}

BLYNK_WRITE(V2)
{
  int pinValue = param.asInt();
  if (pinValue == 1) {
    AUTO = true;
    led2.on();
    Serial.print("AUTO Mode : ");
    Serial.println(AUTO);
  }
  else {
    AUTO = false;
    led2.off();
    Serial.print("AUTO Mode : ");
    Serial.println(AUTO);
  }
}

BLYNK_WRITE(V3)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V3 to a variable

  // process received value
  Serial.print("Pin Value : ");
  Serial.println(pinValue);
  if (!AUTO) {
    if (pinValue == 1) {
      #ifdef SECONDRELAY
      turnRelay2On();
      #endif
      Relay2Event = true;
    }
    else {
      #ifdef SECONDRELAY
      turnRelay2Off();
      #endif
      if (afterStart2 != -1) {
        t_relay2.stop(afterStart2);
      }
      if (afterStop2 != -1) {
        t_delayStart2.stop(afterStop2);
      }

      Relay2Event = false;
      afterStart2 = -1;
      afterStop2 = -1;

    }
  }
  else {
    Serial.println("auto mode!");
    if(Relay2Event) {
      Blynk.virtualWrite(V3, 1);
    }
    else {
      Blynk.virtualWrite(V3, 0);
    }
  }

  Serial.print(" RelayEvent = ");
  Serial.print(Relay2Event);
  Serial.print(" afterStart = ");
  Serial.print(afterStart2);
  Serial.print(" afterStop = ");
  Serial.println(afterStop2);

}

BLYNK_WRITE(V19)
{
  switch (param.asInt())
  {
    case 1: // Item 1
      Serial.println("Item 1 selected (humidity)");
      options = 0;
      Blynk.virtualWrite(V22, 0);   // backward compatible
      break;
    case 2: // Item 2
      Serial.println("Item 2 selected (temperature)");
      options = 1;
      Blynk.virtualWrite(V22, 1);   // backward compatible
      break;
    case 3: // Item 3
      Serial.println("Item 3 selected (temperature & humidity)");
      options = 2;
      Blynk.virtualWrite(V22, 2);   // backward compatible
      break;
    case 4: // Item 4
      Serial.println("Item 4 selected (soil moisture)");
      options = 3;
      Blynk.virtualWrite(V22, 3);   // backward compatible
      break;
    case 5: // Item 5
      Serial.println("Item 5 selected (temperature, humidity)");
      options = 4;
      Blynk.virtualWrite(V22, 4);   // backward compatible
      break;
    default:
      Serial.println("Unknown item selected");
  }
}

BLYNK_WRITE(V20)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V20 to a variable

  COOL = pinValue;
  Serial.print("Set cool: ");
  Serial.println(COOL);
  if (COOL == 1) {
    Blynk.setProperty(V20, "color", BLYNK_BLUE);
  }
  else if (COOL == 0) {
    Blynk.setProperty(V20, "color", BLYNK_RED);
  }
}

BLYNK_WRITE(V21)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V21 to a variable

  MOISTURE = pinValue;
  Serial.print("Set moisture: ");
  Serial.println(MOISTURE);
}

BLYNK_WRITE(V22)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V22 to a variable

  options = pinValue;
  Serial.print("Set option: ");
  Serial.println(options);
}

BLYNK_WRITE(V69)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V23 to a variable

  if (pinValue == 1) {
    delay(500);
    ESP.reset();
    delay(5000);
  }
}

BLYNK_WRITE(V24)
{
  float stepperValue = param.asFloat();

  Serial.print("Temperature setpoint: ");
  Serial.println(stepperValue);
  temperature_setpoint = stepperValue;

}

BLYNK_WRITE(V25)
{
  float stepperValue = param.asFloat();

  Serial.print("Temperature range: ");
  Serial.println(stepperValue);
  temperature_range = stepperValue;
}

BLYNK_WRITE(V26)
{
  int stepperValue = param.asInt();

  Serial.print("Humidity setpoint: ");
  Serial.println(stepperValue);
  humidity_setpoint = stepperValue;
}

BLYNK_WRITE(V27)
{
  int stepperValue = param.asInt();

  Serial.print("Humidity setpoint: ");
  Serial.println(stepperValue);
  humidity_range = stepperValue;
}

BLYNK_READ(V5)
{
  float t = sht30.cTemp;
  char str[5] = "";

  if (isnan(t)) {
    Serial.println("Failed to read from sensor!");
    return;
  }
  dtostrf(t, 4, 1, str);
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V5, str);

}

BLYNK_READ(V6)
{
  float h = sht30.humidity;

  if (isnan(h)) {
    Serial.println("Failed to read from sensor!");
    return;
  }
  Blynk.virtualWrite(V6, (int) h);

}

BLYNK_CONNECTED()
{
  bool relay1_status, relay2_status;

  Serial.println("Blynk Sync.");
  // Blynk.syncAll();

  relay1_status = (0!=(*portOutputRegister( digitalPinToPort(RELAY1) ) & digitalPinToBitMask(RELAY1)));
  Blynk.virtualWrite(V0, relay1_status);
  Serial.print("Relay #1 status = "); Serial.println(relay1_status);

  #ifdef SECONDRELAY
  relay2_status = (0!=(*portOutputRegister( digitalPinToPort(RELAY2) ) & digitalPinToBitMask(RELAY2)));
  Blynk.virtualWrite(V3, relay2_status);
  Serial.print("Relay #2 status = "); Serial.println(relay2_status);
  #endif



  if (relay1_status == 1) {
    led1.on();
  }
  else {
    led1.off();
  }
  if(relay2_status == 1) {
    led3.on();
  }
  else {
    led3.off();
  }
  if (AUTO) {
    Blynk.virtualWrite(V2, 1);
    led2.on();
  }
  else {
    Blynk.virtualWrite(V2, 0);
    led2.off();
    Blynk.syncVirtual(V0);
    Blynk.syncVirtual(V3);
  }
  Blynk.syncVirtual(V19);
  Blynk.syncVirtual(V20);
  Blynk.syncVirtual(V21);
  Blynk.syncVirtual(V22);
  Blynk.syncVirtual(V23);
  Blynk.syncVirtual(V24);
  Blynk.syncVirtual(V25);
  Blynk.syncVirtual(V26);
  Blynk.syncVirtual(V27);
}

// This function sends Arduino's up time every second to Virtual Pin (5).
// In the app, Widget's reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.
void sendSensorT()
{
  float t = sht30.cTemp;
  char str[5] = "";

  if (isnan(t)) {
    Serial.println("Failed to read from sensor!");
    return;
  }
  dtostrf(t, 4, 1, str);
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V5, str);

}

void sendSensorH()
{
  float h = sht30.humidity;

  if (isnan(h)) {
    Serial.println("Failed to read from sensor!");
    return;
  }
  Blynk.virtualWrite(V6, (int) h);
}


void checkBlynkConnection() {
  int mytimeout;

  Serial.println("Check Blynk connection.");
  blynkConnectedResult = Blynk.connected();
  if (!blynkConnectedResult) {
    Serial.println("Blynk not connected");
    mytimeout = millis() / 1000;
    Serial.println("Blynk trying to reconnect.");
    while (!blynkConnectedResult) {
      blynkConnectedResult = Blynk.connect(3333);
      Serial.print(".");
      if((millis() / 1000) > mytimeout + 3) { // try for less than 4 seconds
        Serial.println("Blynk reconnect timeout.");
        break;
      }
    }
  }
  if (blynkConnectedResult) {
      Serial.println("Blynk connected");
  }
  else {
    Serial.println("Blynk not connected");
    Serial.print("blynkreconnect: ");
    Serial.println(blynkreconnect);
    blynkreconnect++;
    if (blynkreconnect >= 10) {
      // delay(60000);
      // ESP.reset();
      blynkreconnect = 0;
    }
  }
}

void buzzer_sound()
{
  analogWriteRange(1047);
  analogWrite(buzzer, 512);
  delay(100);
  analogWrite(buzzer, 0);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  delay(100);

  analogWriteRange(1175);
  analogWrite(buzzer, 512);
  delay(300);
  analogWrite(buzzer, 0);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  delay(300);
}
