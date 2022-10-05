/*
 * Hardware
 * Wemos D1 mini, Pro
 * freetronics watchdog timer https://www.freetronics.com.au/products/watchdog-timer-module
 * 
 * 
 *
 *
 */
 
#include <Timer.h>

Timer t_Watchdog, t_Debug;

void setup() {
  // put your setup code here, to run once:
  pinMode(D6, OUTPUT);
  t_Watchdog.every(10000, resetWatchdog);
  t_Debug.every(60000, DEBUG);
}

void loop() {
  // put your main code here, to run repeatedly:
  t_Watchdog.update();
  t_Debug.update();
}

void resetWatchdog() {
  digitalWrite(D6, HIGH);
  delay(20);
  digitalWrite(D6, LOW);
}

void DEBUG() {
  Serial.println("_DEBUG");
}
