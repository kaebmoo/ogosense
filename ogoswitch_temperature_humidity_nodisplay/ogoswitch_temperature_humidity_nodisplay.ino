/*
  MIT License
Version 1.0 2018-01-22
Copyright (c) 2017 kaebmoo gmail com

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
 * Wemos D1 mini, Pro
 * SHT30 Shield
 * Relay Shield
 *
 *
 */

/* Comment this out to disable prints and save space */
// #define BLYNK_DEBUG // Optional, this enables lots of prints
// #define BLYNK_PRINT Serial

// #define SLEEP
// #define MATRIXLED
// #define SOILMOISTURE
#define BLYNKLOCAL
// #define BLYNK

#ifdef MATRIXLED
#include <MLEDScroll.h>

MLEDScroll matrix;
#endif

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

#include "ogoswitch.h"

const char* host = "ogosense-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "ogosense";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const int FW_VERSION = 9;
const char* firmwareUrlBase = "http://www.ogonan.com/ogoupdate/";
#ifdef ARDUINO_ESP8266_WEMOS_D1MINI
  String firmware_name = "ogoswitch_temperature_humidity_nodisplay.ino.d1_mini";
#elif ARDUINO_ESP8266_WEMOS_D1MINILITE
  String firmware_name = "ogoswitch_temperature_humidity_nodisplay.ino.d1_minilite";
#elif ARDUINO_ESP8266_WEMOS_D1MINIPRO
  String firmware_name = "ogoswitch_temperature_humidity_nodisplay.ino.d1_minipro";
#endif



// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 360709;
char *readAPIKey = "GNZ8WEU763Z5DUEA";
char *writeAPIKey = "8M07EYX8NPCD9V8U";

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "";
bool blynkConnectedResult = false;
int blynkreconnect = 0;


WidgetLED led1(10); // On led
WidgetLED led2(11); // Auto status

int gauge1Push_reset;
int gauge2Push_reset;

int ledStatus = LOW;                        // ledStatus used to set the LED
const int chipSelect = D8;                  // SD CARD

#ifdef MATRIXLED
  const int RELAY1 = D6;                      // ouput for control relay
#else
  const int RELAY1 = D7;                      // ouput for control relay // const int RELAY2 = D7;                   // ouput for second relay
  const int CLK = D6;                         // D6 Set the CLK pin connection to the display 7 segment
  const int DIO = D3;                         // Set the DIO pin connection to the display 7 segment
  TM1637Display display(CLK, DIO);            //set up the 4-Digit Display.
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
char t_setpoint[6] = "30";
char t_range[6] = "2";
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
int COOL = 1;               // true > set point Cool mode, false < set point = HEAT mode
int MOISTURE = 0;           // true < set point moisture mode ; false > set point Dehumidifier mode
boolean tempon = false;     // flag ON/OFF
boolean humion = false;     // flag ON/OFF
boolean AUTO = true;        // AUTO or Manual Mode ON/OFF relay, AUTO is depend on temperature, humidity; Manual is depend on Blynk command
int options = 0;            // option : 0 = humidity only, 1 = temperature only, 2 = temperature & humidiy


WiFiClient client;
SHT3X sht30(0x45);                          // address sensor


const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;

const unsigned long onPeriod = 60L * 60L * 1000L;       // ON relay period minute * second * milli second
const unsigned long standbyPeriod = 300L * 1000L;       // delay start timer for relay

// flag for saving data
bool shouldSaveConfig = false;

BlynkTimer blynkTimer, checkConnectionTimer;
Timer t_relay, t_delayStart, t_readSensor, t_checkFirmware;         // timer for ON period and delay start
bool RelayEvent = false;
int afterStart = -1;
int afterStop = -1;

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

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(analogReadPin, INPUT);
  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1, LOW);

  #ifndef MATRIXLED
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
  Serial.println(mystatus);
  #endif


  // read config from eeprom
  EEPROM.begin(512);
  humidity_setpoint = (float) eeGetInt(0);
  humidity_range = (float) eeGetInt(4);
  temperature_setpoint = (float) eeGetInt(8);
  temperature_range = (float) eeGetInt(12);
  options = eeGetInt(16);
  COOL = eeGetInt(20);
  MOISTURE = eeGetInt(24);
  readEEPROM(writeAPIKey, 28, 16);
  readEEPROM(readAPIKey, 44, 16);
  readEEPROM(auth, 60, 32);
  channelID = (unsigned long) EEPROMReadlong(92);

  int saved = eeGetInt(500);
  if (saved == 6550) {
    dtostrf(humidity_setpoint, 2, 0, h_setpoint);
    dtostrf(humidity_range, 2, 0, h_range);
    dtostrf(temperature_setpoint, 2, 0, t_setpoint);
    dtostrf(temperature_range, 2, 0, t_range);
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

  ThingSpeak.begin( client );

  // microgear.useTLS(true);
  #ifdef NETPIE
  microgear.init(KEY,SECRET, (char *) ALIAS.c_str());
  microgear.connect(APPID);
  #endif

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



  t_readSensor.every(5000, readSensor);                       // read sensor data and make decision
  blynkTimer.setInterval(60000L, sendThingSpeak);                   // send data to thingspeak
  #if defined(BLYNKLOCAL) || defined(BLYNK)
  checkConnectionTimer.setInterval(60000L, checkBlynkConnection);   // check blynk connection
  #endif
  t_checkFirmware.every(86400000L, upintheAir);                     // check firmware update every 24 hrs
  upintheAir();

  #ifdef SLEEP
  sendThingSpeak();
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


  // sendThingSpeak();

  /** netpie connect **/
  #ifdef NETPIE
  if (microgear.connected()) {
    microgear.loop();
    Serial.println("publish to netpie");
    microgear.publish(mystatus, digitalRead(RELAY1), true);
  }
  else {
    Serial.println("connection lost, reconnect...");
    microgear.connect(APPID);
  }
  #endif

  #if defined(BLYNKLOCAL) || defined(BLYNK)
  if (Blynk.connected()) {
    Blynk.run();
  }
  #endif

  blynkTimer.run();
  checkConnectionTimer.run();
  t_relay.update();
  t_delayStart.update();
  t_readSensor.update();
  t_checkFirmware.update();
  //t_displayTemperature.update();
  //t_displayHumidity.update();

}

