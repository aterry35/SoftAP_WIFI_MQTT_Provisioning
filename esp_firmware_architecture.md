# ESP Firmware Architecture Document

## 1. Purpose

This document defines the firmware architecture for an ESP-based IoT device that supports:

- Wi-Fi provisioning through SoftAP and web portal
- MQTT broker configuration from the setup page
- persistent storage of configuration
- factory reset by GPIO long press
- normal runtime operation after provisioning

This architecture is suitable for ESP8266, ESP32, and ESP32-CAM with small platform-specific adjustments.

---

## 2. Architectural Overview

The firmware is organized into layered modules:

```text
+--------------------------------------------------------------+
|                      Application Layer                       |
|  Boot Manager | Provisioning Manager | MQTT Service          |
|  Reset Manager | Device State Manager | App Logic            |
+--------------------------------------------------------------+
|                    Middleware / Services                     |
|  Web Server | WiFi Manager | Config Manager | Event Manager  |
+--------------------------------------------------------------+
|                Hardware Abstraction / Platform               |
|  GPIO | Timers | Flash/NVS | WiFi Driver | Camera Driver     |
+--------------------------------------------------------------+
```

---

## 3. Core Runtime Modes

The firmware operates as a state machine with the following main states.

### State A: Boot

Responsibilities:

- initialize clocks, GPIO, timers, and storage
- check reset input state
- load saved configuration
- determine startup path

### State B: Provisioning Mode

Responsibilities:

- start SoftAP
- start web server
- handle Wi-Fi scan requests
- accept setup form data
- save configuration
- transition to station mode

### State C: Wi-Fi Connect

Responsibilities:

- connect to configured Wi-Fi network
- retry according to policy
- report status to system state manager
- transition to provisioning mode if connection fails persistently

### State D: MQTT Connect

Responsibilities:

- initialize MQTT client
- connect to broker
- subscribe to required topics
- transition to normal operation on success

### State E: Normal Operation

Responsibilities:

- execute application logic
- maintain Wi-Fi connectivity
- maintain MQTT connectivity
- handle incoming and outgoing MQTT traffic
- monitor reset button

### State F: Factory Reset

Responsibilities:

- erase configuration
- clear provisioned flag
- reboot into provisioning mode

---

## 4. Firmware State Diagram

```text
Power On
   |
   v
[Boot Init]
   |
   +--> Reset button held at boot? -- Yes --> [Erase Config] --> [Provisioning Mode]
   |
   No
   |
   v
[Load Saved Config]
   |
   +--> Valid config available? -- No --> [Provisioning Mode]
   |
   Yes
   |
   v
[Connect Wi-Fi]
   |
   +--> Wi-Fi connect failed after retries --> [Provisioning Mode]
   |
   Success
   |
   v
[Connect MQTT]
   |
   +--> MQTT connect failed --> retry loop / degraded operation
   |
   Success
   |
   v
[Normal Operation]
   |
   +--> Reset button long press --> [Erase Config] --> [Provisioning Mode]
   |
   +--> Wi-Fi lost --> [Connect Wi-Fi]
   |
   +--> MQTT lost --> [Connect MQTT]
```

---

## 5. Module Definitions

## 5.1 Boot Manager

Purpose:

- control startup flow
- initialize all required services
- decide initial state

Interfaces:

- `boot_init()`
- `load_config()`
- `decide_startup_mode()`

Responsibilities:

- initialize serial logging
- initialize storage abstraction
- check reset button status at boot
- validate config structure
- hand off control to system state manager

---

## 5.2 Device State Manager

Purpose:

- own the main firmware state machine
- coordinate transitions between provisioning, connect, and run states

Interfaces:

- `set_state()`
- `get_state()`
- `process_state()`

Responsibilities:

- maintain current state
- enforce transition rules
- manage failure recovery paths

Suggested enum:

```c
typedef enum {
    STATE_BOOT,
    STATE_PROVISIONING,
    STATE_WIFI_CONNECT,
    STATE_MQTT_CONNECT,
    STATE_RUNNING,
    STATE_FACTORY_RESET
} system_state_t;
```

---

## 5.3 Provisioning Manager

Purpose:

- implement the setup-mode workflow

Interfaces:

- `prov_start_ap()`
- `prov_start_webserver()`
- `prov_handle_scan()`
- `prov_handle_save()`
- `prov_stop()`

Responsibilities:

- start hotspot
- expose setup page
- provide Wi-Fi scan results
- collect Wi-Fi and MQTT settings
- validate and pass config to storage manager

Setup page fields should include:

- SSID
- Wi-Fi password
- MQTT broker
- MQTT port
- MQTT username/password optional
- client ID optional
- topic prefix optional

---

## 5.4 WiFi Manager

Purpose:

- abstract all Wi-Fi operations

Interfaces:

- `wifi_start_ap()`
- `wifi_scan()`
- `wifi_connect_sta()`
- `wifi_disconnect()`
- `wifi_status()`

Responsibilities:

- start and stop SoftAP
- scan surrounding SSIDs
- connect in station mode
- provide reconnect logic
- report connectivity status to state manager

Notes:

- AP and STA may run in separate phases
- AP+STA mode may be used if needed during provisioning

---

## 5.5 Web Server Manager

Purpose:

- host the configuration portal
- expose web API endpoints

Interfaces:

- `web_start()`
- `web_stop()`
- `handle_root()`
- `handle_scan()`
- `handle_save()`

Suggested endpoints:

- `/` -> HTML setup page
- `/scan` -> JSON or simple response with SSID list
- `/save` -> save submitted configuration
- `/status` -> optional runtime status page

Responsibilities:

- render setup form
- return Wi-Fi scan results
- accept form submission
- return success or error response

