// https://forum.arduino.cc/t/ota-update-in-esp8266/973926/3

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <PatchDNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <StreamString.h>
#include <WiFiUdp.h>
#include <FS.h>

ESP8266WebServer server(80);

const int ledpin = 2;  // I'm running this on an ESP-01
// i have a led connected to this active LOW
// i can't use the internal (pin 1) for the use of Serial

uint8_t ledstatus = 3; // this is keeping track of the state (of our statemachine)

const char *ssid = "Network";
const char *password = "Password";
const char *apname = "esp8266";
const char *appass = "password"; // minimum length 8 characters

bool spiffsmounted = false;

const char* update_path = "/firmware";

const char
*pageheader = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">",
 *htmlhead = "<html><head><title>AP OTA Update Example</title><meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" ></head>",
  *bodystyle = "<body style=\"color: dimgray; background-color: palegoldenrod; font-size: 12pt; font-family: sans-serif;\">",
   *accessIP = "http://192.168.4.1",
    *htmlclose = "</body></html>";

String webIP;

void setup() {
  Serial.begin(115200);
  webIP.reserve(30);  // prevent fragments
  pinMode(ledpin, OUTPUT);
  digitalWrite(ledpin, LOW);

  WiFi.softAP(apname, appass); // start AP mode
  webIP = accessIP;
  Serial.print("Started Access Point \"");
  Serial.print(apname);
  Serial.println("\"");
  Serial.print("With password \"");
  Serial.print(appass);
  Serial.println("\"");
  WiFi.begin(ssid, password);  // attempt starting STA mode
  Serial.println("Attempting to start Station mode");

  uint32_t moment = millis();
  while ((WiFi.status() != WL_CONNECTED) && (millis() < moment + 8000)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    webIP = StationIP();
  }
  else if (WiFi.status() == WL_CONNECT_FAILED) {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println(" Unsuccessful.");
  }
  else if (WiFi.status() == WL_NO_SSID_AVAIL) {
    Serial.print("Network ");
    Serial.print(ssid);
    Serial.println(" not available.");
  }
  WiFi.reconnect();   // reconnect AP after attempting to connect STA


  OTAUpdateSetup();
  spiffsmounted = SPIFFS.begin();

  server.on("/", handleRoot);
  server.on(update_path, HTTP_GET, handleUpdate);
  server.on(update_path, HTTP_POST, handlePostUpdate, handleFileUpload);

  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  yield();
  server.handleClient();
  yield();
  checkLedStatus();
}

void handleUpdate() {

  String s = "";
  s += pageheader;
  s += htmlhead;
  s += bodystyle;
  s += "<h1>OTA Firmware Update</h1>";

  s += "<pre><form method=\"post\" action=\"\" enctype=\"multipart/form-data\">";
  s += "<input type=\"file\" name=\"update\">";
  s += "           <input type=\"submit\" value=\"  Update  \"></form></pre>";
  s += htmlclose;
  server.send(200, "text/html", s);
}


void handlePostUpdate() {

  if (Update.hasError()) {
    StreamString str;
    Update.printError(str);
    str;
    String s = "";
    s += pageheader;
    s += htmlhead;
    s += bodystyle;
    s += "<h1>Update Error </h1>";
    s += str;
    s += htmlclose;
    server.send(200, "text / html", s);
  }
  else {
    String s = "";
    s += pageheader;
    s += htmlhead;
    s += bodystyle;
    s += "<META http-equiv=\"refresh\" content=\"30;URL=/\">Update Success ! Rebooting...\n";
    s += htmlclose;
    server.client().setNoDelay(true);
    server.send(200, "text / html", s);
    delay(100);
    server.client().stop();
    ESP.restart();
  }
}

void handleFileUpload() {

  HTTPUpload& upload = server.upload();
  String updateerror = "";
  if (upload.status == UPLOAD_FILE_START) {
    WiFiUDP::stopAll();
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) { //start with max available size
      StreamString str;
      Update.printError(str);
      updateerror = str;
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      StreamString str;
      Update.printError(str);
      updateerror = str;
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { //true to set the size to the current progress
      StreamString str;
      Update.printError(str);
      updateerror = str;
    }
    else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.end();
    }
    yield();
  }
}


