#include <ESP8266WiFi.h>

#include "config_manager.h"
#include "funtion_manager.h"
#include "mqtt_manager.h"
#include "provisioning_portal.h"
#include "reset_manager.h"

namespace {

enum SystemState {
  STATE_BOOT,
  STATE_PROVISIONING,
  STATE_WIFI_CONNECTING,
  STATE_MQTT_CONNECTING,
  STATE_RUNNING
};

constexpr unsigned long kWifiConnectTimeoutMs = 20000;
constexpr uint8_t kMaxWifiAttempts = 2;
constexpr unsigned long kStatusLogIntervalMs = 5000;
constexpr bool kForceProvisioningModeOnBoot = false;
constexpr unsigned long kFactoryModeArmDelayMs = 10000;
constexpr unsigned long kPortalAutoCloseMs = 45000;
constexpr char kFactoryModeTopic[] = "iot_dev/factory_mode";
constexpr char kFactoryModeCommand[] = "set";
constexpr uint8_t kFactoryResetButtonPin = 14;
constexpr bool kFactoryResetButtonActiveLow = true;
constexpr unsigned long kFactoryResetHoldMs = 5000;

ConfigManager configManager;
FunctionManager functionManager;
ProvisioningPortal portal;
MqttManager mqttManager;
ResetManager resetManager;
device_config::DeviceConfig deviceConfig;
SystemState systemState = STATE_BOOT;
uint8_t wifiAttemptCount = 0;
unsigned long wifiAttemptStartedMs = 0;
unsigned long lastStatusLogMs = 0;
bool factoryResetPending = false;
String factoryResetReason;
unsigned long factoryModeArmedAtMs = 0;
bool portalStatusSessionActive = false;
unsigned long portalAutoCloseAtMs = 0;

String buildAccessPointSsid() {
  String ssid = F("DEVICE_SETUP_");
  ssid += String(ESP.getChipId(), HEX);
  ssid.toUpperCase();
  return ssid;
}

const char *stateName(SystemState state) {
  switch (state) {
    case STATE_PROVISIONING:
      return "PROVISIONING";
    case STATE_WIFI_CONNECTING:
      return "WIFI_CONNECTING";
    case STATE_MQTT_CONNECTING:
      return "MQTT_CONNECTING";
    case STATE_RUNNING:
      return "RUNNING";
    case STATE_BOOT:
    default:
      return "BOOT";
  }
}

void requestFactoryReset(const String &reason) {
  if (factoryResetPending) {
    return;
  }

  factoryResetPending = true;
  factoryResetReason = reason;
  Serial.printf("Factory reset requested: %s\n", factoryResetReason.c_str());
}

void performFactoryReset() {
  factoryResetPending = false;

  Serial.printf("Performing factory reset: %s\n", factoryResetReason.c_str());
  mqttManager.disconnect();
  portal.stop();
  WiFi.disconnect(true);

  if (!configManager.clear()) {
    Serial.println(F("Failed to clear configuration from EEPROM."));
  } else {
    Serial.println(F("Configuration cleared. Restarting into provisioning mode."));
  }

  delay(300);
  ESP.restart();
}

void closePortalSession() {
  if (!portal.isActive()) {
    return;
  }

  portalStatusSessionActive = false;
  portalAutoCloseAtMs = 0;
  Serial.println(F("Closing provisioning hotspot and switching to station mode only."));
  portal.stop();
  WiFi.mode(WIFI_STA);
}

void handleMqttMessage(char *topic, uint8_t *payload, unsigned int length) {
  if (topic == nullptr) {
    return;
  }

  String command;
  command.reserve(length);
  for (unsigned int index = 0; index < length; ++index) {
    command += static_cast<char>(payload[index]);
  }
  command.trim();
  command.toLowerCase();

  Serial.printf("MQTT message received on '%s': '%s'\n", topic, command.c_str());

  if (strcmp(topic, kFactoryModeTopic) == 0) {
    if (static_cast<long>(millis() - factoryModeArmedAtMs) < 0) {
      Serial.println(F("Ignoring early factory mode command during MQTT startup grace period."));
      if (!mqttManager.publish(kFactoryModeTopic, "", true)) {
        Serial.println(F("Warning: failed to clear stale retained factory reset command."));
      }
      return;
    }

    if (command == kFactoryModeCommand) {
      // Clear a retained reset command so it does not trigger again
      // after the device reconnects to MQTT with new settings.
      if (!mqttManager.publish(kFactoryModeTopic, "", true)) {
        Serial.println(F("Warning: failed to clear retained factory reset command."));
      }
      requestFactoryReset(F("MQTT factory mode topic"));
    } else {
      Serial.println(F("Unsupported factory command. Use 'set'."));
    }
    return;
  }

  functionManager.handleMqttCommand(topic, command);
}

void setState(SystemState newState) {
  if (systemState == newState) {
    return;
  }

  systemState = newState;
  Serial.printf("State changed to %s\n", stateName(systemState));
}

void startProvisioningMode() {
  mqttManager.disconnect();
  wifiAttemptCount = 0;
  wifiAttemptStartedMs = 0;
  portalStatusSessionActive = false;
  portalAutoCloseAtMs = 0;

  const String apSsid = buildAccessPointSsid();
  if (!portal.begin(apSsid)) {
    Serial.println(F("Failed to start provisioning portal, rebooting in 2 seconds."));
    delay(2000);
    ESP.restart();
  }

  portal.showSetupPage();
  portal.setConnectionStatus(F("Device Setup"),
                             F("Enter Wi-Fi and MQTT details to continue."),
                             false, false);
  Serial.printf("Provisioning portal active. Connect to '%s' and open http://%s/\n",
                portal.accessPointSsid().c_str(), WiFi.softAPIP().toString().c_str());
  setState(STATE_PROVISIONING);
}

void beginWifiAttempt() {
  if (wifiAttemptCount >= kMaxWifiAttempts) {
    Serial.println(F("Wi-Fi retry limit reached."));
    if (portalStatusSessionActive) {
      portal.setConnectionStatus(F("Wi-Fi connection failed"),
                                 F("Check the Wi-Fi password or SSID, then return to setup and try again."),
                                 false, true);
      portal.showStatusPage();
      portalStatusSessionActive = false;
      portalAutoCloseAtMs = 0;
      setState(STATE_PROVISIONING);
      return;
    }

    startProvisioningMode();
    return;
  }

  ++wifiAttemptCount;
  wifiAttemptStartedMs = millis();

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  if (portalStatusSessionActive) {
    WiFi.mode(WIFI_AP_STA);
    portal.setConnectionStatus(
        F("Connecting to Wi-Fi"),
        String(F("Joining \"")) + deviceConfig.wifiSsid + F("\" (attempt ") +
            String(wifiAttemptCount) + F("/") + String(kMaxWifiAttempts) + F(")."),
        false, false);
  } else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
  WiFi.disconnect();
  delay(100);
  WiFi.begin(deviceConfig.wifiSsid, deviceConfig.wifiPassword);

  Serial.printf("Connecting to Wi-Fi '%s' (attempt %u/%u)\n", deviceConfig.wifiSsid,
                wifiAttemptCount, kMaxWifiAttempts);
  setState(STATE_WIFI_CONNECTING);
}

void startWifiConnectionFlow() {
  mqttManager.disconnect();
  beginWifiAttempt();
}

void serviceWifiConnect() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi connected. IP address: %s\n", WiFi.localIP().toString().c_str());
    if (portalStatusSessionActive) {
      portal.setConnectionStatus(F("Wi-Fi connected"),
                                 F("Wi-Fi is up. Connecting to the MQTT broker now."),
                                 false, false, WiFi.localIP().toString());
    }
    mqttManager.configure(deviceConfig);
    setState(STATE_MQTT_CONNECTING);
    return;
  }

  if (millis() - wifiAttemptStartedMs >= kWifiConnectTimeoutMs) {
    Serial.println(F("Wi-Fi connection attempt timed out."));
    beginWifiAttempt();
    return;
  }

  if (millis() - lastStatusLogMs >= kStatusLogIntervalMs) {
    lastStatusLogMs = millis();
    Serial.println(F("Waiting for Wi-Fi connection..."));
    if (portalStatusSessionActive) {
      portal.setConnectionStatus(
          F("Connecting to Wi-Fi"),
          String(F("Still trying to join \"")) + deviceConfig.wifiSsid + F("\"."),
          false, false);
    }
  }
}

void serviceMqttConnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Wi-Fi lost before MQTT connection completed. Retrying Wi-Fi."));
    startWifiConnectionFlow();
    return;
  }

  if (mqttManager.ensureConnected()) {
    if (!mqttManager.subscribe(FunctionManager::blinkLedTopic())) {
      Serial.printf("MQTT connected, but subscription to '%s' failed.\n",
                    FunctionManager::blinkLedTopic());
      return;
    }

    if (!mqttManager.subscribe(kFactoryModeTopic)) {
      Serial.printf("MQTT connected, but subscription to '%s' failed.\n",
                    kFactoryModeTopic);
      return;
    }

    Serial.printf("MQTT connected to %s:%u\n", deviceConfig.mqttBroker, deviceConfig.mqttPort);
    Serial.printf("Subscribed to MQTT topic '%s'\n", FunctionManager::blinkLedTopic());
    Serial.printf("Subscribed to MQTT topic '%s'\n", kFactoryModeTopic);
    factoryModeArmedAtMs = millis() + kFactoryModeArmDelayMs;
    Serial.printf("Factory mode MQTT command will arm in %lu ms\n", kFactoryModeArmDelayMs);
    if (portalStatusSessionActive) {
      portal.setConnectionStatus(
          F("Device connected"),
          F("Wi-Fi and MQTT are connected. You can disconnect from this hotspot now."),
          true, false, WiFi.localIP().toString());
      portal.showStatusPage();
      portalAutoCloseAtMs = millis() + kPortalAutoCloseMs;
    }
    setState(STATE_RUNNING);
    return;
  }

  if (millis() - lastStatusLogMs >= kStatusLogIntervalMs) {
    lastStatusLogMs = millis();
    Serial.printf("Waiting for MQTT broker %s:%u (state=%d)\n", deviceConfig.mqttBroker,
                  deviceConfig.mqttPort, mqttManager.state());
    if (portalStatusSessionActive) {
      portal.setConnectionStatus(
          F("Connecting to MQTT"),
          String(F("Wi-Fi is connected. Waiting for broker ")) + deviceConfig.mqttBroker +
              F(":") + String(deviceConfig.mqttPort) + F("."),
          false, false, WiFi.localIP().toString());
    }
  }
}

