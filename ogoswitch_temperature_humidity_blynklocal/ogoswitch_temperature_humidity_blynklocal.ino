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

/* Comment this out to disable prints and save space */
//#define BLYNK_DEBUG // Optional, this enables lots of prints
//#define BLYNK_PRINT Serial


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
#include <MicroGear.h>

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
const int FW_VERSION = 5;
const char* firmwareUrlBase = "http://www.ogonan.com/ogoupdate/";
String firmware_name = "ogoswitch_temperature_humidity_blynklocal.ino.d1_mini"; // ogoswitch_temperature_humidity_nodisplay.ino.d1_mini


#define APPID   "OgoSense"                  // application id from netpie
#define KEY     "sYZknE19LHxr1zJ"           // key from netpie
#define SECRET  "wJOErv6EcU365pnBMpcFLDzcZ" // secret from netpie

String ALIAS = "ogosense-0000";              // alias name netpie
char *me = "/ogosense/0000000";                  // topic set for sensor box
char *mystatus = "/ogosense/0000000/status";     // topic status "1" or "0", "ON" or "OFF"

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 360709;
char *readAPIKey = "GNZ8WEU763Z5DUEA";
char *writeAPIKey = "8M07EYX8NPCD9V8U";
const unsigned long postingInterval = 60L * 1000L;        // 60 seconds


long lastUpdateTime = 0;

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "27092c0fc50343bc917a97c755012c9b";

WidgetLED led1(10); // On led
WidgetLED led2(11); // Auto status


int gauge1Push_reset;
int gauge2Push_reset;

int ledStatus = LOW;             // ledStatus used to set the LED
const int chipSelect = D8; // SD CARD
const int CLK = D6; //Set the CLK pin connection to the display
const int DIO = D3; //Set the DIO pin connection to the display

const int buzzer=D5; //Buzzer control port, default D5
const int analogReadPin = A0;               // read for set options use R for voltage divide
const int RELAY1 = D7;                      // ouput for control relay
const int LED = D4;                         // output for LED (toggle) buildin LED board
// const int RELAY2 = D7;                      // ouput for second relay

TM1637Display display(CLK, DIO); //set up the 4-Digit Display.
SHT3X sht30(0x45);                          // address sensor

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

int SAVE = 6550;      // Configuration save : if 6550 = saved
int COOL = 1;        // true > set point Cool mode, false < set point = HEAT mode
int MOISTURE = 0;   // true < set point moisture mode ; false > set point Dehumidifier mode
boolean tempon = false;     // flag ON/OFF
boolean humion = false;     // flag ON/OFF
boolean AUTO = true;       // AUTO or Manual Mode ON/OFF relay, AUTO is depend on temperature, humidity; Manual is depend on Blynk command

int options = 0;            // option : 0 = humidity only, 1 = temperature only, 2 = temperature & humidiy


WiFiClient client;
MicroGear microgear(client);

const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;

const unsigned long onPeriod = 60L * 60L * 1000L;     // ON relay period minute * second * milli second
const unsigned long standbyPeriod = 60L * 1000L;      // delay start timer for relay

//flag for saving data
bool shouldSaveConfig = false;

BlynkTimer blynktimer, checkConnectionTimer;
Timer t_relay, t_delayStart, timer_readsensor, checkFirmware;         // timer for ON period and delay start
bool RelayEvent = false;
int afterStart = -1;
int afterStop = -1;


bool blynkConnectedResult = false;
int blynkreconnect = 0;

const int MAXRETRY=4; // 0 - 4
#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"

#define DEBOUNCE 10  // button debouncer, how many ms to debounce, 5+ ms is usually plenty


byte buttons[] = {D0};  // switch
// This handy macro lets us determine how big the array up above is, by checking the size
#define NUMBUTTONS sizeof(buttons)

// we will track if a button is just pressed, just released, or 'currently pressed'
byte pressed[NUMBUTTONS], justpressed[NUMBUTTONS], justreleased[NUMBUTTONS];
byte previous_keystate[NUMBUTTONS], current_keystate[NUMBUTTONS];


