/*
  MIT License
Version 1.0 2018-01-22
Version 2.0 2025-03-15

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
 * Wemos SHT30 Shield use D1, D2 pin
 * Wemos Relay Shield use D6, D7
 * dot matrix LED // use D5, D7
 *
 *
 */

#define HIGH_TEMPERATURE 30.0
#define LOW_TEMPERATURE 25.0
#define HIGH_HUMIDITY 60
#define LOW_HUMIDITY 55
#define OPTIONS 1
#define COOL_MODE 1
#define MOISTURE_MODE 0


#ifdef SOILMOISTURE
  #define soilMoistureLevel 50
#endif


#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"


#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>         // สำหรับ ESP8266 (Wemos D1 mini pro)
#endif
#include <WiFiManager.h>         // WiFiManager เวอร์ชัน 2.0.17
#include <ThingSpeak.h>
#include <ESP8266HTTPClient.h>   // HTTP Client สำหรับ ESP8266
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <WEMOS_SHT3X.h>              // ไลบรารี SHT3x สำหรับเซนเซอร์ SHT30
#include <SPI.h>
#include <EEPROM.h>
#include <Timer.h>  //https://github.com/JChristensen/Timer
#include "ogosense.h"

#ifdef ESP8266
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

char auth[] = "XXXXXXXXXXed4061bb4e0dXXXXXXXXXX";

 #ifdef MATRIXLED
  #include <MLEDScroll.h>
  MLEDScroll matrix;
#endif

#ifdef NETPIE
  #include <MicroGear.h>
#endif


#if defined(SECONDRELAY) && !defined(MATRIXLED)
  const int RELAY1 = D7;
  const int RELAY2 = D6;
#else
  const int RELAY1 = D7;
#endif

#ifdef SLEEP
  // sleep for this many seconds
  const int sleepSeconds = 300;
#endif

const int buzzer=D5;                        // Buzzer control port, default D5
const int analogReadPin = A0;               // read for set options use R for voltage divide
const int LED = D4;                         // output for LED (toggle) buildin LED board

// ค่าตั้งต้นสำหรับควบคุม Relay (สามารถปรับผ่านคำสั่งจาก Chat)
float lowTemp  = LOW_TEMPERATURE;   // เมื่ออุณหภูมิต่ำกว่าค่านี้ให้ปิด Relay
float highTemp = HIGH_TEMPERATURE;   // เมื่ออุณหภูมิสูงกว่าค่านี้ให้เปิด Relay
float lowHumidity = LOW_HUMIDITY;
float highHumidity = HIGH_HUMIDITY;

// ค่า sensor readings
float temperature = 0.0;
float humidity    = 0.0;

float temperature_sensor_value, fTemperature;
int humidity_sensor_value;

// ===== Control Settings =====
bool AUTO = true;        // AUTO or Manual Mode ON/OFF relay, AUTO is depend on temperature, humidity; Manual is depend on command
int options = OPTIONS; // 1 ค่า default (สามารถเปลี่ยนผ่านคำสั่ง /setmode) // options : 0 = humidity only, 1 = temperature only, 2 = temperature & humidity

// โหมดสำหรับ temperature และ humidity
int COOL = COOL_MODE;    // 1 ค่า default // COOL: 1 = COOL mode (relay ON เมื่ออุณหภูมิสูงเกิน highTemp), 0 = HEAT mode (relay OFF เมื่ออุณหภูมิต่ำกว่า lowTemp)
int MOISTURE = MOISTURE_MODE; // 0 ค่า default // MOISTURE: 1 = moisture mode (relay ON เมื่อความชื้นต่ำกว่า lowHum), 0 = dehumidifier mode (relay OFF เมื่อความชื้นสูงกว่า highHum)

bool tempon = false;     // flag ON/OFF
bool humion = false;     // flag ON/OFF

SHT3X sht30(0x45);  // address sensor use D1, D2 pin

WiFiClient client;

WiFiClientSecure clientSecure;
UniversalTelegramBot bot(telegramToken, clientSecure);

const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;
const unsigned long onPeriod = 60L * 60L * 1000L;       // ON relay period minute * second * milli second
const unsigned long standbyPeriod = 300L * 1000L;       // delay start timer for relay

Timer t_relay, t_delayStart, t_readSensor, t_checkFirmware;         // timer for ON period and delay start
Timer t_relay2, t_delayStart2;
Timer t_sendDatatoThinkSpeak;
Timer t_getTelegramMessage;

bool RelayEvent = false;
int afterStart = -1;
int afterStop = -1;

bool Relay2Event = false;
int afterStart2 = -1;
int afterStop2 = -1;

