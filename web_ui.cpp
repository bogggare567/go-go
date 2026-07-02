// GO-GO — web configuration UI (see web_ui.h).
#include "gogo.h"
#include <WebServer.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "web_html.h"

static WebServer server(80);
static bool serverStarted = false;
static bool apRaised = false;
static bool webPausedBle = false;

// Online update manifest (lives in the GitHub repo, fetched over HTTPS)
static const char* OTA_MANIFEST_URL =
  "https://raw.githubusercontent.com/bogggare567/go-go/main/firmware/version.json";

bool webApActive() { return apRaised; }

String webApPass() { return String("password123"); }

String webUrl() {
  if (apRaised) return "http://192.168.4.1";
  if (WiFi.status() == WL_CONNECTED) return "http://" + WiFi.localIP().toString();
  return "";
}

// ---------------------------------------------------------------------------
// JSON helpers (tiny hand-rolled builder; no external dependencies)
// ---------------------------------------------------------------------------
static String jsonEscape(const char* s) {
  String out;
  for (const char* p = s; *p; p++) {
    if (*p == '"' || *p == '\\') { out += '\\'; }
    out += *p;
  }
  return out;
}

static void handleStatus() {
  bool wifi = WiFi.status() == WL_CONNECTED;
  String peer = lastPeerId ? shortIdString(lastPeerId) : "";
  String j = "{";
  j += "\"name\":\"" + getUniqueName() + "\",";
  j += "\"ver\":\"" GOGO_VERSION "\",";
  j += "\"modeName\":\"" + String(modeLabel(controlMode)) + "\",";
  j += "\"mode\":" + String((int)controlMode) + ",";
  j += "\"out\":" + String((int)gatewayOutputMode) + ",";
  j += "\"osc\":\"" + jsonEscape(config.osc_ip) + ":" + String(config.osc_port) + "\",";
  j += "\"link\":" + String(loraPeerFound() ? "true" : "false") + ",";
  j += "\"region\":\"" + String(currentRegion().name) + "\",";
  j += "\"freq\":" + String(radioCfg.freq, 3) + ",";
  j += "\"auto\":" + String(freqAuto ? "true" : "false") + ",";
  j += "\"peer\":\"" + peer + "\",";
  j += "\"rssi\":" + String(lastPeerRssi) + ",";
  j += "\"snr\":" + String(lastPeerSnr, 1) + ",";
  j += "\"batt\":" + String(batteryPresent ? batteryPercent : -1) + ",";
  j += "\"ble\":" + String(bleConnected ? "true" : "false") + ",";
  j += "\"wifi\":" + String(wifi ? "true" : "false") + ",";
  j += "\"ip\":\"" + (wifi ? WiFi.localIP().toString() : String("")) + "\",";
  j += "\"goKey\":\"" + String(keyName(goKeyCode)) + "\",";
  j += "\"panicKey\":\"" + String(keyName(panicKeyCode)) + "\",";
  j += "\"tx\":" + String(txPacketCount) + ",";
  j += "\"rx\":" + String(rxPacketCount) + ",";
  j += "\"id\":\"" + shortIdString(deviceId) + "\"}";
  server.send(200, "application/json", j);
}

