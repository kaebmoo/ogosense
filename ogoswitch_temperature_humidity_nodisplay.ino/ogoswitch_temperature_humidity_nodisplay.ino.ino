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

#define APPID   "OgoSense"                  // application id from netpie
#define KEY     "sYZknE19LHxr1zJ"           // key from netpie
#define SECRET  "wJOErv6EcU365pnBMpcFLDzcZ" // secret from netpie

String ALIAS = "OgoSense0000";              // alias name netpie
char *me = "/mam/0000000";                  // topic set for sensor box
char *mystatus = "/mam/0000000/status";     // topic status "1" or "0", "ON" or "OFF"

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 360709;
char *readAPIKey = "GNZ8WEU763Z5DUEA";
char *writeAPIKey = "8M07EYX8NPCD9V8U";
const unsigned long postingInterval = 60L * 1000L;        // 60 seconds

unsigned int dataFieldFour = 4;                            // Field to write temperature C data
unsigned int dataFieldFive = 5;
unsigned int dataFieldOne = 1;
unsigned int dataFieldTwo = 2;                       // Field to write relative humidity data
unsigned int dataFieldThree = 3;                     // Field to write temperature F data
unsigned long lastConnectionTime = 0;
long lastUpdateTime = 0;

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "27092c0fc50343bc917a97c755012c9b";

WidgetLED led1(10); //virtual led 
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

float maxtemp = 30.0;
float mintemp = 26.0;
float maxhumidity = 60.0;
float minhumidity = 40.0;

float temperature_setpoint = 30.0;  // 30.0 set point
float temperature_range = 4.0;      // +- 4.0 from set point

float humidity_setpoint = 60.0;     // 60 set point RH %
float humidity_range = 20;          // +- 20 from set point

// set for wifimanager to get value from user
char t_setpoint[6] = "30";
char t_range[6] = "4";
char h_setpoint[6] = "60";
char h_range[6] = "20";
char c_options[6] = "1";
char c_cool[6] = "1";
char c_moisture[6] = "0";

// SHT30 -40 - 125 C ; 0.2-0.6 +-

int COOL = 1;        // true > set point, false < set point = HEAT mode
int MOISTURE = 0;   // true < set point; false > set point
boolean tempon = false;     // flag ON/OFF
boolean humion = false;     // flag ON/OFF
boolean AUTO = false;       // AUTO or Manual Mode ON/OFF relay, AUTO is depend on temperature, humidity; Manual is depend on Blynk command

int options = 0;            // option : 0 = humidity only, 1 = temperature only, 2 = temperature & humidiy


WiFiClient client;
MicroGear microgear(client);

const long interval = 2000;
int ledState = LOW;
unsigned long previousMillis = 0;

const unsigned long onPeriod = 60L * 60L * 1000L;     // ON relay period minute * second * milli second
const unsigned long standbyPeriod = 60L * 1000L;      // delay start timer for relay

//flag for saving data
bool shouldSaveConfig = false;

Timer t_relay, t_delayStart, timer_delaysend;         // timer for ON period and delay start
bool RelayEvent = false;
int afterStart = -1;
int afterStop = -1;

bool send2thingspeak = false;

const int MAXRETRY=4; // 0 - 4

