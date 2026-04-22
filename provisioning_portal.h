#pragma once

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "device_config.h"

class ProvisioningPortal {
 public:
  ProvisioningPortal();

  bool begin(const String &apSsid);
  void stop();
  void loop();
  bool hasSubmittedConfig() const;
  device_config::DeviceConfig takeSubmittedConfig();
  String accessPointSsid() const;

 private:
  void configureRoutes();
  void handlePortalPage();
  void handleScan();
  void handleSave();
  void handleStatus();
  String jsonEscape(const String &input) const;
  void copyString(char *destination, size_t destinationSize,
                  const String &value) const;

  DNSServer dnsServer_;
  ESP8266WebServer server_;
  IPAddress apIp_;
  String accessPointSsid_;
  bool active_;
  bool hasPendingConfig_;
  device_config::DeviceConfig pendingConfig_;
};

