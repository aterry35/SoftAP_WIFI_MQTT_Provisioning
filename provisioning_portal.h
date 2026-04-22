#pragma once

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "device_config.h"

class ProvisioningPortal {
 public:
  enum class PageMode {
    kSetup,
    kStatus,
  };

  ProvisioningPortal();

  bool begin(const String &apSsid);
  void stop();
  void loop();
  bool isActive() const;
  bool hasSubmittedConfig() const;
  device_config::DeviceConfig takeSubmittedConfig();
  String accessPointSsid() const;
  void showSetupPage();
  void showStatusPage();
  PageMode pageMode() const;
  void setConnectionStatus(const String &title, const String &message,
                           bool done, bool error,
                           const String &stationIp = String());
  bool consumeFinishRequest();

 private:
  void configureRoutes();
  void handlePortalPage();
  void handleScan();
  void handleSave();
  void handleStatus();
  void handleFinish();
  void handleSetup();
  String jsonEscape(const String &input) const;
  void copyString(char *destination, size_t destinationSize,
                  const String &value) const;

  DNSServer dnsServer_;
  ESP8266WebServer server_;
  IPAddress apIp_;
  String accessPointSsid_;
  bool active_;
  bool hasPendingConfig_;
  bool finishRequested_;
  PageMode pageMode_;
  String statusTitle_;
  String statusMessage_;
  String stationIp_;
  bool statusDone_;
  bool statusError_;
  device_config::DeviceConfig pendingConfig_;
};
