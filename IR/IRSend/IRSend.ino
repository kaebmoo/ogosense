/* IRremoteESP8266: IRsendDemo - demonstrates sending IR codes with IRsend.
 *
 * Version 1.0 April, 2017
 * Based on Ken Shirriff's IrsendDemo Version 0.1 July, 2009,
 * Copyright 2009 Ken Shirriff, http://arcfn.com
 *
 * An IR LED circuit *MUST* be connected to the ESP8266 on a pin
 * as specified by IR_LED below.
 *
 * TL;DR: The IR LED needs to be driven by a transistor for a good result.
 *
 * Suggested circuit:
 *     https://github.com/markszabo/IRremoteESP8266/wiki#ir-sending
 *
 * Common mistakes & tips:
 *   * Don't just connect the IR LED directly to the pin, it won't
 *     have enough current to drive the IR LED effectively.
 *   * Make sure you have the IR LED polarity correct.
 *     See: https://learn.sparkfun.com/tutorials/polarity/diode-and-led-polarity
 *   * Typical digital camera/phones can be used to see if the IR LED is flashed.
 *     Replace the IR LED with a normal LED if you don't have a digital camera
 *     when debugging.
 *   * Avoid using the following pins unless you really know what you are doing:
 *     * Pin 0/D3: Can interfere with the boot/program mode & support circuits.
 *     * Pin 1/TX/TXD0: Any serial transmissions from the ESP8266 will interfere.
 *     * Pin 3/RX/RXD0: Any serial transmissions to the ESP8266 will interfere.
 *   * ESP-01 modules are tricky. We suggest you use a module with more GPIOs
 *     for your first time. e.g. ESP-12 etc.
 */

#ifndef UNIT_TEST
#include <Arduino.h>
#endif
#include <IRremoteESP8266.h>
#include <IRsend.h>

#define IR_LED D3  


IRsend irsend(IR_LED);  // Set the GPIO to be used to sending the message.

// Example of data captured by IRrecvDumpV2.ino
/*
uint16_t rawData[67] = {9000, 4500, 650, 550, 650, 1650, 600, 550, 650, 550,
                        600, 1650, 650, 550, 600, 1650, 650, 1650, 650, 1650,
                        600, 550, 650, 1650, 650, 1650, 650, 550, 600, 1650,
                        650, 1650, 650, 550, 650, 550, 650, 1650, 650, 550,
                        650, 550, 650, 550, 600, 550, 650, 550, 650, 550,
                        650, 1650, 600, 550, 650, 1650, 650, 1650, 650, 1650,
                        650, 1650, 650, 1650, 650, 1650, 600};
*/                    

uint16_t rawData[135] = { 4516, 4504,  554, 1686,  584, 1656,  528, 1712,  556, 566,  
                        582, 540,  550, 570,  552, 566,  554, 566,  528, 1712,  
                        528, 1712,  554, 1686,  556, 566,  528, 594,  552, 566,  
                        556, 566,  554, 566,  552, 568,  554, 1686,  556, 566,  
                        552, 570,  552, 568,  526, 594,  550, 570,  554, 568,  
                        554, 1686,  582, 540,  556, 1684,  556, 1686,  552, 1688,  
                        554, 1686,  556, 1684,  554, 1688,  558, 46854,  4546, 4476,  
                        554, 1686,  582, 1658,  556, 1684,  554, 568,  580, 540,  
                        552, 568,  550, 568,  556, 566,  554, 1686,  528, 1712,  
                        554, 1686,  556, 566,  554, 566,  582, 536,  554, 568,  
                        554, 566,  580, 540,  552, 1688,  556, 566,  580, 540,  
                        552, 568,  556, 566,  526, 594,  550, 570,  552, 1686,  
                        528, 594,  582, 1658,  554, 1688,  554, 1684,  556, 1684,  
                        556, 1686,  556, 1686,  554};

void setup() {
  irsend.begin();
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
}

void loop() {
#if SEND_NEC
  Serial.println("NEC");
  irsend.sendNEC(0x00FFE01FUL, 32);
#endif  // SEND_NEC
  delay(2000);
#if SEND_SONY
  Serial.println("Sony");
  irsend.sendSony(0xa90, 12, 2);
#endif  // SEND_SONY
  delay(2000);

// #if SEND_RAW
//   Serial.println("a rawData capture from IRrecvDumpV2");
//   irsend.sendRaw(rawData, 137, 38);  // Send a raw data capture at 38kHz.
// #endif  // SEND_RAW

#if SEND_SAMSUNG
  Serial.println("SAMSUNG");
  irsend.sendSAMSUNG(0xE0E040BF, 32);
#endif
  delay(2000);
}