static void handleConfigGet() {
  String j = "{";
  j += "\"mode\":" + String((int)controlMode) + ",";
  j += "\"out\":" + String((int)gatewayOutputMode) + ",";
  j += "\"region\":" + String((int)radioCfg.region) + ",";
  j += "\"freqAuto\":" + String(freqAuto ? "true" : "false") + ",";
  j += "\"chan\":" + String((int)radioCfg.chan) + ",";
  j += "\"oscIp\":\"" + jsonEscape(config.osc_ip) + "\",";
  j += "\"oscPort\":" + String(config.osc_port) + ",";
  j += "\"go\":\"" + jsonEscape(config.osc_address) + "\",";
  j += "\"panic\":\"" + jsonEscape(config.panic_address) + "\",";
  j += "\"goKey\":" + String((int)goKeyCode) + ",";
  j += "\"panicKey\":" + String((int)panicKeyCode) + ",";
  j += "\"ssid\":\"" + jsonEscape(wifiSsid) + "\",";
  j += "\"regions\":[";
  for (uint8_t i = 0; i < regionCount(); i++) {
    if (i) j += ",";
    j += "\"" + String(regionPlan(i).name) + "\"";
  }
  j += "],\"channels\":[";
  for (uint8_t i = 0; i < gridCount(); i++) {
    if (i) j += ",";
    j += String(gridFreq(i), 4);
  }
  j += "],\"keys\":[";
  for (int i = 0; i < keyOptionCount(); i++) {
    if (i) j += ",";
    uint8_t c = keyOptionCode(i);
    j += "{\"c\":" + String((int)c) + ",\"n\":\"" + String(keyName(c)) + "\"}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

static void handleConfigPost() {
  bool reboot = false;

  // Live-applied settings: OSC target + BLE keys.
  if (server.hasArg("oscIp")) safeCopy(config.osc_ip, server.arg("oscIp").c_str(), sizeof(config.osc_ip));
  if (server.hasArg("oscPort")) config.osc_port = server.arg("oscPort").toInt();
  if (server.hasArg("goAddr")) safeCopy(config.osc_address, server.arg("goAddr").c_str(), sizeof(config.osc_address));
  if (server.hasArg("panAddr")) safeCopy(config.panic_address, server.arg("panAddr").c_str(), sizeof(config.panic_address));
  saveOscConfig();

  if (server.hasArg("gokey")) goKeyCode = (uint8_t)server.arg("gokey").toInt();
  if (server.hasArg("pankey")) panicKeyCode = (uint8_t)server.arg("pankey").toInt();
  saveKeymap();

  // WiFi network: sent only when the user actually changed it (see the JS).
  if (server.hasArg("ssid")) {
    safeCopy(wifiSsid, server.arg("ssid").c_str(), sizeof(wifiSsid));
    safeCopy(wifiPass, server.arg("wpass").c_str(), sizeof(wifiPass));
    saveWifiCreds();
    reboot = true;
  }

  // Structural settings: applied via reboot for a clean re-init.
  uint8_t newMode = (uint8_t)server.arg("mode").toInt();
  uint8_t newOut = (uint8_t)server.arg("out").toInt();
  uint8_t newRegion = (uint8_t)server.arg("region").toInt();
  String chanArg = server.arg("chan");
  bool newAuto = (chanArg == "auto");
  uint8_t newChan = newAuto ? 0 : (uint8_t)chanArg.toInt();

  if (newMode != (uint8_t)controlMode || newOut != gatewayOutputMode) {
    saveControlModeAndOutput(newMode, newOut);
    reboot = true;
  }
  if (newRegion != radioCfg.region || newAuto != freqAuto ||
      (!newAuto && newChan != radioCfg.chan)) {
    radioCfg.region = newRegion;
    radioCfg.chan = newChan;
    freqAuto = newAuto;
    regionConfigured = true;
    applyRadioFreq();
    saveRadioConfig();
    reboot = true;
  }

  server.send(200, "application/json", String("{\"reboot\":") + (reboot ? "true" : "false") + "}");
  if (reboot) {
    delay(400);
    ESP.restart();
  }
}

static void handleSpectrumGet() {
  String j = "{\"active\":" + String(currentScreen == SCREEN_SPECTRUM ? "true" : "false");
  j += ",\"from\":" + String(currentRegion().scanFrom, 1);
  j += ",\"to\":" + String(currentRegion().scanTo, 1);
  j += ",\"freq\":" + String(radioCfg.freq, 3);
  j += ",\"levels\":[";
  for (int i = 0; i < 64; i++) {
    if (i) j += ",";
    j += String((int)spectrumLevels[i]);
  }
  j += "]}";
  server.send(200, "application/json", j);
}

static void handleSpectrumPost() {
  // Mirrors the on-device behavior: the OLED shows the sweep too.
  if (server.arg("on") == "1") {
    if (currentScreen != SCREEN_SPECTRUM) {
      enterSpectrum();
      setScreen(SCREEN_SPECTRUM);
    }
  } else {
    if (currentScreen == SCREEN_SPECTRUM) {
      exitSpectrum();
      setScreen(SCREEN_GO);
    }
  }
  server.send(200, "application/json", "{}");
}

static void handleOtaUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}

// ---------------------------------------------------------------------------
// Online update: check a version manifest in the GitHub repo and stream the
// binary straight into the OTA partition. Requires the venue WiFi to have
// internet access; certificate pinning is skipped (setInsecure) on purpose.
// ---------------------------------------------------------------------------
static String jsonField(const String& src, const char* key) {
  String pat = String("\"") + key + "\":\"";
  int a = src.indexOf(pat);
  if (a < 0) return "";
  a += pat.length();
  int b = src.indexOf('"', a);
  if (b < 0) return "";
  return src.substring(a, b);
}

static void handleOtaCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"err\":\"no internet (STA not connected)\"}");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(8000);
  http.begin(client, OTA_MANIFEST_URL);
  int code = http.GET();
  if (code != 200) {
    http.end();
    server.send(200, "application/json", "{\"err\":\"manifest fetch failed\"}");
    return;
  }
  String body = http.getString();
  http.end();
  String latest = jsonField(body, "version");
  String url = jsonField(body, "url");
  String j = "{\"cur\":\"" GOGO_VERSION "\",\"latest\":\"" + latest +
             "\",\"url\":\"" + url + "\"}";
  server.send(200, "application/json", j);
}

