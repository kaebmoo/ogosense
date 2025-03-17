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

#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>         // สำหรับ ESP8266 (Wemos D1 mini pro)
  #include <ESP8266WebServer.h>
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
#include <Ticker.h>  //Ticker Library
// #include <ElegantOTA.h>

#include "ogosense.h"

#ifdef ESP8266
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

WiFiClient client;  // ใช้กับ thinkspeak
WiFiClientSecure clientSecure; // ใช้กับ telegram
UniversalTelegramBot bot(telegramToken, clientSecure);

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
int COOL = COOL_MODE;    // 1 ค่า default // COOL: 1 = COOL mode , 0 = HEAT mode 
int MOISTURE = MOISTURE_MODE; // 0 ค่า default // MOISTURE: 1 = moisture mode, 0 = dehumidifier mode 
bool tempon = false;     // flag ON/OFF
bool humion = false;     // flag ON/OFF

SHT3X sht30(0x45);  // address sensor use D1, D2 pin

const long interval = 1000;
int ledState = LOW;
unsigned long previousMillis = 0;
unsigned long ota_progress_millis = 0;
const unsigned long onPeriod = 60L * 60L * 1000L;       // ON relay period minute * second * milli second
const unsigned long standbyPeriod = 300L * 1000L;       // delay start timer for relay

Timer t_relay, t_delayStart, t_checkFirmware;         // timer for ON period and delay start
Timer t_relay2, t_delayStart2;
Timer t_sendDatatoThinkSpeak;
Timer t_blink;           // สร้าง object สำหรับ timer

Ticker blinker;
Ticker t_getTelegramMessage;
Ticker t_readSensor;

volatile bool shouldReadSensor = false;
volatile bool shouldCheckTelegram = false;
bool RelayEvent = false;
int afterStart = -1;
int afterStop = -1;
/*
In this firmware:

- **afterStart** is used to store the timer ID for the "on period" of the relay. 
When the sensor conditions trigger the relay to turn on, 
the code schedules a timer (using `t_relay.after(onPeriod, turnoff)`) 
and saves its identifier in `afterStart`. This timer will eventually trigger 
the `turnoff()` function to turn off the relay after the defined period.

- **afterStop** is used to store the timer ID for the "delay period" after 
the relay is turned off. Once the relay is turned off (or when sensor conditions change), 
the code schedules another timer (using `t_delayStart.after(standbyPeriod, delayStart)`) 
and saves its identifier in `afterStop`. This ensures there is a standby interval during 
which the relay cannot be turned on immediately again, helping to prevent rapid toggling.

In summary, **afterStart** manages how long the relay stays on, 
and **afterStop** ensures a delay before the relay can be activated again.
*/

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
void turnRelayOn();
void turnRelayOff();
void buzzer_sound();
void blink();


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");         // get UTC time via NTP
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

  // t_blink.every(1000, blink); // 1 second   // กระพริบ LED ทุก 1 วินาที
  blinker.attach(1, blink); //Use attach_ms if you need time in ms

  autoWifiConnect();  // ต่อ WiFi โดยใช้ WiFiManager

  getConfig();        // อ่านค่า config จาก EEPROM
  printConfig();      // แสดงค่า config ที่อ่านได้
  readSensorData();   // อ่านค่า sensor และแสดงผลลัพธ์
  buzzer_sound();     // เสียง buzzer

  #ifdef THINGSPEAK
    ThingSpeak.begin(client);
  #endif
  
  t_readSensor.attach(5, setReadSensorFlag);  // 5 seconds  // อ่านค่า sensor ทุก 5 วินาที
  t_getTelegramMessage.attach(1, checkTelegramFlag);  // อ่านข้อความจาก Telegram ทุก 1 วินาที
  t_sendDatatoThinkSpeak.every(60L * 1000L, sendDataToThingSpeak);  // ส่งข้อมูลไป ThingSpeak ทุก 1 นาที
  
}

void loop() {
  // put your main code here, to run repeatedly:
  // t_blink.update();  // flash LED

  if (shouldReadSensor) {
    shouldReadSensor = false;
    controlRelay(); // เรียกใน task context ป้องกัน core dump
  }

  // ตรวจสอบคำสั่งจาก Telegram
  if (shouldCheckTelegram) {
    shouldCheckTelegram = false;
    getTelegramMessage(); // เรียกใน context ของ loop แทนการทำงานใน interrupt context
  }

  t_relay.update();
  t_delayStart.update();

  t_relay2.update();
  t_delayStart2.update();

  t_sendDatatoThinkSpeak.update();

}

void setReadSensorFlag() {
  shouldReadSensor = true;
}

