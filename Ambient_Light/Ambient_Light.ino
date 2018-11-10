//Install [claws/BH1750 Library](https://github.com/claws/BH1750) first.

#define THINGSBOARD

#include <Wire.h>
#include <BH1750.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Timer.h>
#include <Adafruit_NeoPixel.h>
#include <ThingSpeak.h>
#include <ArduinoJson.h>
#include <MicroGear.h>
#include <MQTTClient.h>
#include <PubSubClient.h>
#include <SHA1.h>
#include <AuthClient.h>
#include <debug.h>

#define TRIGGER_PIN D3
#define PIN D4
#define GPIO0 16
#define GPIO2 2

#define GPIO0_PIN D0
#define GPIO2_PIN D4
// We assume that all GPIOs are LOW
boolean gpioState[] = {false, false};
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);
WiFiClient sensorClient;
Timer timer;

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 432257;
char *writeAPIKey = "ENQ9QDFP7SD3LS17";
char *readAPIKey = "1JK0CKLYIPVH9T1E";

MicroGear microgear(sensorClient);              // declare microgear object
PubSubClient mqttClient(sensorClient);

char *myRoom = "sensor/light/1";
int mqtt_reconnect = 0;

#define TOKEN "CxBiywwBk1FyjIXBxzWF"  // device token a45GVxXw4HnJb6SwdwUT  box: UpF71PeawEvCvbY3cPwH pi0w: kgXTXXF5eceFJZ3V2WEw
#define MQTTPORT  1883 // 1883 or 1888
char thingsboardServer[] = "box.greenwing.email";
#define SENDINTERVAL  10000  // send data interval time

unsigned long lastSend;
const int MAXRETRY=30;

/*

  BH1750 can be physically configured to use two I2C addresses:
    - 0x23 (most common) (if ADD pin had < 0.7VCC voltage)
    - 0x5C (if ADD pin had > 0.7VCC voltage)

  Library uses 0x23 address as default, but you can define any other address.
  If you had troubles with default value - try to change it to 0x5C.

*/
BH1750 lightMeter(0x23);

void setup(){

  Serial.begin(115200);

  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin();
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);

  /*

    BH1750 has six different measurement modes. They are divided in two groups;
    continuous and one-time measurements. In continuous mode, sensor continuously
    measures lightness value. In one-time mode the sensor makes only one
    measurement and then goes into Power Down mode.

    Each mode, has three different precisions:

      - Low Resolution Mode - (4 lx precision, 16ms measurement time)
      - High Resolution Mode - (1 lx precision, 120ms measurement time)
      - High Resolution Mode 2 - (0.5 lx precision, 120ms measurement time)

    By default, the library uses Continuous High Resolution Mode, but you can
    set any other mode, by passing it to BH1750.begin() or BH1750.configure()
    functions.

    [!] Remember, if you use One-Time mode, your sensor will go to Power Down
    mode each time, when it completes a measurement and you've read it.

    Full mode list:

      BH1750_CONTINUOUS_LOW_RES_MODE
      BH1750_CONTINUOUS_HIGH_RES_MODE (default)
      BH1750_CONTINUOUS_HIGH_RES_MODE_2

      BH1750_ONE_TIME_LOW_RES_MODE
      BH1750_ONE_TIME_HIGH_RES_MODE
      BH1750_ONE_TIME_HIGH_RES_MODE_2

  */

  // begin returns a boolean that can be used to detect setup problems.
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else {
    Serial.println(F("Error initialising BH1750"));
  }

  pinMode(TRIGGER_PIN, INPUT);

  pixels.begin(); // This initializes the NeoPixel library.
  pixels.setBrightness(64);

  pixels.setPixelColor(0, 255, 255, 0); // yellow #FFFF00
  pixels.show();

  Serial.println();
  Serial.println();
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.psk());
  String SSID = WiFi.SSID();
  String PSK = WiFi.psk();
  WiFi.begin("Red1", "12345678");
  Serial.print("Connecting");
  Serial.println();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      ondemandWiFi();
    }
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  pixels.setPixelColor(0, 0, 0, 255); // blue #0000FF
  pixels.show();
  delay(1000);
  pixels.setPixelColor(0, 0, 0, 0);
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, 0, 0, 255); // blue #0000FF
  pixels.show();
  delay(1000);
  pixels.setPixelColor(0, 0, 0, 0);
  pixels.show();

  ThingSpeak.begin( sensorClient );
  setup_mqtt();
  readAndSendLightLevel();
  // timer.every(60000, readAndSendLightLevel);
  lastSend = 0;
}


