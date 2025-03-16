---

# OgoSense ESP8266 Firmware

OgoSense is a versatile firmware designed for ESP8266 microcontrollers to monitor and control environmental conditions. It reads sensor data, controls relays based on configurable thresholds, and integrates with platforms like ThingSpeak and Telegram for data logging and remote control.

> **License:** MIT License  
> **Version History:**  
> * Version 1.0 – 2018-01-22  
> * Version 2.0 – 2025-03-15

---

## Features

- **Environmental Sensing:**  
  Reads temperature and humidity data from an SHT30 sensor. Supports optional soil moisture sensing if the sensor is connected.

- **Relay Control:**  
  Automatically controls one or two relays based on sensor readings and user-defined thresholds. Supports both temperature-only and humidity-only modes, as well as combined control modes.

- **Automation Modes:**  
  Choose from several modes using an options parameter:  
  - `0`: Humidity control only  
  - `1`: Temperature control only  
  - `2`: Temperature & Humidity control  
  - `3`: Soil Moisture mode (if a soil moisture sensor is connected)  
  - `4`: Temperature or Humidity control (with support for a second relay)

- **Platform Integrations:**  
  - **ThingSpeak Integration:** Logs sensor data and relay status for remote visualization and analysis.  
  - **Telegram Integration:** Monitor device status, update settings, and control the relay manually (available only in manual mode) via Telegram commands.  
  - **(Optional) Netpie Integration:** For IoT messaging, if enabled in your configuration.

- **Configuration Persistence:**  
  Device settings such as temperature/humidity thresholds, operating modes, and API keys are stored in EEPROM to ensure persistence across reboots.

- **Wi-Fi Management:**  
  Uses WiFiManager to automatically connect to available Wi-Fi networks.

- **Additional Hardware Support:**  
  Optional support for:  
  - A second relay for dual control scenarios  
  - Dot matrix LED display (using the MLEDScroll library)  
  - A buzzer for audible feedback  
  - Soil moisture sensors for additional environmental monitoring

---

## Hardware Requirements

- **Microcontroller:**  
  ESP8266 (e.g., Wemos D1 mini or Wemos D1 mini Pro)

- **Sensors & Shields:**  
  - Wemos SHT30 Shield (connects to D1 and D2 pins)  
  - Wemos Relay Shield (connects to D6 and D7 pins)  
  - *(Optional)* Soil Moisture Sensor (if using soil moisture mode)

- **Additional Peripherals (Optional):**  
  - Dot Matrix LED (e.g., for scrolling messages; requires additional pin connections)  
  - Buzzer (connected to D5 for audio alerts)

---

## Software Requirements

- **Development Environment:**  
  Arduino IDE with ESP8266 Board Support Package

- **Required Libraries (install via Arduino Library Manager):**  
  - WiFiManager  
  - ThingSpeak  
  - ESP8266HTTPClient  
  - ArduinoJson  
  - UniversalTelegramBot  
  - WiFiClientSecure  
  - Wire  
  - WEMOS_SHT3X  
  - SPI  
  - EEPROM  
  - Timer  
  - *(Optional)* MLEDScroll (if using a matrix LED)  
  - *(Optional)* MicroGear (if using Netpie integration)

---

## Installation

1. **Download the Source Code:**  
   Clone or download the OgoSense firmware repository.

2. **Install Required Libraries:**  
   Use the Arduino Library Manager to install all required libraries listed above.

3. **Configure the Firmware:**  
   Open the `ogosense.h` file (or configuration section within the code) and update the following settings as needed:
   - **Platform Integration:**  
     - ThingSpeak: Update `channelID`, `readAPIKey`, and `writeAPIKey` with your credentials.  
     - Telegram Bot: Set your `telegramToken`, `TELEGRAM_CHAT_ID`, and allowed chat IDs.  
     - *(Optional)* Netpie: Provide your APPID, KEY, SECRET, and topic details.
   - **Device Settings:**  
     - Set a unique `DEVICE_ID`.  
     - Define an `INFO_SECRET` used for secure device information requests.
   - **Sensor & Control Parameters:**  
     - Adjust temperature (`HIGH_TEMPERATURE` and `LOW_TEMPERATURE`) and humidity (`HIGH_HUMIDITY` and `LOW_HUMIDITY`) thresholds.  
     - Set the default `OPTIONS`, `COOL_MODE`, and `MOISTURE_MODE` for your control strategy.

4. **Connect Your ESP8266:**  
   Use a USB cable to connect the ESP8266 board to your computer.

5. **Select Board & Port:**  
   In the Arduino IDE, choose the correct board type and serial port.

6. **Upload the Code:**  
   Compile and upload the firmware to your ESP8266.

---

## Configuration Details

### Platform Integration

- **ThingSpeak:**  
  ```cpp
  #ifdef THINGSPEAK
    const char thingSpeakAddress[] = "api.thingspeak.com";
    unsigned long channelID = 0000000;  // Replace with your ThingSpeak Channel ID
    char readAPIKey[]  = "YOUR_READ_API_KEY";   // Replace with your Read API Key
    char writeAPIKey[] = "YOUR_WRITE_API_KEY";  // Replace with your Write API Key
  #endif
  ```

