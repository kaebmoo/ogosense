#include <EEPROM.h>
#include <ESP8266WiFi.h>         // สำหรับ ESP8266 (Wemos D1 mini pro)
#include <WiFiManager.h>
#include "ogosense_eeprom.h"

float lowTemp  = 26.0;   // เมื่ออุณหภูมิต่ำกว่าค่านี้ให้ปิด Relay
float highTemp = 30.0;   // เมื่ออุณหภูมิสูงกว่าค่านี้ให้เปิด Relay
float lowHumidity = 55;
float highHumidity = 60;
int options = 1;          // options : 0 = humidity only, 1 = temperature only, 2 = temperature & humidity
int COOL = 1;             // COOL: 1 = COOL mode (relay ON เมื่ออุณหภูมิสูงเกิน highTemp), 0 = HEAT mode (relay OFF เมื่ออุณหภูมิต่ำกว่า lowTemp)
int MOISTURE = 0;         // MOISTURE: 0 = moisture mode (relay ON เมื่อความชื้นต่ำกว่า lowHumidity), 0 = dehumidifier mode (relay OFF เมื่อความชื้นสูงกว่า highHumidity)

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting:");
  

  autoWifiConnect();
  Serial.println("Setup config:");
  Serial.println("Default config:");
  printConfig();
  Serial.println("Saving config:");
  saveConfig();
  delay(100);
  Serial.println("Reading config:");
  getConfig();

  int saved = eeGetInt(500);
  if (saved == 6550) {
    Serial.println("Saved config:");
    printConfig();
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

void loop() {
  // put your main code here, to run repeatedly:

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