void loop() {

  timer.update();
  if ( !mqttClient.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > SENDINTERVAL ) { // Update and send only after SENDINTERVAL seconds
    readAndSendLightLevel();
    lastSend = millis();
  }

  mqttClient.loop();
}

void readAndSendLightLevel()
{
  Serial.println("Collecting ambient light data.");

  uint16_t lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");

  sendThingsBoard(lux);
  // sendThingSpeak(lux);  // be careful timeout in sending data to thingspeak
}

void sendThingsBoard(uint16_t lux)
{
  // Just debug messages
  Serial.print( "Sending ambient light : [" );
  Serial.print( lux );
  Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"lux\":"; payload += lux; payload += ",";
  payload += "\"active\":"; payload += true;
  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  mqttClient.publish( "v1/devices/me/telemetry", attributes );
  Serial.println( attributes );
  Serial.println();
}

void sendThingSpeak(uint16_t lux)
{
  Serial.println("Sending to thingspeak");
  ThingSpeak.setField( 1,  lux);
  int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
  Serial.print("Return code: ");
  Serial.println(writeSuccess);
  Serial.println();
}

void ondemandWiFi()
{
    WiFiManager wifiManager;

    pixels.setPixelColor(0, 255, 0, 0); // red #FF0000
    pixels.show();
    if (!wifiManager.startConfigPortal("ogoSense")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    pixels.setPixelColor(0, 0, 0, 255); // blue #0000FF
    pixels.show();
    delay(1000);
    pixels.setPixelColor(0, 0, 0, 0);
    pixels.show();
    delay(500);
    pixels.setPixelColor(0, 0, 0, 255); // blue #0000FF
    pixels.show();
    delay(1000);
    pixels.setPixelColor(0, 0, 0, 0);
    pixels.show();
}


void setup_mqtt() {
  #ifdef THINGSBOARD
  mqttClient.setServer(thingsboardServer, MQTTPORT);  // default port 1883, mqtt_server, thingsboardServer
  #else
  mqttClient.setServer(mqtt_server, MQTTPORT);
  #endif
  mqttClient.setCallback(callback);
  if (mqttClient.connect(myRoom, TOKEN, NULL)) {
    // mqttClient.publish(roomStatus,"hello world");

    // mqttClient.subscribe(setmaxtemp);
    // mqttClient.subscribe(setmintemp);
    // mqttClient.subscribe(setmaxhumidity);
    // mqttClient.subscribe(setminhumidity);

    Serial.print("mqtt connected : ");
    Serial.println(thingsboardServer);  // mqtt_server
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

  if (methodName.equals("getGpioStatus")) {
    // Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), get_gpio_status().c_str());
  } else if (methodName.equals("setGpioStatus")) {
    // Update GPIO status and reply
    set_gpio_status(data["params"]["pin"], data["params"]["enabled"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    mqttClient.publish(responseTopic.c_str(), get_gpio_status().c_str());
    mqttClient.publish("v1/devices/me/attributes", get_gpio_status().c_str());
  }

}

String get_gpio_status()
{
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();

  data[String(GPIO0_PIN)] = gpioState[0] ? true : false;
  data[String(GPIO2_PIN)] = gpioState[1] ? true : false;

  char payload[256];
  data.printTo(payload, sizeof(payload));

  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);

  return strPayload;
}

void set_gpio_status(int pin, boolean enabled) {
  if (pin == GPIO0_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO0, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[0] = enabled;
  }
  else if (pin == GPIO2_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO2, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[1] = enabled;
  }

}


void reconnect() {
  // Loop until we're reconnected


  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    #ifdef THINGSBOARD
    if (mqttClient.connect(myRoom, TOKEN, NULL)) {  // connect to thingsboards
    #else
    if (mqttClient.connect(myRoom, mqtt_user, mqtt_password)) {  // connect to thingsboards
    #endif
      Serial.print("connected : ");
      Serial.println(thingsboardServer); // mqtt_server



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