void checkTelegramFlag() {
  shouldCheckTelegram = true;
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages)
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // เรียก yield เพื่อคืน control ให้ system และป้องกัน watchdog reset
    yield();

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
      String welcome = "Welcome to OgoSense!, " + from_name + ".\n";

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
    else if (text.startsWith("/status")) {
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
        // /settemp <device_id> <lowTemp> <highTemp>
    else if (text.startsWith("/settemp")) {
      int firstSpace = text.indexOf(' ');
      if (firstSpace == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /settemp <id> <lowTemp> <highTemp>");
        continue;
      }
      String params = text.substring(firstSpace + 1);
      int space1 = params.indexOf(' ');
      int space2 = params.indexOf(' ', space1 + 1);
      if (space1 == -1 || space2 == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /settemp <id> <lowTemp> <highTemp>");
        continue;
      }
      int cmdID = params.substring(0, space1).toInt();
      if (cmdID != DEVICE_ID) {
        Serial.println("คำสั่ง /settemp ไม่ตรงกับ DEVICE_ID " + String(DEVICE_ID));
        continue;
      }
      float lt = params.substring(space1 + 1, space2).toFloat();
      float ht = params.substring(space2 + 1).toFloat();
      lowTemp = lt;
      highTemp = ht;
      saveConfig();
      bot.sendMessage(chat_id, "ตั้งค่า Temperature สำเร็จ: Low=" + String(lowTemp) +
                      "°C, High=" + String(highTemp) + "°C");
    }
    // /sethum <device_id> <lowHumidity> <highHumidity>
    else if (text.startsWith("/sethum")) {
      int firstSpace = text.indexOf(' ');
      if (firstSpace == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /sethum <id> <lowHumidity> <highHumidity>");
        continue;
      }
      String params = text.substring(firstSpace + 1);
      int space1 = params.indexOf(' ');
      int space2 = params.indexOf(' ', space1 + 1);
      if (space1 == -1 || space2 == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /sethum <id> <lowHumidity> <highHumidity>");
        continue;
      }
      int cmdID = params.substring(0, space1).toInt();
      if (cmdID != DEVICE_ID) {
        Serial.println("คำสั่ง /sethum ไม่ตรงกับ DEVICE_ID " + String(DEVICE_ID));
        continue;
      }
      float lh = params.substring(space1 + 1, space2).toFloat();
      float hh = params.substring(space2 + 1).toFloat();
      lowHumidity = lh;
      highHumidity = hh;
      saveConfig();
      bot.sendMessage(chat_id, "ตั้งค่า Humidity สำเร็จ: Low=" + String(lowHumidity) +
                      "%, High=" + String(highHumidity) + "%");
    }
    // คำสั่ง /setmode <device_id> <mode>
    // mode รับเป็น "auto" หรือ "manual"
    else if (text.startsWith("/setmode")) {
      int firstSpace = text.indexOf(' ');
      if (firstSpace == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /setmode <id> <mode>");
        continue;
      }
      String params = text.substring(firstSpace + 1);
      int spaceIndex = params.indexOf(' ');
      if (spaceIndex == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /setmode <id> <mode>");
        continue;
      }
      int cmdID = params.substring(0, spaceIndex).toInt();
      if (cmdID != DEVICE_ID) {
        Serial.println("คำสั่ง /setmode ไม่ตรงกับ DEVICE_ID " + String(DEVICE_ID));
        continue;
      }
      String modeStr = params.substring(spaceIndex + 1);
      modeStr.trim();
      if (modeStr.equalsIgnoreCase("auto")) {
        // สมมุติว่า mode auto คือ 1
        // deviceMode หรือ AUTO flag
        AUTO = true;
      } else if (modeStr.equalsIgnoreCase("manual")) {
        AUTO = false;
      } else {
        bot.sendMessage(chat_id, "Mode ไม่ถูกต้อง (ควรเป็น auto หรือ manual)", "");
        continue;
      }
      saveConfig();
      bot.sendMessage(chat_id, "ตั้งค่า Mode สำเร็จ: " + String(AUTO ? "Auto" : "Manual"));
    }
    // คำสั่ง /setoption <device_id> <option>
    // option ควรอยู่ในช่วง 0-4
    else if (text.startsWith("/setoption")) {
      int firstSpace = text.indexOf(' ');
      if (firstSpace == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /setoption <id> <option>");
        continue;
      }
      String params = text.substring(firstSpace + 1);
      int spaceIndex = params.indexOf(' ');
      if (spaceIndex == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /setoption <id> <option>");
        continue;
      }
      int cmdID = params.substring(0, spaceIndex).toInt();
      if (cmdID != DEVICE_ID) {
        Serial.println("คำสั่ง /setoption ไม่ตรงกับ DEVICE_ID " + String(DEVICE_ID));
        continue;
      }
      int opt = params.substring(spaceIndex + 1).toInt();
      if (opt < 0 || opt > 4) {
        bot.sendMessage(chat_id, "Option ต้องอยู่ในช่วง 0-4", "");
        continue;
      }
      options = opt;
      saveConfig();
      bot.sendMessage(chat_id, "ตั้งค่า Option สำเร็จ: Option=" + String(options));
    }
    else if (text.startsWith("/info")) {
      int firstSpace = text.indexOf(' ');
      if (firstSpace == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /info <id> <secret>");
      } else {
        // แยก device_id และ secret code
        String params = text.substring(firstSpace + 1);
        int spaceIndex = params.indexOf(' ');
        if (spaceIndex == -1) {
          bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /info <id> <secret>");
          continue;
        }
        int cmdID = params.substring(0, spaceIndex).toInt();
        String secret = params.substring(spaceIndex + 1);
        secret.trim();
        if (cmdID != DEVICE_ID) {
          bot.sendMessage(chat_id, "Device ID ไม่ตรงกับเครื่องนี้", "");
        } else if (!secret.equals(INFO_SECRET)) {
          bot.sendMessage(chat_id, "Secret code ไม่ถูกต้อง", "");
        } else {
          String infoMsg = "----- Device Info -----\n";
          infoMsg += "Device ID: " + String(DEVICE_ID) + "\n";
          infoMsg += "IP Address: " + WiFi.localIP().toString() + "\n";
          infoMsg += "MAC Address: " + WiFi.macAddress() + "\n";
          infoMsg += "Temperature Set Points: Low = " + String(lowTemp) + "°C, High = " + String(highTemp) + "°C\n";
          infoMsg += "Humidity Set Points: Low = " + String(lowHumidity) + "%, High = " + String(highHumidity) + "%\n";
          infoMsg += "Mode: " + String(AUTO ? "AUTO" : "Manual") + "\n";
          
          String optionStr;
          switch(options) {
            case 0: optionStr = "0 (Humidity only)"; break;
            case 1: optionStr = "1 (Temperature only)"; break;
            case 2: optionStr = "2 (Temperature & Humidity)"; break;
            case 3: optionStr = "3 (Soil Moisture mode)"; break;
            case 4: optionStr = "4 (Additional mode)"; break;
            default: optionStr = String(options) + " (Unknown)";
          }
          infoMsg += "Option: " + optionStr + "\n";
          
          String coolStr = (COOL == 1) ? "1 (COOL mode: Relay ON เมื่อ Temp >= High)" : "0 (HEAT mode: Relay ON เมื่อ Temp <= Low)";
          infoMsg += "COOL: " + coolStr + "\n";
          
          String moistureStr = (MOISTURE == 1) ? "1 (Moisture mode: Relay ON เมื่อ Humidity <= Low)" : "0 (Dehumidifier mode: Relay ON เมื่อ Humidity >= High)";
          infoMsg += "MOISTURE: " + moistureStr + "\n";
          
          infoMsg += "Write API Key: " + String(writeAPIKey) + "\n";
          infoMsg += "Read API Key: " + String(readAPIKey) + "\n";
          infoMsg += "Channel ID: " + String(channelID) + "\n";
          
          infoMsg += "Allowed Chat IDs: ";
          for (int j = 0; j < numAllowedChatIDs; j++) {
            infoMsg += String(allowedChatIDs[j]);
            if (j < numAllowedChatIDs - 1)
              infoMsg += ", ";
          }
          infoMsg += "\n-----------------------";
          
          bot.sendMessage(chat_id, infoMsg);
        }
      }
    }
    else if (text.startsWith("/relay")) {
      int firstSpace = text.indexOf(' ');
      if (firstSpace == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /relay <id> <state>");
        continue;
      }
      String params = text.substring(firstSpace + 1);
      int spaceIndex = params.indexOf(' ');
      if (spaceIndex == -1) {
        bot.sendMessage(chat_id, "รูปแบบคำสั่งไม่ถูกต้อง: /relay <id> <state>");
        continue;
      }
      int cmdID = params.substring(0, spaceIndex).toInt();
      if (cmdID != DEVICE_ID) {
        bot.sendMessage(chat_id, "Device ID ไม่ตรงกับเครื่องนี้", "");
        continue;
      }
      int state = params.substring(spaceIndex + 1).toInt();
      
      // คำสั่งนี้ใช้ได้เฉพาะใน Manual mode เท่านั้น
      if (AUTO) {
        bot.sendMessage(chat_id, "เครื่องอยู่ใน Auto mode ไม่สามารถสั่ง Relay ด้วยคำสั่งนี้", "");
      } else {
        if (state == 1) {
          turnRelayOn();
          RelayEvent = true;
          bot.sendMessage(chat_id, "Relay ถูกเปิดแล้ว", "");
        } else if (state == 0) {
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
          bot.sendMessage(chat_id, "Relay ถูกปิดแล้ว", "");
        } else {
          bot.sendMessage(chat_id, "ค่าของ state ต้องเป็น 1 (เปิด) หรือ 0 (ปิด)", "");
        }
      }
    }

    else {
      // bot.sendMessage(chat_id, "คำสั่งไม่รู้จัก/unknow command", "");
      // หากคำสั่งไม่ตรงกับที่ระบุไว้ ให้ส่งข้อความช่วยเหลือ
      String helpMsg = "คำสั่งที่สามารถใช้ได้ / Available commands:\n";
      helpMsg += "/start - เริ่มต้นการใช้งาน / Start using the device\n";
      helpMsg += "/status <id> - ตรวจสอบสถานะอุปกรณ์ / Check device status\n";
      helpMsg += "/settemp <id> <lowTemp> <highTemp> - ตั้งค่า Temperature / Set temperature thresholds\n";
      helpMsg += "/sethum <id> <lowHumidity> <highHumidity> - ตั้งค่า Humidity / Set humidity thresholds\n";
      helpMsg += "/setmode <id> <auto/manual> - ตั้งค่าโหมดการทำงาน / Set operating mode\n";
      helpMsg += "/setoption <id> <option> - ตั้งค่า Option (0-4) / Set option (0-4)\n";
      helpMsg += "/relay <id> <state> - ควบคุม Relay (0: ปิด, 1: เปิด) / Control relay (0: off, 1: on)\n";
      // หมายเหตุ: ไม่รวมคำสั่ง /info
      bot.sendMessage(chat_id, helpMsg, "");
    }

  } // for loop message

}