void setup() {

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(analogReadPin, INPUT);
  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1, LOW);
  display.setBrightness(0x0a); //set the diplay to maximum brightness
  Serial.println("starting");
  options = analogRead(analogReadPin);
  Serial.println(options);

  // put your setup code here, to run once:

  /* setup netpie call back */
  microgear.on(MESSAGE,onMsghandler);
  microgear.on(CONNECTED,onConnected);
  microgear.setEEPROMOffset(48);

  Serial.begin(115200);

  // read config from eeprom
  EEPROM.begin(512);
  humidity_setpoint = (float) eeGetInt(0);
  humidity_range = (float) eeGetInt(4);
  temperature_setpoint = (float) eeGetInt(8);
  temperature_range = (float) eeGetInt(12);
  options = eeGetInt(16);
  COOL = eeGetInt(20);
  MOISTURE = eeGetInt(24);

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

  if (temperature_setpoint > 100 || temperature_setpoint < 0) {
    temperature_setpoint = 30;
    shouldSaveConfig = true;
  }
  if (temperature_range > 100 || temperature_range < 0) {
    temperature_range = 4;
    shouldSaveConfig = true;
  }
  if (humidity_setpoint > 100 || humidity_setpoint < 0) {
    humidity_setpoint = 60;
    shouldSaveConfig = true;
  }
  if (humidity_range > 100 || humidity_range < 0) {
    humidity_range = 20;
    shouldSaveConfig = true;
  }
  if (options > 2 || options < 0) {
    options = 1;
    shouldSaveConfig = true;
  }
  if (COOL > 1 || COOL < 0) {
    COOL = 1;
    shouldSaveConfig = true;
  }
  if (MOISTURE > 1 || MOISTURE < 0) {
    MOISTURE = 0;
    shouldSaveConfig = true;
  }


  WiFiManagerParameter custom_t_setpoint("temperature", "temperature setpoint", t_setpoint, 6);
  WiFiManagerParameter custom_t_range("t_range", "temperature range", t_range, 6);
  WiFiManagerParameter custom_h_setpoint("humidity", "humidity setpoint", h_setpoint, 6);
  WiFiManagerParameter custom_h_range("h_range", "humidity range", h_range, 6);
  WiFiManagerParameter custom_c_options("c_options", "OPTION", c_options, 6);
  WiFiManagerParameter custom_c_cool("c_cool", "COOL", c_cool, 6);
  WiFiManagerParameter custom_c_moisture("c_moisture", "MOISTURE", c_moisture, 6);


    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    String APName;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    wifiManager.addParameter(&custom_t_setpoint);
    wifiManager.addParameter(&custom_t_range);
    wifiManager.addParameter(&custom_h_setpoint);
    wifiManager.addParameter(&custom_h_range);
    wifiManager.addParameter(&custom_c_options);
    wifiManager.addParameter(&custom_c_cool);
    wifiManager.addParameter(&custom_c_moisture);


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
    APName = "OgoSense-"+String(ESP.getChipId());
    if(!wifiManager.autoConnect(APName.c_str()) ) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }


    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

    ALIAS = "OgoSense-"+String(ESP.getChipId()); 
    Serial.print(me);
    Serial.print("\t");
    Serial.print(ALIAS);
    Serial.print("\t");
    Serial.println(mystatus);

    if (shouldSaveConfig) {
      strcpy(t_setpoint, custom_t_setpoint.getValue());
      strcpy(t_range, custom_t_range.getValue());
      strcpy(h_setpoint, custom_h_setpoint.getValue());
      strcpy(h_range, custom_h_range.getValue());
      strcpy(c_options, custom_c_options.getValue());
      strcpy(c_cool, custom_c_cool.getValue());
      strcpy(c_moisture, custom_c_moisture.getValue());

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

      temperature_setpoint = atol(t_setpoint);
      temperature_range = atol(t_range);
      humidity_setpoint = atol(h_setpoint);
      humidity_range = atol(h_range);
      options = atoi(c_options);
      COOL = atoi(c_cool);
      MOISTURE = atoi(c_moisture);

      eeWriteInt(0, atoi(h_setpoint));
      eeWriteInt(4, atoi(h_range));
      eeWriteInt(8, atoi(t_setpoint));
      eeWriteInt(12, atoi(t_range));
      eeWriteInt(16, options);
      eeWriteInt(20, COOL);
      eeWriteInt(24, MOISTURE);
    }

    ThingSpeak.begin( client );

    // microgear.useTLS(true);
    // microgear.init(KEY,SECRET, (char *) ALIAS.c_str());
    // microgear.connect(APPID);

    Blynk.config(auth);  // in place of Blynk.begin(auth, ssid, pass);
    Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk, 3333 is 10 seconds because Blynk.connect is in 3ms units.
}

void loop() {

  blink();

  if (AUTO) {
    temp_humi_sensor();
  }
  
  /*
   * netpie connect
   * 
  if (microgear.connected()) {
    microgear.loop();
    Serial.println("publish to netpie");
    microgear.publish(mystatus, digitalRead(RELAY1), true);
  }
  else {
    Serial.println("connection lost, reconnect...");
    microgear.connect(APPID);
  }
  */
  
  Serial.println();

  t_relay.update();
  t_delayStart.update();
  timer_delaysend.update();

  Blynk.run();

}

