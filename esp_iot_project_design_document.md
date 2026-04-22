# ESP Wi-Fi Provisioning + MQTT IoT Device Design Document

## 1. Project Overview

This project implements an IoT device based on **ESP8266** or **ESP32-CAM** firmware that supports:

- first-time provisioning through a local Wi-Fi hotspot and onboard web page
- scanning and displaying nearby Wi-Fi networks
- selecting a target Wi-Fi network and entering credentials
- configuring MQTT broker settings from the setup page
- saving configuration in non-volatile storage
- connecting to the configured Wi-Fi network after provisioning
- connecting to an MQTT broker for publish/subscribe communication
- restoring factory settings using a GPIO button held for 5 seconds

The design is intended for embedded products that need simple field setup without requiring reflashing firmware for network or broker changes.

---

## 2. Goals

### Functional goals

- Device shall create a **SoftAP** hotspot when unprovisioned.
- Device shall host a local setup web page.
- Setup page shall show nearby Wi-Fi access points.
- User shall be able to enter Wi-Fi credentials.
- User shall be able to configure MQTT broker and port.
- Device shall save the configuration persistently.
- Device shall reboot or switch into station mode after provisioning.
- Device shall connect to the configured MQTT broker after Wi-Fi connection is established.
- A factory reset button shall erase saved configuration and return the device to setup mode.

### Non-functional goals

- Keep the design simple enough for ESP8266.
- Allow the same architecture to scale to ESP32 and ESP32-CAM.
- Avoid unnecessary recompilation when field network parameters change.
- Ensure predictable recovery from Wi-Fi or MQTT failures.

---

## 3. Recommended Hardware Options

## Option A: ESP8266

Suitable when:

- cost is important
- camera support is not required
- web UI is simple
- memory usage is kept moderate
- MQTT is lightweight

Pros:

- low cost
- widely available
- many proven libraries and examples

Constraints:

- tighter RAM and flash margins
- limited headroom for complex captive portal pages, TLS, and large feature sets

## Option B: ESP32-CAM

Suitable when:

- camera capability may be needed later
- more memory headroom is desired than ESP8266
- future expansion is expected

Pros:

- better compute and memory headroom than ESP8266
- camera support
- more room for future firmware features

Constraints:

- GPIO availability can be tighter depending on module usage
- pin assignment must be planned carefully

## Recommendation

For a simple Wi-Fi + MQTT device, **ESP8266 is feasible**.
For a more robust and expandable product, **ESP32-based hardware is preferred**.

---

## 4. High-Level System Behavior

The device has two main operating modes:

### Provisioning mode

Used when:

- device is powered on for the first time
- no valid Wi-Fi configuration exists
- saved Wi-Fi connection fails repeatedly
- user triggers factory reset

Behavior:

- start SoftAP
- run local web server
- scan nearby Wi-Fi networks
- present configuration page
- accept Wi-Fi and MQTT settings
- save configuration
- reboot or switch to station mode

### Normal operating mode

Used when:

- valid configuration exists
- device can connect to target Wi-Fi

Behavior:

- connect to configured Wi-Fi in station mode
- initialize MQTT client
- connect to broker
- run application logic
- reconnect Wi-Fi and MQTT if needed

---

## 5. User Setup Flow

1. User powers on the device.
2. Device starts a Wi-Fi hotspot such as `DEVICE_SETUP_xxxx` if not yet provisioned.
3. User connects a phone or laptop to the device hotspot.
4. User opens a browser and reaches the local config page.
5. Device shows nearby Wi-Fi networks.
6. User selects Wi-Fi SSID and enters password.
7. User enters MQTT settings:
   - broker address or IP
   - broker port
   - optional username
   - optional password
   - optional client ID
   - optional topic prefix
8. User submits the form.
9. Device validates and stores configuration.
10. Device switches to station mode and connects to the chosen network.
11. Device starts the MQTT client after Wi-Fi connection succeeds.

---

## 6. Functional Requirements

### 6.1 Wi-Fi provisioning

- Device shall start SoftAP when configuration is missing or invalid.
- Device shall provide a local webpage for setup.
- Device shall scan and list nearby Wi-Fi SSIDs.
- Device shall allow manual SSID entry if needed.
- Device shall store Wi-Fi SSID and password in persistent memory.

### 6.2 MQTT configuration

- Setup page shall allow entering MQTT broker address.
- Setup page shall allow entering MQTT port.
- Setup page may allow entering MQTT username and password.
- Setup page may allow entering client ID and topic prefix.
- MQTT configuration shall be stored persistently.

### 6.3 Boot logic

- On boot, firmware shall load persistent configuration.
- If configuration is valid, device shall attempt station mode connection.
- If connection fails after configured retries, firmware shall return to provisioning mode.

### 6.4 Factory reset

- One GPIO shall be assigned as factory reset input.
- Holding the button for 5 seconds shall erase stored configuration.
- Device shall reboot into provisioning mode after erase.

### 6.5 MQTT runtime

