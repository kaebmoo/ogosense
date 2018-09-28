/*
 * 
 * 
 * https://github.com/reaper7/SDM_Energy_Meter
 * 
 */


#include <SDM.h>

// SDM<9600> sdm;
SDM sdm(Serial, 9600, NOT_A_PIN, SERIAL_8N1, false);


void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);
  sdm.begin();
}

void loop() {
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
    
  }
  
  delay(1000);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(1000);
}
