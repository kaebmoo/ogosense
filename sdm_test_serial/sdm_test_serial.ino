/*
 *
 *
 * https://github.com/reaper7/SDM_Energy_Meter
 *
 */


#include <SDM.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ThingSpeak.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiClient.h>

// #include <BlynkSimpleEsp32.h>
// #include <WiFi.h> // esp32

#include <ArduinoJson.h>
#include <PubSubClient.h>

#define THINGSBOARD
#define WIFI_AP "Red2"
#define WIFI_PASSWORD "ogonan2019"

// SDM<9600> sdm;
SDM sdm(Serial, 9600, NOT_A_PIN, SERIAL_8N1, false);

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 585496;
char *readAPIKey = "5S8UD732WGE1UVO7";
char *writeAPIKey = "D0T6TVCX48SBB9U5";

BlynkTimer blynkTimer;

WiFiClient client;

float tmpVol = NAN;
float tmpAmp = NAN;
float tmpWat = NAN;
float tmpFre = NAN;
float tmpEne = NAN;
float tmpPf = NAN;
float tmpMaxPowerDemand = NAN;
int readCount = 0;
float voltage = 0.0;
float amp = 0.0;
float watt = 0.0;
float freq = 0.0;
float energy = 0.0;
float pf = 0.0;
float MaxPowerDemand = 0.0;

// int BUILTIN_LED = 2;  // ESP32 board


char thingsboardServer[40] = "box.greenwing.email";
int  mqttport = 1883;                      // 1883 or 1888
char token[32] = "bLdQmUD1GvlIsCzirDld";   // device token from thingsboard server

char *mqtt_user = "seal";
char *mqtt_password = "sealwiththekiss";
const char* mqtt_server = "db.ogonan.com";
int mqtt_reconnect = 0;
const int MAXRETRY=5;
char *myRoom = "smartmeter/1";

PubSubClient mqttClient(client);

int task1Time, task2Time, task3Time;

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);


  sdm.begin();

  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  Serial.print("Connecting");
  Serial.println();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    /*
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      ondemandWiFi();
    }
    */
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  ThingSpeak.begin( client );
  setupMqtt();
  
  task1Time = blynkTimer.setInterval(15000L, readMeterData);
  task2Time = blynkTimer.setInterval(60000L, sendThingsBoard);
  task3Time = blynkTimer.setInterval(60000L, sendThingSpeak);                   // send data to thingspeak

  // Set the timer to overlap 5000ms
  blynkTimer.setTimeout(5000, onceOnlyTask1);
  blynkTimer.setTimeout(10000, onceOnlyTask2);
  blynkTimer.setTimeout(15000, onceOnlyTask3);
  
}

void loop() {
  blynkTimer.run();
  #ifdef THINGSBOARD
  if ( !mqttClient.connected() ) {
    reconnect();
  }

  mqttClient.loop();
  #endif
}

void onceOnlyTask1()
{
  blynkTimer.restartTimer(task1Time);
}

void onceOnlyTask2()
{
  blynkTimer.restartTimer(task2Time);
}

void onceOnlyTask3()
{
  blynkTimer.restartTimer(task3Time);
}


void readMeterData()
{
  tmpVol = sdm.readVal(SDM220T_VOLTAGE);
  delay(50);
  tmpAmp = sdm.readVal(SDM230_CURRENT);
  delay(50);
  tmpWat = sdm.readVal(SDM230_POWER);
  delay(50);
  tmpFre = sdm.readVal(SDM230_FREQUENCY);
  delay(50);
  tmpEne = sdm.readVal(SDM230_TOTAL_ACTIVE_ENERGY);
  delay(50);
  tmpPf = sdm.readVal(SDM230_POWER_FACTOR);
  delay(50);
  tmpMaxPowerDemand = sdm.readVal(SDM230_MAXIMUM_SYSTEM_POWER_DEMAND);

  if (!isnan(tmpVol)) {
    digitalWrite(BUILTIN_LED, LOW);
    Serial.println();
    Serial.print("Voltage:   ");
    Serial.print(tmpVol);
    Serial.println("V");

    Serial.print("Current:   ");
    Serial.print(tmpAmp);
    Serial.println("A");

    Serial.print("Power:     ");
    Serial.print(tmpWat);
    Serial.println("W");

    Serial.print("Frequency: ");
    Serial.print(tmpFre);
    Serial.println("Hz");

    Serial.print("Total Energy: ");
    Serial.print(tmpEne);
    Serial.println("kWh");

    Serial.print("Power Factor: ");
    Serial.print(tmpPf);
    Serial.println("");

    Serial.print("Maximum System Power Demand: ");
    Serial.print(tmpMaxPowerDemand);
    Serial.println("W");

    voltage += tmpVol;
    amp += tmpAmp;
    watt += tmpWat;
    freq = tmpFre;
    energy = tmpEne;
    pf = tmpPf;
    MaxPowerDemand = tmpMaxPowerDemand;
    readCount++;
  }
  delay(100);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(100);
  digitalWrite(BUILTIN_LED, LOW);
}


