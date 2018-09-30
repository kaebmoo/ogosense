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


// SDM<9600> sdm;
SDM sdm(Serial, 9600, NOT_A_PIN, SERIAL_8N1, false);

// ThingSpeak information
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 360709;
char *readAPIKey = "5S8UD732WGE1UVO7";
char *writeAPIKey = "D0T6TVCX48SBB9U5";

BlynkTimer blynkTimer;

WiFiClient client;

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  
  sdm.begin();
  
  WiFi.begin("Red", "12345678");
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
  blynkTimer.setInterval(10000L, sendThingSpeak);                   // send data to thingspeak
}

void loop() {
  blynkTimer.run();
  delay(500);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(500);
}

void sendThingSpeak()
{
  float tmpVol = NAN;
  float tmpAmp = NAN;
  float tmpWat = NAN;
  float tmpFre = NAN;
  float tmpEne = NAN;
  
  tmpVol = sdm.readVal(SDM220T_VOLTAGE);
  delay(50);
  tmpAmp = sdm.readVal(SDM230_CURRENT);
  delay(50);
  tmpWat = sdm.readVal(SDM230_POWER);
  delay(50);
  tmpFre = sdm.readVal(SDM230_FREQUENCY);
  delay(50);
  tmpEne = sdm.readVal(SDM230_TOTAL_ACTIVE_ENERGY);
  

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
    
  

  
    ThingSpeak.setField( 1, tmpVol );
    ThingSpeak.setField( 2, tmpAmp );
    ThingSpeak.setField( 3, tmpWat);
    ThingSpeak.setField( 4, tmpFre );
    ThingSpeak.setField( 5, tmpEne);
  
    int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
    Serial.println(writeSuccess);
    Serial.println();
  }
}
