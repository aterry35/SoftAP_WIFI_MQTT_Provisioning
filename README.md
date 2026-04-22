# ESP8266 SoftAP Wi-Fi MQTT Provisioning

ESP8266 firmware for a self-provisioning IoT device that starts in SoftAP mode, serves a local setup page, stores Wi-Fi and MQTT settings in EEPROM, then connects to the configured network and broker.

## Features

- SoftAP hotspot for first-time setup
- Captive-portal-style provisioning page
- Manual or scanned Wi-Fi SSID selection
- MQTT broker host and port configuration from the setup page
- EEPROM-backed config storage with CRC validation
- MQTT test control topic for onboard LED
- MQTT-triggered factory reset
- 5-second hardware button factory reset

## Current MQTT Topics

- `iot_dev/blink_led`
  - `on` -> turn onboard LED on
  - `off` -> turn onboard LED off
- `iot_dev/factory_mode`
  - `set` -> clear saved config and reboot back into provisioning mode

## Provisioning Flow

1. Power on the ESP8266 with no valid config.
2. Device starts a hotspot named `DEVICE_SETUP_<CHIPID>`.
3. Connect to the hotspot and open `http://192.168.4.1/` if the page does not open automatically.
4. Enter:
   - Wi-Fi SSID and password
   - MQTT broker address
   - MQTT port
5. Press `Save and Connect`.
6. Device saves the config, reboots, joins Wi-Fi, and connects to MQTT.

## Hardware Notes

- Target: ESP8266
- Onboard LED uses the common active-low ESP8266 behavior
- Factory reset button is currently configured on `GPIO14`
- Reset button wiring:
  - one side to `GPIO14`
  - one side to `GND`
  - hold for 5 seconds to erase config

If your board uses a different LED pin or button pin, update the constants in `iot_device.ino`.

## Project Files

- `iot_device.ino`: main state flow and provisioning/runtime transitions
- `provisioning_portal.cpp`: SoftAP portal, scan endpoint, save handler
- `config_manager.cpp`: EEPROM read/write and CRC validation
- `mqtt_manager.cpp`: MQTT client connection, publish, subscribe
- `funtion_manager.cpp`: onboard LED test behavior and future device functions
- `reset_manager.cpp`: 5-second button hold detection
- `esp_iot_project_design_document.md`: requirements/design reference
- `esp_firmware_architecture.md`: architecture reference

## Build

Tested with:

- Arduino IDE 2.x bundled CLI
- ESP8266 core `3.1.2`
- `PubSubClient` `2.8.0`

Example compile command:

```bash
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" compile --fqbn esp8266:esp8266:nodemcuv2 .
```

## Test Commands

Publish LED test messages:

```bash
mosquitto_pub -h <broker> -p <port> -t iot_dev/blink_led -m on
mosquitto_pub -h <broker> -p <port> -t iot_dev/blink_led -m off
```

Trigger factory reset:

```bash
mosquitto_pub -h <broker> -p <port> -t iot_dev/factory_mode -m set
```

Clear a retained factory reset command if needed:

```bash
mosquitto_pub -h <broker> -p <port> -t iot_dev/factory_mode -n -r
```

## Security Notes

- This repository does not contain Wi-Fi passwords, MQTT credentials, or device-specific broker settings.
- Runtime network and broker settings are entered through the provisioning portal and stored on the device.
- Local secrets, exported binaries, and editor artifacts should not be committed.
- For production use, add stronger access control to the provisioning portal and consider MQTT authentication/TLS if the board resources allow it.

## Status

Current implementation covers:

- provisioning hotspot
- Wi-Fi setup page
- MQTT broker/port configuration
- MQTT LED control test path
- factory reset by MQTT and button

Planned future work can build on `funtion_manager` for application-specific device behavior.
