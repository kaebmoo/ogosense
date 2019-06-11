/*
  MIT License
Version 1.0 2018-01-22
Copyright (c) 2017 kaebmoo gmail com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
 * Hardware
 * Wemos D1 mini, Pro
 * SHT30 Shield
 * Relay Shield
 *
 *
 */


#define TEMPERATURE_SETPOINT 30.0
#define TEMPERATURE_RANGE 2
#define HUMIDITY_SETPOINT 60
#define HUMIDITY_RANGE 5
#define OPTIONS 1
#define COOL_MODE 1
#define MOISTURE_MODE 0


#ifdef SOILMOISTURE
  #define soilMoistureLevel 50
#endif


#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"

/*
 * 
 * Netpie 
 * 
*/
#ifdef NETPIE
#define APPID   "OgoSense"                  // application id from netpie
#define KEY     "sYZknE19LHxr1zJ"           // key from netpie
#define SECRET  "wJOErv6EcU365pnBMpcFLDzcZ" // secret from netpie

String ALIAS = "ogosense-0000";              // alias name netpie
char *me = "/ogosense/1";                  // topic set for sensor box
char *relayStatus1 = "/ogosense/relay/1/status";     // topic status "1" or "0", "ON" or "OFF"
char *relayStatus2 = "/ogosense/relay/2/status";     // topic status "1" or "0", "ON" or "OFF"

MicroGear microgear(client);
#endif
