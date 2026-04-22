#include "config_manager.h"

#include <EEPROM.h>

namespace {

constexpr size_t kEepromSize = 512;

}  // namespace

bool ConfigManager::begin() {
  EEPROM.begin(kEepromSize);
  return true;
}

bool ConfigManager::load(device_config::DeviceConfig &config) {
  EEPROM.get(0, config);

  if (!isStructValid(config)) {
    device_config::clear(config);
    return false;
  }

  return true;
}

bool ConfigManager::save(const device_config::DeviceConfig &config) {
  device_config::DeviceConfig candidate = config;
  candidate.magic = device_config::kMagic;
  candidate.version = device_config::kVersion;
  candidate.provisioned = true;
  candidate.crc32 = calculateCrc32(candidate);

  EEPROM.put(0, candidate);
  if (!EEPROM.commit()) {
    return false;
  }

  return true;
}

bool ConfigManager::clear() {
  for (size_t i = 0; i < kEepromSize; ++i) {
    EEPROM.write(i, 0);
  }

  return EEPROM.commit();
}

uint32_t ConfigManager::calculateCrc32(
    const device_config::DeviceConfig &config) const {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&config);
  const size_t length = sizeof(device_config::DeviceConfig) - sizeof(config.crc32);
  uint32_t crc = 0xFFFFFFFFUL;

  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const bool lsb = (crc & 1U) != 0;
      crc >>= 1U;
      if (lsb) {
        crc ^= 0xEDB88320UL;
      }
    }
  }

  return crc ^ 0xFFFFFFFFUL;
}

bool ConfigManager::isStructValid(
    const device_config::DeviceConfig &config) const {
  if (config.magic != device_config::kMagic) {
    return false;
  }

  if (config.version != device_config::kVersion) {
    return false;
  }

  if (!config.provisioned) {
    return false;
  }

  if (config.wifiSsid[0] == '\0' || config.mqttBroker[0] == '\0' ||
      config.mqttPort == 0) {
    return false;
  }

  return config.crc32 == calculateCrc32(config);
}