void sendThingSpeak()
{
  if(readCount != 0) {
    voltage = voltage / readCount;
    amp = amp / readCount;
    watt = watt / readCount;
  }

    Serial.println("Sending data to thingspeak");
    Serial.print("Voltage:   ");
    Serial.print(voltage);
    Serial.println("V");

    Serial.print("Current:   ");
    Serial.print(amp);
    Serial.println("A");

    Serial.print("Power:     ");
    Serial.print(watt);
    Serial.println("W");

    Serial.print("Frequency: ");
    Serial.print(freq);
    Serial.println("Hz");

    Serial.print("Total Energy: ");
    Serial.print(energy);
    Serial.println("kWh");




    ThingSpeak.setField( 1, voltage );
    ThingSpeak.setField( 2, amp );
    ThingSpeak.setField( 3, watt);
    ThingSpeak.setField( 4, freq );
    ThingSpeak.setField( 5, energy);

    int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
    Serial.println(writeSuccess);
    Serial.println();

    voltage = 0;
    amp = 0;
    watt = 0;
    readCount = 0;
}

void sendThingsBoard()
{
  readMeterData();
  if (readCount != 0) {
    // Just debug messages
    Serial.print( "Sending data to thingsboard: [" );

    Serial.print( "]   -> " );

    // Prepare a JSON payload string
    String payload = "{";
    payload += "\"Volt\":"; payload += voltage / readCount; payload += ",";
    payload += "\"Current\":"; payload += amp / readCount; payload += ",";
    payload += "\"Watt\":"; payload += watt / readCount; payload += ",";
    // payload += "\"Frequency\":"; payload += freq; payload += ",";
    payload += "\"Energy\":"; payload += energy; payload += ",";
    payload += "\"PF\":"; payload += pf; payload += ",";
    payload += "\"Max Power Demand\":"; payload += MaxPowerDemand;
    payload += "}";

    // Send payload
    char attributes[100];
    payload.toCharArray( attributes, 100 );
    mqttClient.publish( "v1/devices/me/telemetry", attributes );
    Serial.println( attributes );
    Serial.println();
    delay(100);
    sendStatus();
  }
}

void sendStatus()
{
  String payload = "{";
  if (amp > 0.0) {
    payload += "\"Active\":"; payload += true;
  }
  else if (amp == 0.0) {
    payload += "\"Active\":"; payload += false;
  }

  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  mqttClient.publish( "v1/devices/me/telemetry", attributes );
  Serial.println( attributes );
  Serial.println();
}

void setupMqtt()
{
  #ifdef THINGSBOARD
  Serial.println("FARM Server : "+String(thingsboardServer)+" mqtt port: "+String(mqttport));
  mqttClient.setServer(thingsboardServer, mqttport);  // default port 1883, mqtt_server, thingsboardServer
  #else
  mqttClient.setServer(mqtt_server, mqttport);
  #endif
  mqttClient.setCallback(callback);
  if (mqttClient.connect(myRoom, token, NULL)) {
    Serial.print("mqtt connected : ");
    #ifdef THINGSBOARD
    Serial.println(thingsboardServer);  // mqtt_server
    #else
    Serial.println(mqtt_server);  // mqtt_server
    #endif
    mqttClient.subscribe("v1/devices/me/rpc/request/+");
    Serial.println();
  }
}


void reconnect()
{
  // Loop until we're reconnected
  int status = WL_IDLE_STATUS;

  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to AP");
    }
    // Attempt to connect
    #ifdef THINGSBOARD
    if (mqttClient.connect(myRoom, token, NULL)) {  // connect to thingsboards
    #else
    if (mqttClient.connect(myRoom, mqtt_user, mqtt_password)) {  // connect to thingsboards
    #endif
      Serial.print("connected : ");
      Serial.println(thingsboardServer); // mqtt_server
      mqttClient.subscribe("v1/devices/me/rpc/request/+");



      // Once connected, publish an announcement...
      // mqttClient.publish(roomStatus, "hello world");

    } else {
      Serial.print("failed, reconnecting state = ");
      Serial.print(mqttClient.state());
      Serial.print(" try : ");
      Serial.print(mqtt_reconnect+1);
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
    mqtt_reconnect++;
    if (mqtt_reconnect > MAXRETRY) {
      mqtt_reconnect = 0;
      break;
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  int i;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  if (methodName.equals("checkStatus")) {
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), getStatus().c_str());
  }
  else if (methodName.equals("setGpioStatus")) {

  }

}

String getStatus()
{
  // Prepare JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();

  if (amp > 0.0) {
    data["Active"] = true;
  }
  else if (amp == 0.0) {
    data["Active"] = false;
  }

  char payload[256];
  data.printTo(payload, sizeof(payload));

  String strPayload = String(payload);
  Serial.print("Get pump status: ");
  Serial.println(strPayload);

  return strPayload;
}
