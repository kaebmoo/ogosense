# OgoSense ESP8266 Firmware

This firmware is designed for ESP8266 microcontrollers to monitor and control environmental conditions using various sensors and actuators. It supports temperature and humidity sensing, relay control, and integration with ThingSpeak and Telegram.

## Features

* **Environmental Sensing:** Reads temperature and humidity data from an SHT30 sensor.
* **Relay Control:** Controls relays based on sensor readings and user-defined settings.
* **Automation Modes:** Supports different automation modes based on temperature, humidity, or both.
* **ThingSpeak Integration:** Logs sensor data and relay status to ThingSpeak for data visualization and analysis.
* **Telegram Integration:** Allows users to monitor device status, control settings, and receive alerts via Telegram Bot.
* **Configuration Storage:** Stores device configuration (temperature/humidity thresholds, options, etc.) in EEPROM for persistence.
* **Wi-Fi Management:** Automatically connects to Wi-Fi networks using WiFiManager.
* **Over-the-Air (OTA) Updates:** (Note: This README doesn't show OTA code, but the project might include it)

## Hardware Requirements

* ESP8266 (e.g., Wemos D1 mini, Pro)
* Wemos SHT30 Shield (connected to D1, D2 pins)
* Wemos Relay Shield (connected to D6, D7 pins)
* (Optional) Dot matrix LED (connected to D5, D7)
* (Optional) Buzzer (connected to D5)
* (Optional) Soil Moisture Sensor

## Software Requirements

* Arduino IDE
* ESP8266 Board Support Package
* Required Libraries (install via Arduino Library Manager):
    * WiFiManager
    * ThingSpeak
    * ESP8266HTTPClient
    * ArduinoJson
    * UniversalTelegramBot
    * WiFiClientSecure
    * Wire
    * WEMOS\_SHT3X
    * SPI
    * Timer
    * MLEDScroll (if using Matrix LED)

## Installation

1.  Download the project source code.
2.  In the Arduino IDE, install the required libraries using the Library Manager.
3.  Install the ESP8266 Board Support Package if you haven't already.
4.  Open the project in the Arduino IDE.
5.  Configure the settings in the `ogosense.h` file (see Configuration section below).
6.  Connect your ESP8266 to your computer.
7.  Select the correct board and port in the Arduino IDE.
8.  Upload the code to your ESP8266.

## Configuration

The firmware's behavior is configured through the `ogosense.h` header file. You'll need to adjust the following settings:

###   Platform Integration

* **ThingSpeak:**
    ```c++
    #ifdef THINGSPEAK
      // ThingSpeak information
      const char thingSpeakAddress= "api.thingspeak.com";
      unsigned long channelID = 0000000;  // Replace with your Channel ID
      char readAPIKey= "YOUR_READ_API_KEY";   // Replace with your Read API Key
      char writeAPIKey= "YOUR_WRITE_API_KEY";  // Replace with your Write API Key
    #endif
    ```
    * `channelID`: The ID of your ThingSpeak channel.
    * `readAPIKey`: The Read API Key for your ThingSpeak channel.
    * `writeAPIKey`: The Write API Key for your ThingSpeak channel.

* **Telegram Bot:**
    ```c++
    const char* telegramToken = "YOUR_TELEGRAM_BOT_TOKEN"; // Replace with your Bot Token
    #define TELEGRAM_CHAT_ID "YOUR_CHAT_ID"  // Replace with your Chat ID
    const char* allowedChatIDs= {"YOUR_CHAT_ID_1", "YOUR_CHAT_ID_2"}; // Replace with allowed Chat IDs
    const int numAllowedChatIDs = sizeof(allowedChatIDs) / sizeof(allowedChatIDs[0]);
    ```
    * `telegramToken`: The API token for your Telegram Bot.
    * `TELEGRAM_CHAT_ID`: The chat ID of the user or group to receive messages.
    * `allowedChatIDs`: An array of chat IDs that are allowed to interact with the bot.

* **Netpie:**
    ```c++
    #ifdef NETPIE
      #define APPID   "YOUR_APP_ID"                  // application id from netpie
      #define KEY     "YOUR_NETPIE_KEY"           // key from netpie
      #define SECRET  "YOUR_NETPIE_SECRET" // secret from netpie

      String ALIAS = "ogosense-device";              // alias name netpie
      char *me = "/ogosense/data";                  // topic set for sensor box
      char *relayStatus1 = "/ogosense/relay/1";     // topic status "1" or "0", "ON" or "OFF"
      char *relayStatus2 = "/ogosense/relay/2";     // topic status "1" or "0", "ON" or "OFF"

      MicroGear microgear(client);
    #endif
    ```
    * `APPID`, `KEY`, `SECRET`: Obtain these from your Netpie account.
    * `ALIAS`: A unique alias for your device on Netpie.
    * `me`, `relayStatus1`, `relayStatus2`: Define the topics used for communication with Netpie.

###   Device Settings

* **Device ID and Secret:**
    ```c++
    const int DEVICE_ID = 12345;  // Replace with a unique Device ID
    const char* INFO_SECRET = "your_secret_code"; // Replace with your secret code
    ```
    * `DEVICE_ID`: A unique identifier for your device.
    * `INFO_SECRET`: A secret code used for authentication when accessing device information.

###   Sensor and Control Parameters

* **Temperature and Humidity Thresholds:**
    ```c++
    #define HIGH_TEMPERATURE 30.0    // Replace with your high temperature threshold
    #define LOW_TEMPERATURE 25.0     // Replace with your low temperature threshold
    #define HIGH_HUMIDITY 60         // Replace with your high humidity threshold
    #define LOW_HUMIDITY 55          // Replace with your low humidity threshold
    ```
    * `HIGH_TEMPERATURE`: Temperature above which the relay may turn on (depending on mode).
    * `LOW_TEMPERATURE`: Temperature below which the relay may turn on (depending on mode).
    * `HIGH_HUMIDITY`: Humidity above which the relay may turn on (depending on mode).
    * `LOW_HUMIDITY`: Humidity below which the relay may turn on (depending on mode).

* **Control Options:**
    ```c++
    #define OPTIONS 1        // Replace with your default option
    #define COOL_MODE 1      // Replace with your default COOL mode
    #define MOISTURE_MODE 0  // Replace with your default MOISTURE mode
    ```
    * `OPTIONS`:
        * `0`: Humidity control only
        * `1`: Temperature control only
        * `2`: Temperature and humidity control
        * `3`: Soil moisture control (if soil moisture sensor is connected)
        * `4`: Temperature or Humidity (Second Relay Control)
    * `COOL_MODE`:
        * `1`: COOL mode (Relay turns ON when temperature is HIGH)
        * `0`: HEAT mode (Relay turns ON when temperature is LOW)
    * `MOISTURE_MODE`:
        * `1`: Moisture mode (Relay turns ON when humidity is LOW)
        * `0`: Dehumidifier mode (Relay turns ON when humidity is HIGH)

###   Relay Pin Definitions

* ```c++
    #if defined(SECONDRELAY) && !defined(MATRIXLED)
      const int RELAY1 = D7;
      const int RELAY2 = D6;
    #else
      const int RELAY1 = D7;
    #endif
    ```

## Usage

1.  After uploading the code and configuring the settings, the ESP8266 will connect to Wi-Fi.
2.  The device will start reading sensor data and controlling the relay(s) based on the configured options and thresholds.
3.  Sensor data and relay status will be sent to ThingSpeak (if enabled).
4.  You can interact with the device via Telegram Bot (if enabled) using the following commands:
    * `/start`:  Displays a welcome message.
    * `/status <device_id>`: Displays the device's status (temperature, humidity, relay status).
    * `/settemp <device_id> <lowTemp> <highTemp>`: Sets the temperature thresholds.
    * `/sethum <device_id> <lowHumidity> <highHumidity>`: Sets the humidity thresholds.
    * `/info <device_id> <secret>`: Displays device configuration information (requires the correct `INFO_SECRET`).

## Important Notes

* Replace all placeholder values in the configuration sections with your actual credentials and settings.
* Ensure that your hardware connections are correct, especially for the SHT30 and Relay Shields.
* This firmware provides a basic framework. You may need to modify or extend it to meet your specific requirements.
* Handle API keys and sensitive information securely. Avoid storing them directly in your code if possible, and consider using environment variables or configuration files.
* Refer to the documentation and examples of the libraries used in this project for more details on their usage.