// ===== Function Prototypes =====
int readSensorData();
void controlRelay();
void sendDataToThingSpeak();
void printConfig();
void getConfig();
void handleNewMessages(int numNewMessages);
void getTelegramMessage();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");   // get UTC time via NTP
    clientSecure.setTrustAnchors(&cert);      // Add root certificate for api.telegram.org
  #endif
  delay(10);
  Serial.println();
  Serial.println("Starting");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);

  pinMode(analogReadPin, INPUT);
  pinMode(RELAY1, OUTPUT);

  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1, LOW);

  #ifdef SECONDRELAY
    pinMode(RELAY2, OUTPUT);
    digitalWrite(RELAY2, LOW);
  #endif

  autoWifiConnect();

  getConfig();
  printConfig();
  readSensorData();
  buzzer_sound();

  #ifdef THINGSPEAK
    ThingSpeak.begin(client);
  #endif

  t_readSensor.every(5000, controlRelay); 
  t_sendDatatoThinkSpeak.every(60L * 1000L, sendDataToThingSpeak);
  t_getTelegramMessage.every(1000L, getTelegramMessage);

}

void loop() {
  // put your main code here, to run repeatedly:
  blink();  // flash LED

  // ตรวจสอบคำสั่งจาก Telegram
  t_getTelegramMessage.update();

  t_relay.update();
  t_delayStart.update();

  t_relay2.update();
  t_delayStart2.update();

  t_readSensor.update();

  t_sendDatatoThinkSpeak.update();

}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages)
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    // ตรวจสอบว่า chat_id อยู่ในรายชื่อ allowedChatIDs หรือไม่
    bool authorized = false;
    for (int j = 0; j < numAllowedChatIDs; j++) {
      if (chat_id == String(allowedChatIDs[j])) {
        authorized = true;
        break;
      }
    }
    if (!authorized) {
      Serial.println("Unauthorized chat: " + chat_id);
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    /*
    if (chat_id != TELEGRAM_CHAT_ID){
      Serial.println("Chat ID: " + chat_id);
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    */

    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);
    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";

      bot.sendMessage(chat_id, welcome, "");
    }
    /*
    if (text == "/status") {
      String statusMsg = "🌡 Temp: " + String(sht30.cTemp) + "°C\n";
      statusMsg += "💧 Humidity: " + String(sht30.humidity) + "%\n";
      bool relayState = (0 != (*portOutputRegister(digitalPinToPort(RELAY1)) & digitalPinToBitMask(RELAY1)));
      statusMsg += "💡 Relay: " + String(relayState ? "ON" : "OFF");
      bot.sendMessage(chat_id, statusMsg);
    }
    */
    if (text.startsWith("/status")) {
      int spaceIndex = text.indexOf(' ');
      if (spaceIndex == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /status <id>");
      } else {
        int cmdID = text.substring(spaceIndex + 1).toInt();
        if (cmdID != DEVICE_ID) {
          bot.sendMessage(chat_id, "Device ID ไม่ตรงกับเครื่องนี้", "");
        } else {
          String statusMsg = "Device ID: " + String(DEVICE_ID) + "\n";
          statusMsg += "🌡 Temperature: " + String(sht30.cTemp) + "°C\n";
          statusMsg += "💧 Humidity: " + String(sht30.humidity) + "%\n";
          bool relayState = (0 != (*portOutputRegister(digitalPinToPort(RELAY1)) & digitalPinToBitMask(RELAY1)));
          statusMsg += "💡 Relay: " + String(relayState ? "ON" : "OFF");
          bot.sendMessage(chat_id, statusMsg);
        }
      }
    }

  }

}

