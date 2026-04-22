#include "reset_manager.h"

void ResetManager::begin(uint8_t pin, bool activeLow, unsigned long holdDurationMs) {
  pin_ = pin;
  activeLow_ = activeLow;
  holdDurationMs_ = holdDurationMs;
  configured_ = true;
  pressLatched_ = false;
  resetRequested_ = false;
  pressedAtMs_ = 0;

  pinMode(pin_, activeLow_ ? INPUT_PULLUP : INPUT);
}

void ResetManager::poll() {
  if (!configured_) {
    return;
  }

  if (!isPressed()) {
    pressedAtMs_ = 0;
    pressLatched_ = false;
    return;
  }

  const unsigned long now = millis();
  if (pressedAtMs_ == 0) {
    pressedAtMs_ = now;
    return;
  }

  if (!pressLatched_ && now - pressedAtMs_ >= holdDurationMs_) {
    pressLatched_ = true;
    resetRequested_ = true;
    Serial.println(F("Factory reset button hold detected."));
  }
}

bool ResetManager::consumeFactoryResetRequest() {
  const bool requested = resetRequested_;
  resetRequested_ = false;
  return requested;
}

bool ResetManager::isPressed() const {
  if (!configured_) {
    return false;
  }

  const int level = digitalRead(pin_);
  return activeLow_ ? (level == LOW) : (level == HIGH);
}