void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("starting");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(analogReadPin, INPUT);
  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1, LOW);

  display.setBrightness(0x0a); //set the diplay to maximum brightness
  // options = analogRead(analogReadPin);
  // Serial.println(options);

  // put your setup code here, to run once:

  #ifdef NETPIE
  /* setup netpie call back */
  microgear.on(MESSAGE,onMsghandler);
  microgear.on(CONNECTED,onConnected);
  microgear.setEEPROMOffset(512);
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

    // WiFi.begin();
    auto_wifi_connect();
    Serial.println(WiFi.SSID().c_str());
    Serial.println(WiFi.psk().c_str());
  }
  else {
    ondemand_wifi_setup();
  }

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

  Blynk.config(auth, "ogoservice.ogonan.com", 80);  // in place of Blynk.begin(auth, ssid, pass);
  blynkConnectedResult = Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk, 3333 is 10 seconds because Blynk.connect is in 3ms units.
  Serial.print("Blynk connect : ");
  Serial.println(blynkConnectedResult);
  if (blynkConnectedResult) {
    Serial.println("Connected to Blynk server");
  }
  else {
    Serial.println("Not connected to Blynk server");
  }

  // Setup a function to be called every second
  // gauge1Push_reset = blynktimer.setInterval(300000L, sendSensorT);
  // gauge2Push_reset = blynktimer.setInterval(300000L, sendSensorH);

  // ตั้งการส่งให้เหลื่อมกัน 150ms
  // blynktimer.setTimeout(150, OnceOnlyTask1); // Guage V5 temperature
  // blynktimer.setTimeout(300, OnceOnlyTask2); // Guage v6 humidity


  digitalWrite(LED, HIGH);
  delay(500);
  digitalWrite(LED, LOW);
  buzzer_sound();

  timer_readsensor.every(5000, temp_humi_sensor);
  checkConnectionTimer.setInterval(300000L, checkBlynkConnection);
  checkFirmware.every(86400000L, upintheair);
  upintheair();

}

void loop() {

  httpServer.handleClient();
  blink();

  // read data from sensor and action by condition in options, COOL, MOISTURE
  // temp_humi_sensor() every 1 sec.

  sendThingSpeak();
  /*
   * netpie connect
   *
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
  */

  t_relay.update();
  t_delayStart.update();
  timer_readsensor.update();
  checkFirmware.update();

  if (Blynk.connected()) {
    Blynk.run();
  }
  // blynktimer.run();


  checkConnectionTimer.run();

  byte thisSwitch=thisSwitch_justPressed();
  switch(thisSwitch)
  {
    case 0:
      ondemand_wifi_setup();
      ESP.reset();
      break;
  }
  delay(100);
}

void auto_wifi_connect()
{
  WiFiManager wifiManager;
  String APName;

  WiFiManagerParameter custom_t_setpoint("temperature", "temperature setpoint : 0-100", t_setpoint, 6);
  WiFiManagerParameter custom_t_range("t_range", "temperature range : 0-50", t_range, 6);
  WiFiManagerParameter custom_h_setpoint("humidity", "humidity setpoint : 0-100", h_setpoint, 6);
  WiFiManagerParameter custom_h_range("h_range", "humidity range : 0-50", h_range, 6);
  WiFiManagerParameter custom_c_options("c_options", "0,1,2 : 0-Humidity 1-Temperature 2-Both", c_options, 6);
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

  wifiManager.setTimeout(300);
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

  ALIAS = "ogosense-"+String(ESP.getChipId());
  Serial.print(me);
  Serial.print("\t");
  Serial.print(ALIAS);
  Serial.print("\t");
  Serial.println(mystatus);

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

void ondemand_wifi_setup()
{
  WiFiManager wifiManager;

  Serial.println("On demand AP");
  if (!wifiManager.startConfigPortal("ogosense")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

}

void OnceOnlyTask1()
{
  blynktimer.restartTimer(gauge1Push_reset);
}

void OnceOnlyTask2()
{
  blynktimer.restartTimer(gauge2Push_reset);
}

void temp_humi_sensor()
{
  int humidity_sensor_value;

  // Debug
  Serial.printf("loop heap size: %u\n", ESP.getFreeHeap());

  if(AUTO) {


    /*
    options = analogRead(analogReadPin);
    Serial.print("Analog Read : ");
    Serial.print(options);

    if (options < 100)
      options = 0;  // humidity
     else if (options > 100 && options < 200)
      options = 1;  // temperature
     else
      options = 2;  // temperature && humidity
    */

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
            turnrelay_onoff(HIGH);

          }
        }
        else if (tempon == false && humion == false) {
          if (afterStart != -1) {
            t_relay.stop(afterStart);
            afterStart = -1;
          }
          Serial.println("OFF");
          if (digitalRead(RELAY1) == HIGH) {
            turnrelay_onoff(LOW);
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
            turnrelay_onoff(HIGH);

          }
        }
        else if (tempon == false) {
          if (afterStart != -1) {
            t_relay.stop(afterStart);
            afterStart = -1;
          }
          Serial.println("OFF");
          if (digitalRead(RELAY1) == HIGH) {
            turnrelay_onoff(LOW);
          }

          // delay start
          if (RelayEvent == true && afterStop == -1) {
              afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
              Serial.println("Timer Delay Start");
          }

        }
      }
      else {
        Serial.println("Option: Humidity");
        if (humion == true) {
          if (RelayEvent == false) {
            afterStart = t_relay.after(onPeriod, turnoff);
            Serial.println("On Timer Start.");
            RelayEvent = true;
            turnrelay_onoff(HIGH);

          }
        }
        else if (humion == false) {
          if (afterStart != -1) {
            t_relay.stop(afterStart);
            afterStart = -1;
          }
          Serial.println("OFF");
          if (digitalRead(RELAY1) == HIGH) {
            turnrelay_onoff(LOW);
          }

          // delay start
          if (RelayEvent == true && afterStop == -1) {
              afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
              Serial.println("Timer Delay Start");
          }
        }
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
}

void upintheair()
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

void segment_display()
{
    float tempdisplay = sht30.cTemp * 10;

    //  gfedcba
    // 00111001 = 0011 1001 = 0x39 = C

    uint8_t data[] = { 0x00, 0x00, 0x00, 0x39 };
    display.setSegments(data);
    display.showNumberDecEx(tempdisplay, (0x80 >> 2), true, 3, 0);
    delay(2000);

    // 0x76 = H
    data[0] = 0x00;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 0x76;
    display.setSegments(data);
    tempdisplay = sht30.humidity * 10;
    display.showNumberDecEx(tempdisplay, (0x80 >> 2), true, 3, 0);
    delay(2000);
}

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

void turnoff()
{
  afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
  turnrelay_onoff(LOW);
  Serial.println("Timer Stop: RELAY1 OFF");
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


  // Only update if posting time is exceeded
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdateTime >=  postingInterval) {
    lastUpdateTime = currentMillis;
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
    ThingSpeak.setField( 3, digitalRead(RELAY1) );
    ThingSpeak.setField( 4, (float) freeheap );
    ThingSpeak.setField( 5, blynkConnectedResult);
    int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
    Serial.println(writeSuccess);
    Serial.println();
  }
}

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

BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable

  // process received value
  Serial.print("Pin Value : ");
  Serial.println(pinValue);
  if (!AUTO) {
    if (pinValue == 1) {
      turnrelay_onoff(HIGH);
      RelayEvent = true;
    }
    else {
      turnrelay_onoff(LOW);
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

BLYNK_WRITE(V23)
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
    Blynk.config(auth, "ogoservice.ogonan.com", 80);
    mytimeout = millis() / 1000;
    Serial.println("Blynk trying to reconnect.");
    while (!blynkConnectedResult) {
      blynkConnectedResult = Blynk.connect(3333);
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
    blynkreconnect++;
    Serial.print("blynkreconnect: ");
    Serial.println(blynkreconnect);
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


byte thisSwitch_justPressed() {
  byte thisSwitch = 255;
  check_switches();  //check the switches &amp; get the current state
  for (byte i = 0; i < NUMBUTTONS; i++) {
    current_keystate[i]=justpressed[i];
    if (current_keystate[i] != previous_keystate[i]) {
      if (current_keystate[i]) thisSwitch=i;
    }
    previous_keystate[i]=current_keystate[i];
  }
  return thisSwitch;
}

void check_switches()
{
  static byte previousstate[NUMBUTTONS];
  static byte currentstate[NUMBUTTONS];
  static long lasttime;
  byte index;
  if (millis() < lasttime) {
     lasttime = millis(); // we wrapped around, lets just try again
  }

  if ((lasttime + DEBOUNCE) > millis()) {
    return; // not enough time has passed to debounce
  }
  // ok we have waited DEBOUNCE milliseconds, lets reset the timer
  lasttime = millis();

  for (index = 0; index < NUMBUTTONS; index++) {
    justpressed[index] = 0;       // when we start, we clear out the "just" indicators
    justreleased[index] = 0;

    currentstate[index] = digitalRead(buttons[index]);   // read the button
    if (currentstate[index] == previousstate[index]) {
      if ((pressed[index] == LOW) && (currentstate[index] == LOW)) {
          // just pressed
          justpressed[index] = 1;
      }
      else if ((pressed[index] == HIGH) && (currentstate[index] == HIGH)) {
          // just released
          justreleased[index] = 1;
      }
      pressed[index] = !currentstate[index];  // remember, digital HIGH means NOT pressed
    }
    //Serial.println(pressed[index], DEC);
    previousstate[index] = currentstate[index];   // keep a running tally of the buttons
  }
}