static void handleOtaInstall() {
  String url = server.arg("url");
  if (WiFi.status() != WL_CONNECTED || !url.startsWith("https://")) {
    server.send(400, "application/json", "{\"err\":\"bad url or no internet\"}");
    return;
  }
  showSimpleMessage("Updating...", "do not power off", 100);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(30000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, url);
  int code = http.GET();
  int len = http.getSize();
  if (code != 200 || len <= 0 || !Update.begin(len)) {
    http.end();
    server.send(500, "application/json", "{\"err\":\"download failed\"}");
    return;
  }
  size_t written = Update.writeStream(http.getStream());
  http.end();
  bool ok = (written == (size_t)len) && Update.end(true);
  server.send(ok ? 200 : 500, "application/json",
              ok ? "{\"ok\":true}" : "{\"err\":\"flash failed\"}");
  if (ok) {
    showSimpleMessage("Updated!", "rebooting...", 800);
    ESP.restart();
  }
}

static void startServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", WEB_PAGE);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/spectrum", HTTP_GET, handleSpectrumGet);
  server.on("/api/spectrum", HTTP_POST, handleSpectrumPost);
  server.on("/api/go", HTTP_POST, []() { performGO(); server.send(200, "application/json", "{}"); });
  server.on("/api/panic", HTTP_POST, []() { performPanic(); server.send(200, "application/json", "{}"); });
  server.on("/api/otacheck", HTTP_GET, handleOtaCheck);
  server.on("/api/otainstall", HTTP_POST, handleOtaInstall);
  server.on("/api/reboot", HTTP_POST, []() {
    server.send(200, "application/json", "{}");
    delay(300);
    ESP.restart();
  });
  server.on("/update", HTTP_POST, []() {
    bool ok = !Update.hasError();
    server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
    if (ok) { delay(400); ESP.restart(); }
  }, handleOtaUpload);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  serverStarted = true;
  DBGLN("[WEB] server started");
}

void startWebSetup() {
  // In OSC modes the STA link already carries the UI; otherwise raise an AP.
  if (WiFi.status() != WL_CONNECTED) {
    // BLE and WiFi share the single 2.4 GHz radio on the ESP32-S3 and fight
    // for airtime. Setup is a pause anyway: suspend BLE while the AP is up.
    if (bleStarted) {
      stopBLEMode();
      webPausedBle = true;
    }
    wifi_mode_t m = WiFi.getMode();
    WiFi.mode(m == WIFI_OFF ? WIFI_AP : (wifi_mode_t)(m | WIFI_AP));
    WiFi.softAP(getUniqueName().c_str(), webApPass().c_str());
    apRaised = true;
  }
  if (!serverStarted) startServer();
}

void stopWebSetup() {
  if (apRaised) {
    WiFi.softAPdisconnect(true);
    apRaised = false;
    if (WiFi.status() != WL_CONNECTED && controlMode != MODE_OSC_WIFI &&
        !(controlMode == MODE_LORA_GATEWAY && outputWantsOsc())) {
      WiFi.mode(WIFI_OFF);
    }
  }
  if (webPausedBle) {
    webPausedBle = false;
    startBLEMode();
  }
}

void webLoop() {
  // Auto-serve on the venue LAN whenever the station link is up (OSC modes).
  if (!serverStarted && (WiFi.status() == WL_CONNECTED || apRaised)) startServer();
  if (serverStarted) server.handleClient();
}