void getTelegramMessage()
{
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while(numNewMessages) {
    Serial.println("got response");
    // เรียก yield เพื่อคืน control ให้ system และป้องกัน watchdog reset
    yield();
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
  // https://randomnerdtutorials.com/telegram-control-esp32-esp8266-nodemcu-outputs/
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
  buzzer_sound();
}

void turnRelay2Off()
{
  digitalWrite(RELAY2, LOW);
  Serial.println("RELAY2 OFF");
  digitalWrite(LED_BUILTIN, HIGH);  // turn off
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
   *  4 = temperature or humidity (temperature relay2)
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
      // กำหนด flag เพื่อให้ relay เปิด หรือ ปิด 
      if (MOISTURE == 1) {  // ความชื้นต่ำให้เปิด relay
        if (humidity_sensor_value < lowHumidity) {
          humion = true;
        }
        else if (humidity_sensor_value > highHumidity) { // ความชื้นสูงให้ปิด relay
          humion = false;
        }
      }
      // Dehumidifier mode
      else if (MOISTURE == 0){ // ความชื้นสูงให้เปิด relay หรือให้เปิด heater
        if (humidity_sensor_value > highHumidity) {
          humion = true;
        }
        else if (humidity_sensor_value < lowHumidity) { // ความชื้นต่ำให้ปิด relay 
          humion = false;
        }
      }
      // cool mode
      if(COOL == 1) {
        if (temperature_sensor_value > highTemp) { // ร้อนให้เปิด relay
          tempon = true;
        }
        else if (temperature_sensor_value < lowTemp) { // เย็นให้ปิด relay
          tempon = false;
        }
      }
      // heater mode
      else if (COOL == 0){
        if (temperature_sensor_value < lowTemp) {  // เย็นให้เปิด relay or heater
          tempon = true;
        }
        else if (temperature_sensor_value > highTemp) { // ร้อนให้ปิด relay or heater
          tempon = false;
        }
      }
      // จบเงื่อนไข

      // ตรวจสอบ option เพื่อทำงานตามอุณหภูมิ หรือ ความชื้น หรือทั้งสองอย่าง 
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
  else {
    Serial.println("Manual Mode Active");
  }


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

void saveConfig()
{
  EEPROM.begin(512);
  EEPROMWritelong(0, (long) highHumidity);
  EEPROMWritelong(4, (long) lowHumidity);
  EEPROMWritelong(8, (long) highTemp);
  EEPROMWritelong(12, (long) lowTemp);
  eeWriteInt(16, options);
  eeWriteInt(20, COOL);
  eeWriteInt(24, MOISTURE);
  writeEEPROM(writeAPIKey, 28, 16);
  writeEEPROM(readAPIKey, 44, 16);
  writeEEPROM(auth, 60, 32);
  EEPROMWritelong(92, (long) channelID);
  eeWriteInt(500, 6550);
  EEPROM.end();
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
  EEPROM.end();
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
  static bool ledState = false;  // ใช้ static variable เพื่อเก็บสถานะ LED ปัจจุบัน
  ledState = !ledState;          // สลับสถานะ
  digitalWrite(LED, ledState);
}