---

## 5.6 Config Manager

Purpose:

- own persistent configuration read/write/validation

Interfaces:

- `config_load()`
- `config_save()`
- `config_clear()`
- `config_validate()`

Responsibilities:

- serialize configuration structure
- validate config version and CRC
- handle defaults when fields are absent
- erase config on reset request

Suggested structure:

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

---

## 5.7 MQTT Manager

Purpose:

- abstract broker connectivity and topic handling

Interfaces:

- `mqtt_init()`
- `mqtt_connect()`
- `mqtt_disconnect()`
- `mqtt_publish()`
- `mqtt_subscribe()`
- `mqtt_loop()`

Responsibilities:

- initialize client using stored broker settings
- connect only after Wi-Fi is ready
- manage reconnect attempts
- receive subscribed messages
- dispatch commands to app logic

Typical dependencies:

- broker host
- broker port
- credentials if required
- topic namespace
- client ID

---

## 5.8 Factory Reset Manager

Purpose:

- detect and execute reset to defaults

Interfaces:

- `reset_init()`
- `reset_poll()`
- `reset_long_press_detected()`

Responsibilities:

- monitor reset GPIO
- debounce signal
- measure press duration
- trigger config erase after 5 seconds
- notify state manager

Recommended behavior:

- short press: ignored or application-specific
- long press >= 5 seconds: erase configuration and reboot or change state

---

## 5.9 Application Logic

Purpose:

- implement product-specific behavior

Examples:

- control relays or GPIO outputs
- read sensors
- publish telemetry
- process MQTT commands
- trigger camera operations on ESP32-CAM

Interfaces:

- `app_init()`
- `app_run()`
- `app_handle_mqtt_message()`

---

## 5.10 HAL / Platform Layer

Purpose:

- isolate board-specific and SDK-specific details

Submodules:

- GPIO HAL
- Timer HAL
- Storage HAL
- Wi-Fi HAL
- Camera HAL if needed

Benefits:

- easier migration between ESP8266 and ESP32
- clearer unit boundaries
- reduced coupling to framework APIs

---

## 6. Data Flow

### Provisioning data flow

```text
User Browser
   -> HTTP request to setup page
   -> ESP Web Server
   -> WiFi Manager scan results
   -> User submits Wi-Fi + MQTT form
   -> Config Manager validates and saves
   -> State Manager moves to Wi-Fi connect state
```

### Runtime data flow

```text
Wi-Fi connected
   -> MQTT Manager connects to broker
   -> App Logic publishes telemetry
   -> MQTT messages received
   -> App Logic processes command
```

### Reset data flow

```text
GPIO button long press
   -> Reset Manager detects 5-second hold
   -> Config Manager clears stored config
   -> State Manager enters provisioning mode
```

---

## 7. Timing and Periodic Tasks

Minimum periodic service loop should cover:

- web server handling
- captive portal DNS handling if used
- Wi-Fi reconnect checks
- MQTT client loop
- reset button polling
- application periodic tasks

### Arduino-style single loop approach

Suitable for ESP8266:

```c
void setup(void) {
    boot_init();
}

void loop(void) {
    reset_poll();
    wifi_service();
    web_service();
    mqtt_loop();
    app_run();
}
```

### FreeRTOS task approach

Suitable for ESP32 / ESP32-CAM:

- system manager task
- provisioning/web task
- MQTT task
- application task
- button monitor task

---

## 8. Failure Recovery Policy

### Wi-Fi failure policy

- retry station connection a defined number of times
- if retry threshold reached, enter provisioning mode

### MQTT failure policy

- retry broker connection periodically
- remain in normal runtime state if Wi-Fi is still available
- avoid erasing config for temporary broker failure

### Corrupt config policy

- validate config on load using version and CRC
- enter provisioning mode if validation fails

### Power-loss during config write

- prefer atomic write pattern if possible
- include versioning and validation to reject partial writes

---

## 9. Suggested Source Tree

```text
src/
├── main.cpp
├── boot_manager.cpp
├── state_manager.cpp
├── provisioning_manager.cpp
├── wifi_manager.cpp
├── web_manager.cpp
├── config_manager.cpp
├── mqtt_manager.cpp
├── reset_manager.cpp
├── app_logic.cpp
└── hal/
    ├── hal_gpio.cpp
    ├── hal_storage.cpp
    ├── hal_timer.cpp
    └── hal_wifi.cpp

include/
├── boot_manager.h
├── state_manager.h
├── provisioning_manager.h
├── wifi_manager.h
├── web_manager.h
├── config_manager.h
├── mqtt_manager.h
├── reset_manager.h
├── app_logic.h
└── device_config.h
```

---

## 10. Design Recommendations

### For ESP8266

- keep HTML page compact
- avoid large buffers
- keep MQTT payloads small
- be cautious with TLS and large certificates
- use a simple cooperative loop design

### For ESP32 / ESP32-CAM

- use NVS for configuration
- use task separation if the system grows
- carefully review GPIO usage if camera is active
- reserve enough stack for web and MQTT tasks

---

## 11. Optional Future Extensions

- OTA firmware update
- runtime status page
- LED status state machine
- multiple broker profiles
- static IP support
- broker certificate upload for TLS
- cloud provisioning fallback
- BLE provisioning for ESP32 variants

---

## 12. Conclusion

This architecture cleanly separates provisioning, connectivity, persistence, reset handling, and application logic.
It supports the exact project flow discussed:

- hotspot-based first-time setup
- Wi-Fi scan and selection
- MQTT broker and port configuration from the setup page
- persistent credentials storage
- runtime MQTT operation
- button-triggered factory reset

It is suitable for both proof-of-concept and product-grade evolution.

