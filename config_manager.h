#pragma once

#include "device_config.h"

class ConfigManager {
 public:
  bool begin();
  bool load(device_config::DeviceConfig &config);
  bool save(const device_config::DeviceConfig &config);
  bool clear();

 private:
  uint32_t calculateCrc32(const device_config::DeviceConfig &config) const;
  bool isStructValid(const device_config::DeviceConfig &config) const;
};