void OTAUpdateSetup() {
  const char* hostName = "ThisAp";  // this is also the mDNS responder name
  
  Serial.print("MDNS responder started, type ");
  Serial.print (hostName);
  Serial.println(".local/ in your browser"); 
                                  
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {

    }
    else { // U_SPIFFS
      type = "filesystem";
      if (spiffsmounted) {
        SPIFFS.end();
        spiffsmounted = false;
      }
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    //  Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    if (!spiffsmounted) spiffsmounted = SPIFFS.begin();
    delay(1000);
    // Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //  Serial.printf("Progress: % u % % \r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[ % u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //  Serial.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void checkLedStatus() {  // our statemachine
  switch (ledstatus) {
    case 0: {
        digitalWrite(ledpin, HIGH);
        return;
      }
    case 1: {
        digitalWrite(ledpin, LOW);
        return;
      }
    case 2: {
        if (ledBlink(500)) ; // here the return value (of ledBlink() ) gets discarded
        return;
      }
    case 3: {
        modulateLed();
        return;
      }
  }
}

void modulateLed() {
  static uint16_t ms = 100;
  static bool increase = true;

  if (!ledBlink(ms)) return;
  if (ms > 250) increase = false;
  if (ms < 20) increase = true;
  if (increase) ms = (ms * 10) / 9;
  else ms = (ms * 9) / 10;
}

bool ledBlink(uint32_t wait) {
  static bool pinstate = false;
  static uint32_t moment = millis();
  if (millis() > moment + wait) {
    pinstate = !pinstate;
    moment = millis();
    if (pinstate) digitalWrite(ledpin, LOW);
    else digitalWrite(ledpin, HIGH);
    return pinstate;  // if pinstate is true and the pinstate has changed, the modulator will change speed
  }
  return false;
}

void handleRoot() {
  String ledstatusupdate;
  if (server.hasArg("led")) {
    if (server.arg("led") == "off") {
      ledstatus = 0;
      ledstatusupdate = "The LED has been turned Off<br>";
    }
    else if (server.arg("led") == "on") {
      ledstatus = 1;
      ledstatusupdate = "The LED has been turned On<br>";
    }
    else if (server.arg("led") == "blink") {
      ledstatus = 2;
      ledstatusupdate = "The LED has been set to Blink<br>";
    }
    else if (server.arg("led") == "modulate") {
      ledstatus = 3;
      ledstatusupdate = "The LED has been set to Modulate<br>";
    }
  }

  String s = "";
  s += pageheader;
  s += htmlhead;
  s += bodystyle;

  s += "<h1>Welcome to ESP webserver</h1><p>From here you can control your LED making it blink or just turn on or off. ";
  s += "</p>";

  s += ledstatusupdate;
  s += "<br>";

  s += "<form action=\"";
  s += webIP;
  s += "\" method=\"get\" name=\"button\">";
  s += "<input type=\"hidden\" name=\"led\" value=\"on\">"; // the hidden parameter gets included
  s += "<input type=\"submit\" value=\" LED ON \"></form><br>"; // the button simply submits the form

  s += "<form action=\"";
  s += webIP;
  s += "\" method=\"get\" name=\"button\">";
  s += "<input type=\"hidden\" name=\"led\" value=\"off\">";
  s += "<input type=\"submit\" value=\" LED OFF\"></form><br>";

  s += "<form action=\"";
  s += webIP;
  s += "\" method=\"get\" name=\"button\">";
  s += "<input type=\"hidden\" name=\"led\" value=\"blink\">";
  s += "<input type=\"submit\" value=\"  BLINK  \"></form><br>";

  s += "<form action=\"";
  s += webIP;
  s += "\" method=\"get\" name=\"button\">";
  s += "<input type=\"hidden\" name=\"led\" value=\"modulate\">";
  s += "<input type=\"submit\" value=\"MODULATE\"></form><br>";

  s += "<form action=\"";
  s += webIP;
  s += update_path;
  s += "\" method=\"get\" name=\"button\">";
  s += "<input type=\"submit\" value=\"UPDATE\"></form><br>";

  s += htmlclose;
  yield();  // not stricktly neccesary, though the String class can be slow
  server.send(200, "text/html", s); //Send web page
}


String StationIP() {
  String stationIP = "http://";
  stationIP += WiFi.localIP().toString();
  return stationIP;
}
