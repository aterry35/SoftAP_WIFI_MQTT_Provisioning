#include "mqtt_manager.h"

namespace {

constexpr unsigned long kReconnectIntervalMs = 5000;

String buildClientId() {
  String clientId = F("esp8266-");
  clientId += String(ESP.getChipId(), HEX);
  return clientId;
}

String buildStatusTopic() {
  String topic = F("devices/");
  topic += String(ESP.getChipId(), HEX);
  topic += F("/status");
  return topic;
}

}  // namespace

MqttManager::MqttManager()
    : client_(wifiClient_),
      broker_(),
      port_(1883),
      lastAttemptMs_(0),
      callback_(nullptr) {
  client_.setKeepAlive(30);
  client_.setSocketTimeout(5);
  client_.setBufferSize(256);
}

void MqttManager::configure(const device_config::DeviceConfig &config) {
  broker_ = config.mqttBroker;
  port_ = config.mqttPort;
  client_.setServer(broker_.c_str(), port_);
}

void MqttManager::setMessageCallback(MessageCallback callback) {
  callback_ = callback;
  client_.setCallback(callback_);
}

bool MqttManager::ensureConnected() {
  if (client_.connected()) {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED || broker_.isEmpty() || port_ == 0) {
    return false;
  }

  const unsigned long now = millis();
  if (lastAttemptMs_ != 0 && now - lastAttemptMs_ < kReconnectIntervalMs) {
    return false;
  }

  lastAttemptMs_ = now;
  String clientId = buildClientId();

  if (!client_.connect(clientId.c_str())) {
    return false;
  }

  String topic = buildStatusTopic();
  client_.publish(topic.c_str(), "online", true);
  return true;
}

bool MqttManager::publish(const char *topic, const char *payload, bool retained) {
  if (!client_.connected() || topic == nullptr || topic[0] == '\0') {
    return false;
  }

  return client_.publish(topic, payload != nullptr ? payload : "", retained);
}

bool MqttManager::subscribe(const char *topic) {
  if (!client_.connected() || topic == nullptr || topic[0] == '\0') {
    return false;
  }

  return client_.subscribe(topic);
}

void MqttManager::loop() {
  if (client_.connected()) {
    client_.loop();
  }
}

bool MqttManager::connected() { return client_.connected(); }

void MqttManager::disconnect() { client_.disconnect(); }

int MqttManager::state() { return client_.state(); }
