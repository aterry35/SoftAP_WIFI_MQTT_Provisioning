#pragma once

#include <Arduino.h>

class FunctionManager {
 public:
  void begin();
  bool handleMqttCommand(const char *topic, const String &command);
  void setOnboardLed(bool on);
  static const char *blinkLedTopic();

 private:
  static constexpr bool kOnboardLedActiveLow = true;
#ifdef LED_BUILTIN
  static constexpr uint8_t kOnboardLedPin = LED_BUILTIN;
#else
  static constexpr uint8_t kOnboardLedPin = 2;
#endif
};