- MQTT shall start only after Wi-Fi is connected.
- Firmware shall reconnect MQTT if broker connection drops.
- Firmware shall reconnect Wi-Fi if network is lost.
- Application logic shall continue to operate according to system policy during reconnect.

---

## 7. Setup Page Fields

Recommended setup page fields:

### Wi-Fi section

- Available Wi-Fi list
- SSID manual entry fallback
- Wi-Fi password

### MQTT section

- Broker address
- Port
- Username (optional)
- Password (optional)
- Client ID (optional)
- Topic prefix (optional)

### Device section (optional)

- Device name
- Location tag
- Firmware version display

### Action buttons

- Scan Wi-Fi
- Save and Connect
- Reset to Defaults

---

## 8. Data Storage Requirements

The following parameters should be stored persistently:

```c
struct DeviceConfig {
    char wifi_ssid[32];
    char wifi_password[64];
    char mqtt_broker[64];
    uint16_t mqtt_port;
    char mqtt_username[32];
    char mqtt_password[32];
    char client_id[32];
    char topic_prefix[64];
    bool provisioned;
    uint32_t crc;
};
```

Storage options:

- ESP8266: EEPROM emulation or flash-based config storage
- ESP32: NVS preferred

Recommended design details:

- include a version field or CRC
- validate stored structure at boot
- use defaults when values are absent or corrupt

---

## 9. Error Handling Strategy

### Wi-Fi failures

- If saved Wi-Fi credentials fail repeatedly, return to provisioning mode.
- Do not immediately erase stored settings on a temporary failure.
- Log failure reason where possible.

### MQTT failures

- Retry broker connection periodically.
- Keep Wi-Fi configuration intact.
- Provide status indication through serial log or LED if available.

### Storage failures

- Detect invalid or corrupted config using versioning or CRC.
- Fall back to provisioning mode.

### Factory reset handling

- Debounce the reset button.
- Confirm 5-second hold before erase.
- Prevent accidental reset on brief presses.

---

## 10. Security Considerations

Minimum practical measures:

- use a non-trivial AP password for provisioning hotspot if required
- sanitize and validate all form inputs
- restrict setup portal to local AP access during provisioning
- avoid exposing stored credentials in the webpage after save

Optional future improvements:

- MQTT over TLS
- captive portal timeout
- admin password for reconfiguration page
- signed firmware update mechanism

For ESP8266, TLS support may be possible but must be evaluated against memory constraints.

---

## 11. Software Architecture Summary

Main firmware modules:

- Boot Manager
- WiFi Manager
- Provisioning Web Server
- Config Manager
- MQTT Manager
- Factory Reset Manager
- Application Logic
- HAL / platform abstraction

Detailed architecture is provided in the companion firmware architecture file.

---

## 12. Suggested Folder Structure

```text
firmware/
├── app/
│   ├── main.cpp
│   ├── boot_manager.cpp
│   ├── wifi_manager.cpp
│   ├── mqtt_manager.cpp
│   ├── config_manager.cpp
│   ├── reset_manager.cpp
│   └── app_logic.cpp
├── include/
│   ├── config.h
│   ├── device_config.h
│   ├── wifi_manager.h
│   ├── mqtt_manager.h
│   ├── reset_manager.h
│   └── app_logic.h
├── web/
│   ├── index.html
│   ├── style.css
│   └── script.js
└── platform/
    ├── hal_gpio.cpp
    ├── hal_storage.cpp
    └── hal_timer.cpp
```

---

## 13. Suggested Development Phases

### Phase 1: Bring-up

- basic boot
- GPIO test
- reset button detection
- serial logging

### Phase 2: Provisioning

- SoftAP startup
- local web server
- Wi-Fi scan endpoint
- save Wi-Fi credentials

### Phase 3: MQTT integration

- add broker/port form fields
- save MQTT parameters
- connect MQTT after Wi-Fi
- publish and subscribe validation

### Phase 4: Recovery behavior

- reconnect logic
- factory reset
- invalid config handling

### Phase 5: Product polish

- captive portal behavior
- status webpage
- LED indicators
- optional OTA support

---

## 14. Risks and Constraints

### ESP8266-specific risks

- memory pressure with large pages or TLS
- limited margin for camera or advanced UI features
- potential instability if reconnect logic is not lightweight

### ESP32-CAM-specific risks

- GPIO conflicts depending on camera use
- careful pin planning required for reset button and peripheral control

### General risks

- unstable Wi-Fi environments
- MQTT broker unreachable conditions
- corrupted config after unexpected power loss during write

Mitigations:

- keep config structure compact
- separate provisioning and runtime logic cleanly
- validate config before use
- implement safe retry and fallback paths

---

## 15. Conclusion

This project is technically feasible on both **ESP8266** and **ESP32-CAM**.
The design is a standard and practical embedded IoT provisioning architecture.
Including MQTT broker and port in the setup page is strongly recommended because it avoids firmware rebuilds when deployment parameters change.

For a production-oriented design:

- use **ESP8266** for a low-cost simple device
- use **ESP32-based hardware** when future expansion, better memory margin, or camera integration is desired

