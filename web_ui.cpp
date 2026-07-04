// GO-GO — web configuration UI (see web_ui.h).
#include "gogo.h"
#include <WebServer.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "web_html.h"
#include "web_font.h"

static WebServer server(80);
static DNSServer dns;   // captive portal: every AP lookup resolves to us (Web Setup panel AP)
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

  // Structural settings: applied via reboot for a clean re-init. Only when
  // the fields were actually sent - the onboarding page posts OSC-only forms.
  if (server.hasArg("mode") && server.hasArg("out")) {
    uint8_t newMode = (uint8_t)server.arg("mode").toInt();
    uint8_t newOut = (uint8_t)server.arg("out").toInt();
    if (newMode != (uint8_t)controlMode || newOut != gatewayOutputMode) {
      saveControlModeAndOutput(newMode, newOut);
      reboot = true;
    }
  }
  if (server.hasArg("region") && server.hasArg("chan")) {
    uint8_t newRegion = (uint8_t)server.arg("region").toInt();
    String chanArg = server.arg("chan");
    bool newAuto = (chanArg == "auto");
    uint8_t newChan = newAuto ? 0 : (uint8_t)chanArg.toInt();
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
    // A previous aborted session would make begin() fail forever: clear it.
    if (Update.isRunning()) Update.abort();
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
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
  if (!ok && Update.isRunning()) Update.abort();  // unblock future attempts
  server.send(ok ? 200 : 500, "application/json",
              ok ? "{\"ok\":true}" : "{\"err\":\"flash failed\"}");
  if (ok) {
    showSimpleMessage("Updated!", "rebooting...", 800);
    ESP.restart();
  }
}

// ---------------------------------------------------------------------------
// QLab bridge: the browser asks us over HTTP, we ask QLab over OSC and relay
// the raw JSON reply back. All rendering happens in the phone's browser.
// ---------------------------------------------------------------------------
static void handleQlabCmd() {
  String addr = server.arg("addr");
  if (WiFi.status() != WL_CONNECTED || !addr.startsWith("/")) {
    server.send(400, "application/json", "{\"err\":\"bad addr or no wifi\"}");
    return;
  }
  OSCMessage msg(addr.c_str());
  if (server.hasArg("s") && server.arg("s").length()) msg.add(server.arg("s").c_str());
  if (server.hasArg("f")) msg.add(server.arg("f").toFloat());
  udp.beginPacket(config.osc_ip, config.osc_port);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
  server.send(200, "application/json", "{}");
}

static void handleQlabQuery() {
  String addr = server.arg("addr");
  if (WiFi.status() != WL_CONNECTED || !addr.startsWith("/")) {
    server.send(400, "application/json", "{\"err\":\"bad addr or no wifi\"}");
    return;
  }
  // Drop any stale datagrams, then ask.
  while (udp.parsePacket() > 0) { udp.flush(); }
  OSCMessage q(addr.c_str());
  udp.beginPacket(config.osc_ip, config.osc_port);
  q.send(udp);
  udp.endPacket();
  q.empty();

  // LAN OSC round-trip is normally <50ms; a long wait here just ties up the
  // single-threaded WebServer and makes every other panel action feel stuck
  // behind it (this was the real cause of the QLab tab feeling laggy).
  unsigned long start = millis();
  while (millis() - start < 400) {
    int size = udp.parsePacket();
    if (size > 0) {
      OSCMessage m;
      while (size--) m.fill(udp.read());
      char a[96] = {0};
      if (!m.hasError()) m.getAddress(a);
      if (strncmp(a, "/reply", 6) == 0 && m.isString(0)) {
        int len = m.getDataLength(0);
        char* buf = (char*)malloc(len + 2);
        if (buf) {
          m.getString(0, buf, len + 1);
          server.send(200, "application/json", buf);
          free(buf);
          return;
        }
      }
    }
    delay(5);
  }
  // Note: big workspaces can exceed one UDP datagram; TCP OSC is the roadmap fix.
  server.send(200, "application/json", "{\"err\":\"no reply from QLab\"}");
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
  server.on("/font.css", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "max-age=86400");
    server.send_P(200, "text/css", FONT_CSS);
  });
  server.on("/api/qlab/cmd", HTTP_POST, handleQlabCmd);
  server.on("/api/qlab/query", HTTP_GET, handleQlabQuery);
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
  server.onNotFound([]() {
    if (apRaised) {
      // Captive-portal probe (generate_204, hotspot-detect.html, ...):
      // redirect to the panel so the join popup opens it automatically.
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "not found");
    }
  });
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
    // Stop an in-flight join attempt before reconfiguring the interface,
    // and keep the modem awake: the power-save wake path crashes the chip
    // when the station is disconnected (Cache error in ppTask).
    WiFi.disconnect();
    delay(50);
    wifi_mode_t m = WiFi.getMode();
    WiFi.mode(m == WIFI_OFF ? WIFI_AP : (wifi_mode_t)(m | WIFI_AP));
    WiFi.setSleep(false);
    WiFi.softAP(getUniqueName().c_str(), webApPass().c_str());
    apRaised = true;
    // Captive portal: the OS probes a known URL right after joining; the DNS
    // catch-all + the 302 in onNotFound make the panel pop up by itself.
    dns.start(53, "*", WiFi.softAPIP());
  }
  if (!serverStarted) startServer();
}

void stopWebSetup() {
  if (apRaised) {
    dns.stop();
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

  // gogo.local on the venue LAN: no more remembering IP addresses.
  static bool mdnsUp = false;
  if (!mdnsUp && serverStarted && WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin("gogo")) {
      MDNS.addService("http", "tcp", 80);
      mdnsUp = true;
    }
  }
  if (apRaised) dns.processNextRequest();
  if (serverStarted) server.handleClient();
}
