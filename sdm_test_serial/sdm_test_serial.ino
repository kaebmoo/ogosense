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

float tmpVol = NAN;
float tmpAmp = NAN;
float tmpWat = NAN;
float tmpFre = NAN;
float tmpEne = NAN;
int readCount = 0;
float voltage = 0.0;
float amp = 0.0;
float watt = 0.0;
float freq = 0.0;
float energy = 0.0;


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
  blynkTimer.setInterval(60000L, sendThingSpeak);                   // send data to thingspeak
  blynkTimer.setInterval(5000L, readMeterData);
}

void loop() {
  blynkTimer.run();
  
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
    voltage += tmpVol;
    amp += tmpAmp;
    watt += tmpWat;
    freq = tmpFre;
    energy = tmpEne;
    readCount++;
  }
  delay(500);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(500);
  digitalWrite(BUILTIN_LED, LOW);
}


void sendThingSpeak()
{
  if(readCount != 0) {
    voltage = voltage / readCount;
    amp = amp / readCount;
    watt = watt / readCount;    
  }

    Serial.println();
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
