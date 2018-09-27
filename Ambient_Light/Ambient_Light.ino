//Install [claws/BH1750 Library](https://github.com/claws/BH1750) first.


#include <Wire.h>
#include <BH1750.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Timer.h>
#include <Adafruit_NeoPixel.h>
#include <ThingSpeak.h>

#define TRIGGER_PIN D3
#define PIN D4
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);
WiFiClient wifiClient;
Timer timer;

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 432257;
char *writeAPIKey = "ENQ9QDFP7SD3LS17";
char *readAPIKey = "1JK0CKLYIPVH9T1E";


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
  WiFi.begin("Red", "12345678");
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

  ThingSpeak.begin( wifiClient );
  timer.every(60000, readLightLevel);
}


void loop() {

  timer.update();
  

}

void readLightLevel()
{
  uint16_t lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");

  sendThingSpeak(lux);
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

void sendThingSpeak(uint16_t lux)
{
  ThingSpeak.setField( 1,  lux);
  int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
  Serial.print("Return code: ");
  Serial.println(writeSuccess);
  Serial.println();
}
