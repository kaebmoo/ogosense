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

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <WEMOS_SHT3X.h>
#include <TM1637Display.h>

const int chipSelect = D8; // SD CARD

const int CLK = D6; //Set the CLK pin connection to the display
const int DIO = D5; //Set the DIO pin connection to the display
const int analogReadPin = A0;
const int RELAY1 = D0;
const int LED = D4;
const int RELAY2 = D7;

TM1637Display display(CLK, DIO); //set up the 4-Digit Display.
SHT3X sht30(0x45);

float maxtemp = 99.0;
float mintemp = 0.0;
float maxhumidity = 99.0;
float minhumidity = 0.0;

float temperature_setpoint = 22.0;
float temperature_range = 4.0;

float humidity_setpoint = 60.0;
float humidity_range = 20;

char h_setpoint[6] = "60";
char h_range[6] = "20";

// SHT30 -40 - 125 C ; 0.2-0.6 +-

boolean COOL = true;  // true > set point, false < set point = HEAT mode
boolean MOISTURE = false; // true < set point; false > set point
boolean tempon = false;
boolean humion = false;

int options = 0;

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 360709;
char *readAPIKey = "GNZ8WEU763Z5DUEA";
char *writeAPIKey = "8M07EYX8NPCD9V8U";
const unsigned long postingInterval = 15L * 1000L;
unsigned int dataFieldOne = 1;                            // Field to write temperature C data
unsigned int dataFieldTwo = 2;                       // Field to write relative humidity data
unsigned int dataFieldThree = 3;                     // Field to write temperature F data
unsigned long lastConnectionTime = 0;
long lastUpdateTime = 0;
WiFiClient client;

const long interval = 2000;
int ledState = LOW;
unsigned long previousMillis = 0;

void setup() {

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(analogReadPin, INPUT);
  pinMode(RELAY1, OUTPUT);
  display.setBrightness(0x0a); //set the diplay to maximum brightness
  Serial.println("starting");
  options = analogRead(analogReadPin);
  Serial.println(options);

      // put your setup code here, to run once:
    Serial.begin(115200);

  WiFiManagerParameter custom_h_setpoint("humidity", "setpoint", h_setpoint, 6);
  WiFiManagerParameter custom_h_range("h_range", "range", h_range, 6);
  
  
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    String APName;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.addParameter(&custom_h_setpoint);
    wifiManager.addParameter(&custom_h_range);
    
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
    strcpy(h_setpoint, custom_h_setpoint.getValue());
    strcpy(h_range, custom_h_range.getValue());
    Serial.print("Humidity : ");
    Serial.println(h_setpoint);
    Serial.print("Humidity Range : ");
    Serial.println(h_range);

    humidity_setpoint = atol(h_setpoint);
    humidity_range = atol(h_range);
    
    ThingSpeak.begin( client );
}

void loop() {
  options = analogRead(analogReadPin);
  Serial.print("Analog Read : ");
  Serial.print(options);
  
  if (options < 100)
    options = 0;  // humidity
   else if (options > 100 && options < 200)
    options = 1;  // temperature
   else
    options = 2;  // temperature && humidity

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


    float tempdisplay = sht30.cTemp * 10;

    //  gfedcba
    // 00111001 = 0011 1001 = 0x39 = C

    uint8_t data[] = { 0x00, 0x00, 0x00, 0x39 };
    display.setSegments(data);
    display.showNumberDecEx(tempdisplay, (0x80 >> 2), true, 3, 0);
    delay(2000);
    
    blink();
    
    // 0x76 = H
    data[0] = 0x00;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 0x76;
    display.setSegments(data);
    tempdisplay = sht30.humidity * 10;
    display.showNumberDecEx(tempdisplay, (0x80 >> 2), true, 3, 0);
    delay(2000);

    
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
        digitalWrite(RELAY1, HIGH);
        Serial.println("RELAY1 ON");
        digitalWrite(LED_BUILTIN, LOW);  // turn on
      }
      else if (tempon == false && humion == false) {
        digitalWrite(RELAY1, LOW);
        Serial.println("RELAY1 OFF");
        digitalWrite(LED_BUILTIN, HIGH);  // turn off
      }
    }
    else if (options == 1) {
      Serial.println("Option: Temperature");
      if (tempon == true) {
        digitalWrite(RELAY1, HIGH);
        Serial.println("RELAY1 ON");
        digitalWrite(LED_BUILTIN, LOW);  // turn on
      }
      else if (tempon == false) {
        digitalWrite(RELAY1, LOW);
        Serial.println("RELAY1 OFF");
        digitalWrite(LED_BUILTIN, HIGH);  // turn off
      }
    }
    else {
      Serial.println("Option: Humidity");
      if (humion == true) {
        digitalWrite(RELAY2, HIGH);
        Serial.println("RELAY2 ON");
        digitalWrite(LED_BUILTIN, LOW);  // turn on
      }
      else if (humion == false) {
        digitalWrite(RELAY2, LOW);
        Serial.println("RELAY2 OFF");
        digitalWrite(LED_BUILTIN, HIGH);  // turn off
      }
    }
  
    Serial.print("tempon = ");
    Serial.print(tempon);
    Serial.print(" humion = ");
    Serial.println(humion);
    Serial.println();
  
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
      Serial.println("Sending data to ThingSpeak");
      Serial.println();
      //ThingSpeak.writeField(channelID, dataFieldOne, celsiusTemperature, writeAPIKey);
      //ThingSpeak.writeField(channelID, dataFieldTwo, rhHumidity, writeAPIKey);
      //ThingSpeak.writeField(channelID, dataFieldThree, fahrenheitTemperature, writeAPIKey);
      write2TSData(channelID, dataFieldOne, celsiusTemperature, dataFieldTwo, rhHumidity, dataFieldThree, fahrenheitTemperature);
  
  
    }
  }
  else
  {
    Serial.println("Sensor Error!");
  }

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
  // shouldSaveConfig = true;
  
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
