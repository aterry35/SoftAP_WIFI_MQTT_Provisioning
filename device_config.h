#pragma once

#include <Arduino.h>

namespace device_config {

constexpr uint32_t kMagic = 0x45535031UL;  // "ESP1"
constexpr uint16_t kVersion = 1;

struct DeviceConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t mqttPort;
  char wifiSsid[33];
  char wifiPassword[65];
  char mqttBroker[65];
  bool provisioned;
  uint8_t reserved[3];
  uint32_t crc32;
};

inline void clear(DeviceConfig &config) {
  memset(&config, 0, sizeof(DeviceConfig));
  config.magic = kMagic;
  config.version = kVersion;
  config.mqttPort = 1883;
}

}  // namespace device_config