- **Telegram Bot:**  
  ```cpp
  const char* telegramToken = "YOUR_TELEGRAM_BOT_TOKEN";  // Replace with your Bot Token
  #define TELEGRAM_CHAT_ID "YOUR_CHAT_ID"                  // Replace with your Chat ID
  const char* allowedChatIDs[] = {"YOUR_CHAT_ID_1", "YOUR_CHAT_ID_2"}; // Allowed chat IDs array
  const int numAllowedChatIDs = sizeof(allowedChatIDs) / sizeof(allowedChatIDs[0]);
  ```

- **Netpie (Optional):**  
  ```cpp
  #ifdef NETPIE
    #define APPID   "YOUR_APP_ID"             // Netpie application ID
    #define KEY     "YOUR_NETPIE_KEY"           // Netpie key
    #define SECRET  "YOUR_NETPIE_SECRET"        // Netpie secret

    String ALIAS = "ogosense-device";           // Device alias on Netpie
    char *me = "/ogosense/data";                // Topic for sensor data
    char *relayStatus1 = "/ogosense/relay/1";   // Topic for Relay #1 status
    char *relayStatus2 = "/ogosense/relay/2";   // Topic for Relay #2 status

    MicroGear microgear(client);
  #endif
  ```

### Device Settings

- **Device Identification:**  
  ```cpp
  const int DEVICE_ID = 12345;  // Set a unique Device ID for each device
  const char* INFO_SECRET = "your_secret_code";  // Secret code for secure info requests
  ```

### Sensor and Control Parameters

- **Temperature & Humidity Thresholds:**  
  ```cpp
  #define HIGH_TEMPERATURE 30.0  // High temperature threshold (°C)
  #define LOW_TEMPERATURE 25.0   // Low temperature threshold (°C)
  #define HIGH_HUMIDITY 60       // High humidity threshold (%)
  #define LOW_HUMIDITY 55        // Low humidity threshold (%)
  ```

- **Control Options:**  
  ```cpp
  #define OPTIONS 1        // Default control option (0 to 4)
  #define COOL_MODE 1      // 1: COOL mode (Relay ON when temperature is high), 0: HEAT mode (Relay ON when temperature is low)
  #define MOISTURE_MODE 0  // 1: Moisture mode (Relay ON when humidity is low), 0: Dehumidifier mode (Relay ON when humidity is high)
  ```
  *Modes available (OPTIONS):*  
  - `0`: Humidity control only  
  - `1`: Temperature control only  
  - `2`: Combined Temperature & Humidity control  
  - `3`: Soil Moisture mode (requires soil moisture sensor)  
  - `4`: Temperature or Humidity control (with optional second relay support)

### Relay Pin Definitions

- **Relay Setup:**  
  ```cpp
  #if defined(SECONDRELAY) && !defined(MATRIXLED)
    const int RELAY1 = D7;
    const int RELAY2 = D6;
  #else
    const int RELAY1 = D7;
  #endif
  ```
  If you have a second relay connected, define `SECONDRELAY` in your configuration.

---

## Usage

1. **Power On & Wi-Fi Connection:**  
   After uploading, the firmware will automatically connect to Wi-Fi using WiFiManager.

2. **Sensor Monitoring & Relay Control:**  
   The ESP8266 continuously reads temperature, humidity (and optionally soil moisture) data from the SHT30 sensor. Based on the set thresholds and mode:
   - In **Auto mode**, the relay(s) are controlled automatically.
   - In **Manual mode**, you can send commands via Telegram to control the relay(s).

3. **Data Logging:**  
   Sensor readings and relay statuses are sent to ThingSpeak (if enabled) for remote monitoring.

4. **Remote Control via Telegram:**  
   Use the following commands (ensure you use the correct `DEVICE_ID`):
   - `/start`  
     Displays a welcome message.
   - `/status <device_id>`  
     Returns current temperature, humidity, and relay status.
   - `/settemp <device_id> <lowTemp> <highTemp>`  
     Update the temperature thresholds.
   - `/sethum <device_id> <lowHumidity> <highHumidity>`  
     Update the humidity thresholds.
   - `/setmode <device_id> <auto|manual>`  
     Switch between automatic and manual relay control.
   - `/setoption <device_id> <option>`  
     Change the control option (0–4).
   - `/info <device_id> <secret>`  
     Returns detailed device configuration (authentication required).
   - `/relay <device_id> <state>`  
     Manually control the relay (only works in manual mode; state should be 1 for ON or 0 for OFF).

---

## Important Notes

- **Configuration:**  
  Ensure you update all placeholder values (API keys, channel IDs, device IDs, etc.) in the configuration section before deployment.

- **Hardware Connections:**  
  Verify that all sensors and shields (SHT30, Relay, optional modules) are connected to the correct pins as specified in the source code.

- **Security:**  
  Handle API keys, tokens, and sensitive information securely. Consider using external configuration methods or environment variables for production systems.

- **Extensibility:**  
  OgoSense provides a flexible framework that can be extended. Modify the source as needed to add features or support additional sensors and peripherals.

---

By following these instructions, you can set up and deploy OgoSense on your ESP8266, enabling robust environmental monitoring and control with remote access via ThingSpeak and Telegram.

Happy coding!

---