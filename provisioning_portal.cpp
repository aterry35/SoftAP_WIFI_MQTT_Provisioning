#include "provisioning_portal.h"

namespace {

constexpr byte kDnsPort = 53;

const char kSetupPage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP8266 Setup</title>
  <style>
    body{margin:0;font-family:Arial,sans-serif;background:#f3f6fb;color:#1f2937}
    .wrap{max-width:560px;margin:0 auto;padding:24px}
    .card{background:#fff;border-radius:14px;padding:24px;box-shadow:0 16px 36px rgba(15,23,42,.08)}
    h1{margin:0 0 8px;font-size:28px}
    p{line-height:1.5}
    label{display:block;margin:16px 0 6px;font-weight:700}
    input,select,button{width:100%;box-sizing:border-box;border-radius:10px;border:1px solid #cbd5e1;padding:12px;font-size:16px}
    button{background:#0f766e;color:#fff;border:none;font-weight:700;cursor:pointer}
    .secondary{margin-top:10px;background:#e2e8f0;color:#0f172a}
    .hint{margin-top:12px;font-size:14px;color:#475569}
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>Device Setup</h1>
      <p>Connect the device to your Wi-Fi and MQTT broker.</p>
      <form method="POST" action="/save">
        <label for="ssid">Nearby Wi-Fi Networks</label>
        <select id="ssid" name="ssid"></select>
        <button class="secondary" type="button" onclick="loadNetworks()">Scan Wi-Fi</button>

        <label for="manual_ssid">Manual SSID</label>
        <input id="manual_ssid" name="manual_ssid" maxlength="32" placeholder="Use this for hidden networks">

        <label for="password">Wi-Fi Password</label>
        <input id="password" name="password" type="password" maxlength="64" placeholder="Your Wi-Fi password">

        <label for="mqtt_broker">MQTT Broker Address</label>
        <input id="mqtt_broker" name="mqtt_broker" maxlength="64" placeholder="broker.local or 192.168.1.50">

        <label for="mqtt_port">MQTT Port</label>
        <input id="mqtt_port" name="mqtt_port" type="number" min="1" max="65535" value="1883">

        <button type="submit">Save and Connect</button>
      </form>
      <p class="hint" id="scanStatus">Tap "Scan Wi-Fi" to list nearby networks. If the page is unstable on your phone, use manual SSID entry.</p>
    </div>
  </div>
  <script>
    async function loadNetworks(){
      const select=document.getElementById('ssid');
      const status=document.getElementById('scanStatus');
      status.textContent='Scanning nearby networks...';
      select.innerHTML='';
      try{
        const response=await fetch('/scan',{cache:'no-store'});
        const networks=await response.json();
        if(!Array.isArray(networks) || networks.length===0){
          const option=document.createElement('option');
          option.value='';
          option.textContent='No networks found';
          select.appendChild(option);
          status.textContent='No networks found. You can still enter a hidden SSID manually.';
          return;
        }
        networks.forEach((network)=>{
          const option=document.createElement('option');
          option.value=network.ssid;
          option.textContent=`${network.ssid} (${network.rssi} dBm${network.secured ? ', secured' : ', open'})`;
          select.appendChild(option);
        });
        status.textContent='Select your Wi-Fi network or enter one manually.';
      }catch(error){
        status.textContent='Scan failed. Try again, or enter the SSID manually.';
      }
    }
  </script>
</body>
</html>
)HTML";

const char kSavedPage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Saved</title>
  <style>
    body{font-family:Arial,sans-serif;background:#f3f6fb;color:#1f2937;padding:24px}
    .card{max-width:520px;margin:0 auto;background:#fff;border-radius:14px;padding:24px;box-shadow:0 16px 36px rgba(15,23,42,.08)}
  </style>
</head>
<body>
  <div class="card">
    <h1>Settings saved</h1>
    <p>The device is leaving setup mode and will try to connect to your Wi-Fi and MQTT broker now.</p>
    <p>You can close this page.</p>
  </div>
</body>
</html>
)HTML";

const char kErrorPagePrefix[] PROGMEM =
    "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Setup Error</title></head><body style=\"font-family:Arial,sans-serif;"
    "padding:24px;background:#f3f6fb;color:#1f2937\"><h1>Setup Error</h1><p>";
const char kErrorPageSuffix[] PROGMEM = "</p><p><a href=\"/\">Back to setup</a></p></body></html>";

}  // namespace

ProvisioningPortal::ProvisioningPortal()
    : dnsServer_(),
      server_(80),
      apIp_(192, 168, 4, 1),
      accessPointSsid_(),
      active_(false),
      hasPendingConfig_(false),
      pendingConfig_() {
  device_config::clear(pendingConfig_);
}

bool ProvisioningPortal::begin(const String &apSsid) {
  stop();

  accessPointSsid_ = apSsid;
  hasPendingConfig_ = false;
  device_config::clear(pendingConfig_);

  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIp_, apIp_, IPAddress(255, 255, 255, 0));

  if (!WiFi.softAP(accessPointSsid_.c_str())) {
    return false;
  }

  dnsServer_.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer_.start(kDnsPort, "*", apIp_);

  configureRoutes();
  server_.begin();
  active_ = true;
  return true;
}

void ProvisioningPortal::stop() {
  if (!active_) {
    return;
  }

  server_.stop();
  dnsServer_.stop();
  WiFi.softAPdisconnect(true);
  active_ = false;
}

void ProvisioningPortal::loop() {
  if (!active_) {
    return;
  }

  dnsServer_.processNextRequest();
  server_.handleClient();
}

bool ProvisioningPortal::hasSubmittedConfig() const { return hasPendingConfig_; }

device_config::DeviceConfig ProvisioningPortal::takeSubmittedConfig() {
  hasPendingConfig_ = false;
  return pendingConfig_;
}

String ProvisioningPortal::accessPointSsid() const { return accessPointSsid_; }

void ProvisioningPortal::configureRoutes() {
  server_.on("/", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/scan", HTTP_GET, [this]() { handleScan(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/status", HTTP_GET, [this]() { handleStatus(); });

  server_.on("/generate_204", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/connecttest.txt", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/ncsi.txt", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/fwlink", HTTP_GET, [this]() { handlePortalPage(); });

  server_.onNotFound([this]() { handlePortalPage(); });
}

void ProvisioningPortal::handlePortalPage() {
  server_.send_P(200, PSTR("text/html"), kSetupPage);
}

void ProvisioningPortal::handleScan() {
  const int count = WiFi.scanNetworks(false, true);

  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, F("application/json"), "");
  server_.sendContent("[");

  if (count > 0) {
    for (int index = 0; index < count; ++index) {
      if (index > 0) {
        server_.sendContent(",");
      }

      String entry = "{\"ssid\":\"";
      entry += jsonEscape(WiFi.SSID(index));
      entry += "\",\"rssi\":";
      entry += String(WiFi.RSSI(index));
      entry += ",\"secured\":";
      entry += (WiFi.encryptionType(index) == ENC_TYPE_NONE) ? F("false") : F("true");
      entry += "}";
      server_.sendContent(entry);
    }
  }

  server_.sendContent("]");
  WiFi.scanDelete();
}

void ProvisioningPortal::handleSave() {
  String ssid = server_.arg(F("manual_ssid"));
  ssid.trim();
  if (ssid.isEmpty()) {
    ssid = server_.arg(F("ssid"));
    ssid.trim();
  }

  String password = server_.arg(F("password"));
  String broker = server_.arg(F("mqtt_broker"));
  broker.trim();
  String portValue = server_.arg(F("mqtt_port"));
  portValue.trim();

  const unsigned long mqttPort = portValue.toInt();
  if (ssid.isEmpty()) {
    String page = FPSTR(kErrorPagePrefix);
    page += F("Wi-Fi SSID is required.");
    page += FPSTR(kErrorPageSuffix);
    server_.send(400, F("text/html"), page);
    return;
  }

  if (broker.isEmpty()) {
    String page = FPSTR(kErrorPagePrefix);
    page += F("MQTT broker address is required.");
    page += FPSTR(kErrorPageSuffix);
    server_.send(400, F("text/html"), page);
    return;
  }

  if (mqttPort == 0 || mqttPort > 65535UL) {
    String page = FPSTR(kErrorPagePrefix);
    page += F("MQTT port must be between 1 and 65535.");
    page += FPSTR(kErrorPageSuffix);
    server_.send(400, F("text/html"), page);
    return;
  }

  device_config::clear(pendingConfig_);
  copyString(pendingConfig_.wifiSsid, sizeof(pendingConfig_.wifiSsid), ssid);
  copyString(pendingConfig_.wifiPassword, sizeof(pendingConfig_.wifiPassword), password);
  copyString(pendingConfig_.mqttBroker, sizeof(pendingConfig_.mqttBroker), broker);
  pendingConfig_.mqttPort = static_cast<uint16_t>(mqttPort);
  pendingConfig_.provisioned = true;
  hasPendingConfig_ = true;

  server_.send_P(200, PSTR("text/html"), kSavedPage);
}

void ProvisioningPortal::handleStatus() {
  String payload = "{\"mode\":\"provisioning\",\"ap_ssid\":\"";
  payload += jsonEscape(accessPointSsid_);
  payload += "\",\"ap_ip\":\"";
  payload += WiFi.softAPIP().toString();
  payload += "\"}";
  server_.send(200, F("application/json"), payload);
}

String ProvisioningPortal::jsonEscape(const String &input) const {
  String output;
  output.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); ++i) {
    const char ch = input.charAt(i);
    switch (ch) {
      case '\"':
        output += F("\\\"");
        break;
      case '\\':
        output += F("\\\\");
        break;
      case '\n':
        output += F("\\n");
        break;
      case '\r':
        output += F("\\r");
        break;
      case '\t':
        output += F("\\t");
        break;
      default:
        output += ch;
        break;
    }
  }

  return output;
}

void ProvisioningPortal::copyString(char *destination, size_t destinationSize,
                                    const String &value) const {
  if (destinationSize == 0) {
    return;
  }

  strncpy(destination, value.c_str(), destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}
