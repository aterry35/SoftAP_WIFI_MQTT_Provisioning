#pragma once

#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#include "device_config.h"

class MqttManager {
 public:
  using MessageCallback = void (*)(char *topic, uint8_t *payload,
                                   unsigned int length);

  MqttManager();

  void configure(const device_config::DeviceConfig &config);
  void setMessageCallback(MessageCallback callback);
  bool ensureConnected();
  bool publish(const char *topic, const char *payload, bool retained = false);
  bool subscribe(const char *topic);
  void loop();
  bool connected();
  void disconnect();
  int state();

 private:
  WiFiClient wifiClient_;
  PubSubClient client_;
  String broker_;
  uint16_t port_;
  unsigned long lastAttemptMs_;
  MessageCallback callback_;
};
