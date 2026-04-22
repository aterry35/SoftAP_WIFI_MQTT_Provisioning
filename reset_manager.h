#pragma once

#include <Arduino.h>

class ResetManager {
 public:
  void begin(uint8_t pin, bool activeLow, unsigned long holdDurationMs);
  void poll();
  bool consumeFactoryResetRequest();

 private:
  bool isPressed() const;

  uint8_t pin_ = 0;
  bool activeLow_ = true;
  bool configured_ = false;
  bool pressLatched_ = false;
  bool resetRequested_ = false;
  unsigned long holdDurationMs_ = 5000;
  unsigned long pressedAtMs_ = 0;
};