void temp_humi_sensor()
{
  
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


    if (MOISTURE) {
      if (sht30.humidity < humidity_setpoint) {

        humion = true;
      }
      else if (sht30.humidity > (humidity_setpoint + humidity_range)) {
        humion = false;
      }
    }
    else {
      if (sht30.humidity > humidity_setpoint) {
        humion = true;
      }
      else if (sht30.humidity < (humidity_setpoint - humidity_range)) {

        humion = false;
      }
    }

    if(COOL) {
      if (sht30.cTemp > temperature_setpoint) {

        tempon = true;
      }
      else if (sht30.cTemp < (temperature_setpoint - temperature_range)) {

        tempon = false;
      }
    }
    else {
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
          digitalWrite(RELAY1, HIGH);
          Serial.println("RELAY1 ON");
          digitalWrite(LED_BUILTIN, LOW);  // turn on
        }
      }
      else if (tempon == false && humion == false) {
        if (afterStart != -1) {
          t_relay.stop(afterStart);
          afterStart = -1;
        }
        digitalWrite(RELAY1, LOW);
        Serial.println("RELAY1 OFF");
        digitalWrite(LED_BUILTIN, HIGH);  // turn off
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
          digitalWrite(RELAY1, HIGH);
          Serial.println("RELAY1 ON");
          digitalWrite(LED_BUILTIN, LOW);  // turn on
        }
      }
      else if (tempon == false) {
        if (afterStart != -1) {
          t_relay.stop(afterStart);
          afterStart = -1;
        }
        digitalWrite(RELAY1, LOW);
        Serial.println("RELAY1 OFF");
        digitalWrite(LED_BUILTIN, HIGH);  // turn off
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
          digitalWrite(RELAY1, HIGH);
          Serial.println("RELAY1 ON");
          digitalWrite(LED_BUILTIN, LOW);  // turn on
        }
      }
      else if (humion == false) {
        if (afterStart != -1) {
          t_relay.stop(afterStart);
          afterStart = -1;
        }
        digitalWrite(RELAY1, LOW);
        Serial.println("RELAY1 OFF");
        digitalWrite(LED_BUILTIN, HIGH);  // turn off
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
    Serial.println(humion);
    // Serial.println();



    // Only update if posting time is exceeded
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdateTime >=  postingInterval) {
      lastUpdateTime = currentMillis;

      float fahrenheitTemperature, celsiusTemperature;
      float rhHumidity;

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

      //ThingSpeak.writeField(channelID, dataFieldOne, celsiusTemperature, writeAPIKey);
      //ThingSpeak.writeField(channelID, dataFieldTwo, rhHumidity, writeAPIKey);
      //ThingSpeak.writeField(channelID, dataFieldThree, fahrenheitTemperature, writeAPIKey);
      // write2TSData(channelID, dataFieldOne, celsiusTemperature, dataFieldTwo, rhHumidity, dataFieldThree, fahrenheitTemperature);

      ThingSpeak.setField( 1, celsiusTemperature );
      ThingSpeak.setField( 2, rhHumidity );
      ThingSpeak.setField( 3, digitalRead(RELAY1));
      int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
      Serial.println(writeSuccess);
      Serial.println();
    }
  }
  else
  {
    Serial.println("Sensor Error!");
  }
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

void turnoff()
{
  afterStop = t_delayStart.after(standbyPeriod, delayStart);   // 10 * 60 * 1000 = 10 minutes
  digitalWrite(RELAY1, LOW);
  Serial.println("Timer Stop: RELAY1 OFF");
  digitalWrite(LED_BUILTIN, HIGH);  // turn off
  afterStart = -1;
}

void delayStart()
{
  RelayEvent = false;
  afterStop = -1;
  Serial.println("Timer Delay Start End.");
}

//use this function if you want multiple fields simultaneously
int write2TSData( long TSChannel, unsigned int TSField1, float field1Data, unsigned int TSField2, float field2Data, unsigned int TSField3, float field3Data ){

  ThingSpeak.setField( TSField1, field1Data );
  ThingSpeak.setField( TSField2, field2Data );
  ThingSpeak.setField( TSField3, field3Data );

  int writeSuccess = ThingSpeak.writeFields( TSChannel, writeAPIKey );
  return writeSuccess;
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
  send2thingspeak = true;
}

void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen)
{
    Serial.print("Incoming message --> ");
    msg[msglen] = '\0';
    Serial.println((char *)msg);

    /*
    if(strcmp(topic, me) == 0) {
      if ((char)msg[0] == '1') {
        timer_delaysend.after(15 * 1000, sendThingSpeak());
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
  Serial.println(pinValue);
  if (pinValue == 1) {
    buzzer_sound();
    led1.on();
    digitalWrite(RELAY1, HIGH);
  }
  else {
    buzzer_sound();
    led1.off();
    digitalWrite(RELAY1, LOW);
  }
  
}

BLYNK_WRITE(V2)
{
  int pinValue = param.asInt();
  if (pinValue == 1) {
    AUTO = true;
  }
  else {
    AUTO = false;
  }
}

void reconnectBlynk() {
  if (!Blynk.connected()) {
    if(Blynk.connect()) {
      BLYNK_LOG("Reconnected");
    } else {
      BLYNK_LOG("Not reconnected");
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
