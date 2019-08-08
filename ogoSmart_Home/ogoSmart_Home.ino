/*
  Simple example for receiving
  
  https://github.com/sui77/rc-switch/
*/

#include <RCSwitch.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <BlynkSimpleEsp8266.h>
#include <Timer.h>  // https://playground.arduino.cc/Code/Timer/ 

RCSwitch mySwitch = RCSwitch();
BlynkTimer timerBlynk;
Timer timer;

int count = 0;
int timerID = -1;
int ledEvent;

void setup() {
  Serial.begin(9600);
  pinMode(D4, OUTPUT);
  mySwitch.enableReceive(0);  // Receiver on interrupt 0 => that is pin #2
  ledEvent = timer.oscillate(D4, 1000, HIGH);
}

void loop() {
  /*
  if (mySwitch.available()) {
    
    Serial.print("Received ");
    Serial.print( mySwitch.getReceivedValue() );
    Serial.print(" / ");
    Serial.print( mySwitch.getReceivedBitlength() );
    Serial.print("bit ");
    Serial.print("Protocol: ");
    Serial.println( mySwitch.getReceivedProtocol() );

    mySwitch.resetAvailable();
  }
  */
  

  if (mySwitch.available()) {
    Serial.print("Received ");
    Serial.print( mySwitch.getReceivedValue() );
    Serial.print(" / ");
    Serial.print( mySwitch.getReceivedBitlength() );
    Serial.print("bit ");
    Serial.print("delay ");
    Serial.print(mySwitch.getReceivedDelay());
    Serial.print(" / ");
    // Serial.print("raw data ");
    // Serial.print(mySwitch.getReceivedRawdata());
    // Serial.print(" / ");
    Serial.print("Protocol: ");
    Serial.println( mySwitch.getReceivedProtocol() );
    count++;
    mySwitch.resetAvailable();
  }

  if (count >= 10) {
    count = 0;
    Serial.println("Triger : Power On");
    timer.stop(ledEvent);
    digitalWrite(D4, LOW);
    if (timerID != -1) {
      timer.stop(timerID);
    }
    Serial.println("Timer start");
    timerID = timer.after(10000, stopPower);
  }
  timer.update();
}

void stopPower()
{
  Serial.println("stop the power");
  digitalWrite(D4, HIGH);
  timer.oscillate(D4, 500, HIGH, 5);
}