void getTelegramMessage()
{
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while(numNewMessages) {
    Serial.println("got response");
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

#ifdef THINGSPEAK
void sendDataToThingSpeak()
{
  float celsiusTemperature = 0;
  float rhHumidity = 0;

  readSensorData();

  celsiusTemperature = temperature;
  rhHumidity = humidity;

  Serial.print(celsiusTemperature);
  Serial.print(", ");
  Serial.print(rhHumidity);
  Serial.println();
  Serial.println("Sending data to ThingSpeak : ");

  ThingSpeak.setField( 1, celsiusTemperature );
  ThingSpeak.setField( 2, rhHumidity );
  ThingSpeak.setField( 3, digitalRead(RELAY1));

  int httpResponseCode = ThingSpeak.writeFields(channelID, writeAPIKey);
  
  if (httpResponseCode == 200) {
    Serial.println("ส่งข้อมูล ThingSpeak สำเร็จ");
  } else {
    Serial.println("ส่งข้อมูล ThingSpeak ล้มเหลว: " + String(httpResponseCode));
  }
  Serial.println();

}
#endif

void turnRelayOn()
{
  digitalWrite(RELAY1, HIGH);
  Serial.println("RELAY1 ON");
  digitalWrite(LED_BUILTIN, LOW);  // turn on
  buzzer_sound();
}

void turnRelayOff()
{
  digitalWrite(RELAY1, LOW);
  Serial.println("RELAY1 OFF");
  digitalWrite(LED_BUILTIN, HIGH);  // turn off
  buzzer_sound();
}

#ifdef SECONDRELAY
void turnRelay2On()
{
  digitalWrite(RELAY2, HIGH);
  Serial.println("RELAY2 ON");
  digitalWrite(LED_BUILTIN, LOW);  // turn on
  led3.on();
  Blynk.virtualWrite(V3, 1);
  buzzer_sound();
}

void turnRelay2Off()
{
  digitalWrite(RELAY2, LOW);
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
    // turnrelay_onoff(LOW);
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

void controlRelay()
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

  sensorStatus = readSensorData();  // 0 is OK

  if (AUTO) {
    Serial.print("\tOptions : ");
    Serial.println(options);

    if(sensorStatus == 0) {
      // Moisture mode
      
      if (MOISTURE == 1) {  // ความชื้นต่ำให้เปิด relay
        if (humidity_sensor_value <= lowHumidity) {
          humion = true;
        }
        else if (humidity_sensor_value >= highHumidity) { // ความชื้นสูงให้ปิด relay
          humion = false;
        }
      }
      // Dehumidifier mode
      else if (MOISTURE == 0){ // ความชื้นสูงให้เปิด relay
        if (humidity_sensor_value >= highHumidity) {
          humion = true;
        }
        else if (humidity_sensor_value <= lowHumidity) { // ความชื้นต่ำให้ปิด relay 
          humion = false;
        }
      }
      // cool mode
      if(COOL == 1) {
        if (temperature_sensor_value >= highTemp) { // ร้อนให้เปิด relay
          tempon = true;
        }
        else if (temperature_sensor_value <= lowTemp) { // เย็นให้ปิด relay
          tempon = false;
        }
      }
      // heater mode
      else if (COOL == 0){
        if (temperature_sensor_value <= lowTemp) {  // เย็นให้เปิด relay or heater
          tempon = true;
        }
        else if (temperature_sensor_value >= highTemp) { // ร้อนให้ปิด relay or heater
          tempon = false;
        }
      }

      if (options == 2) { // ทำงานสองเงื่อนไขพร้อมกัน 
        Serial.println("Option: Temperature & Humidity");
        if (tempon == true && humion == true) {
          if (RelayEvent == false) {
            afterStart = t_relay.after(onPeriod, turnoff);
            Serial.println("On Timer Start.");
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
          if (digitalRead(RELAY1) == HIGH) {
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
            if (digitalRead(RELAY1) == HIGH) {
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
            if (digitalRead(RELAY2) == HIGH) {
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
          if (digitalRead(RELAY1) == HIGH) {
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

    } // if sensorStatus
  } // if AUTO 


}

int readSensorData() 
{
  if (sht30.get() == 0) {
    humidity_sensor_value = (int) sht30.humidity;
    temperature_sensor_value = sht30.cTemp;
    fTemperature = sht30.fTemp;
    Serial.println("Device " + String(DEVICE_ID) + " - Temperature: " + String(temperature_sensor_value) + "°C, Humidity: " + String(humidity_sensor_value) + "%");
    temperature = temperature_sensor_value;
    humidity = humidity_sensor_value;
    return 0; // OK
  }
  else {
    Serial.println("Failed to read from SHT30 sensor.");
    return 1; // 
  }
}

void printConfig()
{
  Serial.println("Configuration");

  Serial.print("Temperature High: ");
  Serial.println(highTemp);
  Serial.print("Temperature Low: ");
  Serial.println(lowTemp);

  Serial.print("Humidity High: ");
  Serial.println(highHumidity);
  Serial.print("Humidity Low: ");
  Serial.println(lowHumidity);

  Serial.print("Option: ");
  Serial.println(options);

  Serial.print("COOL: ");
  Serial.println(COOL);

  Serial.print("MOISTURE: ");
  Serial.println(MOISTURE);

  Serial.print("Write API Key: ");
  Serial.println(writeAPIKey);

  Serial.print("Read API Key: ");
  Serial.println(readAPIKey);

  Serial.print("Channel ID : ");
  Serial.println(channelID);
}

void getConfig()
{
  // read config from eeprom
  EEPROM.begin(512);
  highHumidity = (float) EEPROMReadlong(0);
  lowHumidity = (float) EEPROMReadlong(4);
  highTemp = (float) EEPROMReadlong(8);
  lowTemp = (float) EEPROMReadlong(12);
  options = eeGetInt(16);
  COOL = eeGetInt(20);
  MOISTURE = eeGetInt(24);
  readEEPROM(writeAPIKey, 28, 16);
  readEEPROM(readAPIKey, 44, 16);
  readEEPROM(auth, 60, 32);
  channelID = (unsigned long) EEPROMReadlong(92);
}

void autoWifiConnect()
{
  WiFiManager wifiManager;
  bool res;

  //first parameter is name of access point, second is the password
  res = wifiManager.autoConnect("ogosense", "12345678");

  if(!res) {
      Serial.println("Failed to connect");
      delay(3000);
      ESP.restart();
      delay(5000);
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("connected...yeey :)");
  }
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