void autoWifiConnect()
{
  WiFiManager wifiManager;
  String APName;

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
  if (options > 2 || options < 0) {
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

    temperature_setpoint = atol(t_setpoint);
    temperature_range = atol(t_range);
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
    Serial.println(t_setpoint);
    Serial.print("Temperature Range : ");
    Serial.println(t_range);
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
    eeWriteInt(8, atoi(t_setpoint));
    eeWriteInt(12, atoi(t_range));
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

void soilMoistureSensor()
{
  int _soilMoisture;

  _soilMoisture = analogRead(analogReadPin);
  Serial.print("Analog Read : ");
  Serial.println(_soilMoisture);

  if (_soilMoisture > soilMoistureLevel) {  // soilMoistureLevel define in ogoswitch.h, default = 500
    Serial.println("High Moisture");
    if (digitalRead(RELAY1) == LOW) {
      Serial.println("Soil Moisture: Turn Relay On");
      // turnrelay_onoff(HIGH);
      turnRelayOn();
      delay(300);
      Blynk.virtualWrite(V1, 1);
      // Blynk.syncVirtual(V1);
      RelayEvent = true;
    }
  }
  else {
    Serial.println("Low Moisture");
    if (digitalRead(RELAY1) == HIGH) {
      Serial.println("Soil Moisture: Turn Relay Off");
      // turnrelay_onoff(LOW);
      turnRelayOff();
      delay(300);
      Blynk.virtualWrite(V1, 0);
      RelayEvent = false;
    }
  }
}
#endif

void readSensor()
{
  /*
   *  read data from temperature & humidity sensor
   *  set action by options
   *  options:
   *  3 = soil moisture
   *  2 = temperature & humidity
   *  1 = temperature
   *  0 = humidity
   *
  */
  int humidity_sensor_value;

  Serial.printf("loop heap size: %u\n", ESP.getFreeHeap());
  if(AUTO) {
    Serial.print("\tOptions : ");
    Serial.println(options);

    if(sht30.get()==0){
      Serial.print("Temperature in Celsius : ");
      Serial.print(sht30.cTemp);
      Serial.print("\tset point : ");
      Serial.print(temperature_setpoint);
      Serial.print("\trange : ");
      Serial.println(temperature_range);
      Serial.print("Temperature in Fahrenheit : ");
      Serial.println(sht30.fTemp);
      Serial.print("Relative Humidity : ");
      Serial.print(sht30.humidity);
      Serial.print("\tset point : ");
      Serial.print(humidity_setpoint);
      Serial.print("\trange : ");
      Serial.println(humidity_range);

      humidity_sensor_value = (int) sht30.humidity;

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
        if (sht30.cTemp > temperature_setpoint) {

          tempon = true;
        }
        else if (sht30.cTemp < (temperature_setpoint - temperature_range)) {

          tempon = false;
        }
      }
      else if (COOL == 0){
        if (sht30.cTemp < temperature_setpoint) {

          tempon = true;
        }
        else if (sht30.cTemp > (temperature_setpoint + temperature_range)) {

          tempon = false;
        }
      }

      if (options == 2) {
        Serial.println("Option: Temperature & Humidity");
        if (tempon == true && humion == true) {
          if (RelayEvent == false) {
            afterStart = t_relay.after(onPeriod, turnoff);
            Serial.println("On Timer Start.");
            RelayEvent = true;
            // turnrelay_onoff(HIGH);
            turnRelayOn();
          }
        }
        else if (tempon == false && humion == false) {
          if (afterStart != -1) {
            t_relay.stop(afterStart);
            afterStart = -1;
          }
          Serial.println("OFF");
          if (digitalRead(RELAY1) == HIGH) {
            // turnrelay_onoff(LOW);
            turnRelayOff();
          }

          // delay start
          if (RelayEvent == true && afterStop == -1) {
              afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
              Serial.println("Timer Delay Start");
          }
        }
      }
      else if (options == 1) {
        Serial.println("Option: Temperature");
        if (tempon == true) {
          if (RelayEvent == false) {
            afterStart = t_relay.after(onPeriod, turnoff);
            Serial.println("On Timer Start.");
            RelayEvent = true;
            // turnrelay_onoff(HIGH);
            turnRelayOn();
          }
        }
        else if (tempon == false) {
          if (afterStart != -1) {
            t_relay.stop(afterStart);
            afterStart = -1;
          }
          Serial.println("OFF");
          if (digitalRead(RELAY1) == HIGH) {
            // turnrelay_onoff(LOW);
            turnRelayOff();
          }

          // delay start
          if (RelayEvent == true && afterStop == -1) {
              afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
              Serial.println("Timer Delay Start");
          }

        }
      }
      else if (options == 0) {
        Serial.println("Option: Humidity");
        if (humion == true) {
          if (RelayEvent == false) {
            afterStart = t_relay.after(onPeriod, turnoff);
            Serial.println("On Timer Start.");
            RelayEvent = true;
            // turnrelay_onoff(HIGH);
            turnRelayOn();
          }
        }
        else if (humion == false) {
          if (afterStart != -1) {
            t_relay.stop(afterStart);
            afterStart = -1;
          }
          Serial.println("OFF");
          if (digitalRead(RELAY1) == HIGH) {
            // turnrelay_onoff(LOW);
            turnRelayOff();
          }

          // delay start
          if (RelayEvent == true && afterStop == -1) {
              afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
              Serial.println("Timer Delay Start");
          }
        }
      }
      else if (options == 3) {
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



    }
    else
    {
      Serial.println("Sensor Error!");
    }
  }
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
  #ifndef MATRIXLED
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
  #ifndef MATRIXLED
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
  digitalWrite(RELAY1, HIGH);
  Serial.println("RELAY1 ON");
  digitalWrite(LED_BUILTIN, LOW);  // turn on
  led1.on();
  Blynk.virtualWrite(V1, 1);
  buzzer_sound();
}

void turnRelayOff()
{
  digitalWrite(RELAY1, LOW);
  Serial.println("RELAY1 OFF");
  digitalWrite(LED_BUILTIN, HIGH);  // turn off
  led1.off();
  Blynk.virtualWrite(V1, 0);
  buzzer_sound();
}

/*
void turnrelay_onoff(uint8_t value)
{
    if (value == HIGH) {
      digitalWrite(RELAY1, HIGH);
      Serial.println("RELAY1 ON");
      digitalWrite(LED_BUILTIN, LOW);  // turn on
      led1.on();
      Blynk.virtualWrite(V1, 1);
      buzzer_sound();
    }
    else if (value == LOW) {
      digitalWrite(RELAY1, LOW);
      Serial.println("RELAY1 OFF");
      digitalWrite(LED_BUILTIN, HIGH);  // turn off
      led1.off();
      Blynk.virtualWrite(V1, 0);
      buzzer_sound();
    }
}
*/

void turnoff()
{
  afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
  if (standbyPeriod >= 5000) {
    // turnrelay_onoff(LOW);
    turnRelayOff();
    Serial.println("Timer Stop: RELAY1 OFF");
  }
  afterStart = -1;
}

void delayStart()
{
  RelayEvent = false;
  afterStop = -1;
  Serial.println("Timer Delay Start End.");
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
      volt = checkBattery();
      ThingSpeak.setField(6, volt);
    #endif

    int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
    Serial.println(writeSuccess);
    Serial.println();
}

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

BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable

  // process received value
  Serial.print("Pin Value : ");
  Serial.println(pinValue);
  if (!AUTO) {
    if (pinValue == 1) {
      // turnrelay_onoff(HIGH);
      turnRelayOn();
      RelayEvent = true;
    }
    else {
      // turnrelay_onoff(LOW);
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
  }

  Serial.print(" RelayEvent = ");
  Serial.print(RelayEvent);
  Serial.print(" afterStart = ");
  Serial.print(afterStart);
  Serial.print(" afterStop = ");
  Serial.println(afterStop);

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
  int stepperValue = param.asInt();

  Serial.print("Temperature setpoint: ");
  Serial.println(stepperValue);
  temperature_setpoint = stepperValue;

}

BLYNK_WRITE(V25)
{
  int stepperValue = param.asInt();

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
  Serial.println("Blynk Connected");
  // Blynk.syncAll();

  int relay_status = digitalRead(RELAY1);
  Blynk.virtualWrite(V1, relay_status);
  if (relay_status == 1) {
    led1.on();
  }
  else {
    led1.off();
  }
  if (AUTO) {
    Blynk.virtualWrite(V2, 1);
    led2.on();
  }
  else {
    Blynk.virtualWrite(V2, 0);
    led2.off();
    Blynk.syncVirtual(V1);
  }
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