void serviceRunning() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Wi-Fi disconnected. Restarting Wi-Fi connection flow."));
    if (portalStatusSessionActive) {
      portal.setConnectionStatus(F("Wi-Fi disconnected"),
                                 F("The device lost Wi-Fi and is reconnecting."),
                                 false, false);
    }
    startWifiConnectionFlow();
    return;
  }

  if (!mqttManager.connected()) {
    Serial.println(F("MQTT disconnected. Reconnecting."));
    if (portalStatusSessionActive) {
      portal.setConnectionStatus(F("MQTT disconnected"),
                                 F("Wi-Fi is still connected. Reconnecting to MQTT."),
                                 false, false, WiFi.localIP().toString());
    }
    setState(STATE_MQTT_CONNECTING);
    return;
  }

  mqttManager.loop();
}

void handleProvisioningSubmission() {
  if (!portal.hasSubmittedConfig()) {
    return;
  }

  device_config::DeviceConfig submittedConfig = portal.takeSubmittedConfig();
  if (!configManager.save(submittedConfig)) {
    Serial.println(F("Failed to save configuration to EEPROM."));
    startProvisioningMode();
    return;
  }

  device_config::DeviceConfig verifiedConfig;
  if (!configManager.load(verifiedConfig)) {
    Serial.println(F("Saved configuration could not be verified after write."));
    startProvisioningMode();
    return;
  }

  deviceConfig = verifiedConfig;
  Serial.printf("Configuration saved for SSID '%s' and MQTT broker '%s:%u'\n",
                deviceConfig.wifiSsid, deviceConfig.mqttBroker, deviceConfig.mqttPort);
  portalStatusSessionActive = true;
  portalAutoCloseAtMs = 0;
  portal.showStatusPage();
  portal.setConnectionStatus(F("Configuration saved"),
                             F("Keeping the hotspot open while the device joins Wi-Fi and MQTT."),
                             false, false);
  startWifiConnectionFlow();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("ESP8266 provisioning + MQTT firmware starting..."));

  device_config::clear(deviceConfig);
  functionManager.begin();
  resetManager.begin(kFactoryResetButtonPin, kFactoryResetButtonActiveLow,
                     kFactoryResetHoldMs);
  mqttManager.setMessageCallback(handleMqttMessage);

  if (!configManager.begin()) {
    Serial.println(F("EEPROM initialization failed. Rebooting."));
    delay(2000);
    ESP.restart();
  }

  if (!kForceProvisioningModeOnBoot && configManager.load(deviceConfig)) {
    Serial.printf("Loaded saved config. SSID='%s', MQTT='%s:%u'\n", deviceConfig.wifiSsid,
                  deviceConfig.mqttBroker, deviceConfig.mqttPort);
    portalStatusSessionActive = false;
    portalAutoCloseAtMs = 0;
    startWifiConnectionFlow();
  } else {
    if (kForceProvisioningModeOnBoot) {
      Serial.println(F("Force provisioning mode is enabled. Ignoring saved config."));
    } else {
      Serial.println(F("No valid saved config found. Starting provisioning mode."));
    }
    startProvisioningMode();
  }
}

void loop() {
  if (portal.isActive()) {
    portal.loop();
  }

  resetManager.poll();
  if (resetManager.consumeFactoryResetRequest()) {
    requestFactoryReset(F("Physical button hold"));
  }

  if (factoryResetPending) {
    performFactoryReset();
    return;
  }

  if (portal.consumeFinishRequest()) {
    closePortalSession();
  }

  if (portalStatusSessionActive && portalAutoCloseAtMs != 0 &&
      static_cast<long>(millis() - portalAutoCloseAtMs) >= 0) {
    Serial.println(F("Auto-closing provisioning hotspot after successful handoff."));
    closePortalSession();
  }

  switch (systemState) {
    case STATE_PROVISIONING:
      handleProvisioningSubmission();
      break;

    case STATE_WIFI_CONNECTING:
      serviceWifiConnect();
      break;

    case STATE_MQTT_CONNECTING:
      serviceMqttConnect();
      mqttManager.loop();
      break;

    case STATE_RUNNING:
      serviceRunning();
      break;

    case STATE_BOOT:
    default:
      break;
  }

  yield();
}
