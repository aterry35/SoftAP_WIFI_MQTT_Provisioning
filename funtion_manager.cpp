#include "funtion_manager.h"

#include <cstring>

namespace {

constexpr char kBlinkLedTopic[] = "iot_dev/blink_led";

}  // namespace

void FunctionManager::begin() {
  pinMode(kOnboardLedPin, OUTPUT);
  setOnboardLed(false);
}

bool FunctionManager::handleMqttCommand(const char *topic, const String &command) {
  if (topic == nullptr || strcmp(topic, kBlinkLedTopic) != 0) {
    return false;
  }

  if (command == F("on")) {
    setOnboardLed(true);
    return true;
  }

  if (command == F("off")) {
    setOnboardLed(false);
    return true;
  }

  Serial.println(F("Unsupported LED command. Use 'on' or 'off'."));
  return true;
}

void FunctionManager::setOnboardLed(bool on) {
  const uint8_t outputLevel =
      (on == kOnboardLedActiveLow) ? LOW : HIGH;
  digitalWrite(kOnboardLedPin, outputLevel);
  Serial.printf("Onboard LED is now %s\n", on ? "ON" : "OFF");
}

const char *FunctionManager::blinkLedTopic() { return kBlinkLedTopic; }

