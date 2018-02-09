#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>


#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <WEMOS_SHT3X.h>

#include <Timer.h>

String host = "http://piwatair.net";   //กำหนดชื่อ Server ที่ต้องการเชื่อมต่อ
const char* KeyCode     = "";      //== 2 == ใส่ KeyCode  ของ Device
const char* SecretKey = "";    //== 3 == ใส่ SecretKey ของ Device
// uri
// http://piwatair.net/SensorRead.php?KeyCode=xxxx&SecretKey=yyyy&temp=25&humi=50


SHT3X sht30(0x45);                          // address sensor

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  Serial.println();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  String APName;

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



}

void loop() {
  int humidity_sensor_value;
  float temperature_sensor_value;

  // put your main code here, to run repeatedly:
  if(sht30.get()==0) {
    temperature_sensor_value = sht30.cTemp;
    humidity_sensor_value = (int) sht30.humidity;
    sendData(temperature_sensor_value, humidity_sensor_value);

  }
  else {
      Serial.println("Sensor Error!");
  }
  delay(5000);
}

void sendData(float temperature, int humidity)
{
  HTTPClient http;
  String uri = host + "/SensorRead.php?" + "KeyCode=" + KeyCode + "&SecretKey=" + SecretKey + "&temp=" + String(temperature,2) + "&humi=" + String(humidity, DEC);
  Serial.println(uri);
  http.begin(uri);
  int httpCode = http.GET();
  if (httpCode > 0) { //Check the returning code
    Serial.print("HTTP Code : ");
    Serial.println(httpCode);
    String payload = http.getString();   //Get the request response payload
    Serial.println(payload);             //Print the response payload
  }

  http.end();   //Close connection
}
