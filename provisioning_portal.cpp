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

const char kStatusPage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Device Status</title>
  <style>
    body{font-family:Arial,sans-serif;background:#f3f6fb;color:#1f2937;padding:24px}
    .card{max-width:520px;margin:0 auto;background:#fff;border-radius:14px;padding:24px;box-shadow:0 16px 36px rgba(15,23,42,.08)}
    .meta{font-size:14px;color:#475569;margin-top:12px}
    .ok{color:#166534}
    .err{color:#b91c1c}
    button,a{display:inline-block;margin-top:14px;padding:12px 16px;border-radius:10px;text-decoration:none;font-weight:700;border:none;cursor:pointer}
    button{background:#0f766e;color:#fff}
    a{background:#e2e8f0;color:#0f172a}
  </style>
</head>
<body>
  <div class="card">
    <h1 id="title">Applying settings</h1>
    <p id="message">Starting connection sequence...</p>
    <p class="meta" id="networkInfo"></p>
    <p class="meta" id="apInfo"></p>
    <button id="finishButton" type="button" style="display:none" onclick="finishSetup()">Disconnect Hotspot</button>
    <a id="setupLink" href="/setup" style="display:none">Return to Setup</a>
  </div>
  <script>
    async function refreshStatus(){
      try{
        const response=await fetch('/status',{cache:'no-store'});
        const status=await response.json();
        document.getElementById('title').textContent=status.title || 'Device Status';
        document.getElementById('message').textContent=status.message || '';
        document.getElementById('message').className=status.error ? 'err' : (status.done ? 'ok' : '');
        document.getElementById('networkInfo').textContent=status.station_ip ? `Device IP on Wi-Fi: ${status.station_ip}` : '';
        document.getElementById('apInfo').textContent=status.ap_ssid ? `Hotspot: ${status.ap_ssid} (${status.ap_ip})` : '';
        document.getElementById('finishButton').style.display=status.done ? 'inline-block' : 'none';
        document.getElementById('setupLink').style.display=status.error ? 'inline-block' : 'none';
      }catch(error){
        document.getElementById('message').textContent='Lost contact with the device. Reconnect to the hotspot or power-cycle the device.';
        document.getElementById('message').className='err';
      }
    }

    async function finishSetup(){
      const button=document.getElementById('finishButton');
      button.disabled=true;
      try{
        await fetch('/finish',{cache:'no-store'});
      }catch(error){}
      document.getElementById('message').textContent='Hotspot is closing. Reconnect your phone or laptop to your normal Wi-Fi.';
      document.getElementById('message').className='ok';
    }

    refreshStatus();
    setInterval(refreshStatus,1000);
  </script>
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
      finishRequested_(false),
      pageMode_(PageMode::kSetup),
      statusTitle_(),
      statusMessage_(),
      stationIp_(),
      statusDone_(false),
      statusError_(false),
      pendingConfig_() {
  device_config::clear(pendingConfig_);
}

bool ProvisioningPortal::begin(const String &apSsid) {
  stop();

  accessPointSsid_ = apSsid;
  hasPendingConfig_ = false;
  finishRequested_ = false;
  pageMode_ = PageMode::kSetup;
  statusTitle_ = F("Device Setup");
  statusMessage_ = F("Enter Wi-Fi and MQTT details.");
  stationIp_ = "";
  statusDone_ = false;
  statusError_ = false;
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

bool ProvisioningPortal::isActive() const { return active_; }

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

void ProvisioningPortal::showSetupPage() {
  pageMode_ = PageMode::kSetup;
}

void ProvisioningPortal::showStatusPage() {
  pageMode_ = PageMode::kStatus;
}

ProvisioningPortal::PageMode ProvisioningPortal::pageMode() const {
  return pageMode_;
}

void ProvisioningPortal::setConnectionStatus(const String &title,
                                             const String &message, bool done,
                                             bool error,
                                             const String &stationIp) {
  statusTitle_ = title;
  statusMessage_ = message;
  statusDone_ = done;
  statusError_ = error;
  stationIp_ = stationIp;
}

bool ProvisioningPortal::consumeFinishRequest() {
  const bool requested = finishRequested_;
  finishRequested_ = false;
  return requested;
}

void ProvisioningPortal::configureRoutes() {
  server_.on("/", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/scan", HTTP_GET, [this]() { handleScan(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/finish", HTTP_ANY, [this]() { handleFinish(); });
  server_.on("/setup", HTTP_GET, [this]() { handleSetup(); });

  server_.on("/generate_204", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/connecttest.txt", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/ncsi.txt", HTTP_GET, [this]() { handlePortalPage(); });
  server_.on("/fwlink", HTTP_GET, [this]() { handlePortalPage(); });

  server_.onNotFound([this]() { handlePortalPage(); });
}

void ProvisioningPortal::handlePortalPage() {
  if (pageMode_ == PageMode::kStatus) {
    server_.send_P(200, PSTR("text/html"), kStatusPage);
    return;
  }

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
  showStatusPage();
  setConnectionStatus(F("Configuration saved"),
                      F("Keeping the hotspot open while the device connects to Wi-Fi and MQTT."),
                      false, false);

  server_.send_P(200, PSTR("text/html"), kStatusPage);
}

void ProvisioningPortal::handleStatus() {
  String payload = "{\"mode\":\"";
  payload += (pageMode_ == PageMode::kStatus) ? F("status") : F("setup");
  payload += F("\",\"title\":\"");
  payload += jsonEscape(statusTitle_);
  payload += F("\",\"message\":\"");
  payload += jsonEscape(statusMessage_);
  payload += F("\",\"done\":");
  payload += statusDone_ ? F("true") : F("false");
  payload += F(",\"error\":");
  payload += statusError_ ? F("true") : F("false");
  payload += F(",\"station_ip\":\"");
  payload += jsonEscape(stationIp_);
  payload += F("\",\"ap_ssid\":\"");
  payload += jsonEscape(accessPointSsid_);
  payload += F("\",\"ap_ip\":\"");
  payload += WiFi.softAPIP().toString();
  payload += F("\"}");
  server_.send(200, F("application/json"), payload);
}

void ProvisioningPortal::handleFinish() {
  finishRequested_ = true;
  server_.send(200, F("application/json"), F("{\"ok\":true}"));
}

void ProvisioningPortal::handleSetup() {
  showSetupPage();
  setConnectionStatus(F("Device Setup"), F("Enter Wi-Fi and MQTT details."), false,
                      false);
  server_.send_P(200, PSTR("text/html"), kSetupPage);
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
