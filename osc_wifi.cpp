// GO-GO — osc_wifi module.
// v16.17: back to WiFiManager for the join/portal flow (owner: "the logic
// was fine, just restyle it" - dropping it in v16.7 traded a proven state
// machine for a hand-rolled one that turned out less reliable). WiFiManager
// owns its own AP + DNS + webserver while a portal is open; our own panel
// server (web_ui.cpp) only runs once the station link is up or once the
// separate "Web Setup" AP is raised - the two never overlap on port 80.
#include "gogo.h"
#include <WiFiManager.h>
#include "web_font.h"

// Restyle WiFiManager's stock pages to match the panel's look (web_html.h)
// without touching its page structure or logic at all - same brand colors,
// same Unbounded font (reusing the exact asset already embedded for the
// panel), injected once via setCustomHeadElement().
static const char* brandHead() {
  static String css;
  if (!css.length()) {
    css.reserve(2200);
    css += "<style>";
    css += FONT_CSS;
    css +=
      "body{background:#0b0e12!important;color:#f4f2ef!important;"
      "font-family:-apple-system,'Segoe UI',Roboto,sans-serif!important}"
      "h1,h2,h3{font-family:'Unbounded',sans-serif!important;color:#f4f2ef!important;"
      "letter-spacing:.5px;font-weight:600!important}"
      ".wrap{max-width:420px}"
      "button,input[type='button'],input[type='submit']{"
      "background:#5cb2a2!important;color:#0b0e12!important;"
      "font-family:'Unbounded',sans-serif!important;font-weight:500!important;"
      "border-radius:10px!important}"
      "button.D{background:#e0564f!important;color:#fff!important}"
      "input,select{background:#0c1320!important;color:#f4f2ef!important;"
      "border:1px solid rgba(255,255,255,.15)!important;border-radius:8px!important}"
      "a{color:#5cb2a2!important}"
      ".msg{background:#12161c!important;border-color:rgba(255,255,255,.12)!important;"
      "color:#f4f2ef!important;border-radius:10px!important}"
      ".msg.S{border-left-color:#8ec9a8!important}.msg.S h4{color:#8ec9a8!important}"
      ".msg.D{border-left-color:#e0564f!important}.msg.D h4{color:#e0564f!important}"
      ".q{filter:none!important}";
    css += "</style>";
  }
  return css.c_str();
}

static void showApScreen(WiFiManager*) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("WIFI SETUP", 0, 1);
  display.setTextSize(1);
  drawCenteredText("Join WiFi:", 16, 1);
  drawCenteredText(getUniqueName().c_str(), 28, 1);
  drawCenteredText("pass: password123", 40, 1);
  drawCenteredText("192.168.4.1", 56, 1);
  display.display();
}

// Runs the WiFiManager flow (blocking): autoConnect tries the saved network
// first and only falls back to the portal if that fails; startConfigPortal
// (menu-triggered) always opens the portal directly. Either way, the OSC
// target fields are collected on the SAME page as the network - no separate
// trip into the panel needed afterward.
static bool runWiFiManager(bool tryAutoConnectFirst) {
  char portBuf[8];
  snprintf(portBuf, sizeof(portBuf), "%d", config.osc_port);

  WiFiManager wm;
  wm.setCustomHeadElement(brandHead());
  wm.setClass("invert");
  wm.setTitle("GO-GO");
  wm.setAPCallback(showApScreen);
  wm.setConfigPortalTimeout(90);
  // Default is a single ~10s attempt (setConnectRetries default 1) - too
  // impatient for a lot of real venue APs (busy 2.4 GHz, slow DHCP). Give it
  // 3 tries at 15s each before giving up, matching the "often fails to
  // connect" complaint more than a single quick attempt does.
  wm.setConnectRetries(3);
  wm.setConnectTimeout(15);

  WiFiManagerParameter p_ip("osc_ip", "OSC target IP (QLab computer) - optional, can set later in Web Setup", config.osc_ip, sizeof(config.osc_ip));
  WiFiManagerParameter p_port("osc_port", "OSC port - optional", portBuf, sizeof(portBuf));
  WiFiManagerParameter p_addr("osc_addr", "GO OSC address - optional", config.osc_address, sizeof(config.osc_address));
  WiFiManagerParameter p_panic("panic_addr", "PANIC OSC address - optional", config.panic_address, sizeof(config.panic_address));
  wm.addParameter(&p_ip);
  wm.addParameter(&p_port);
  wm.addParameter(&p_addr);
  wm.addParameter(&p_panic);

  // No manual WiFi.begin() pre-seed here: autoConnect() already reconnects
  // using whatever the ESP-IDF has stored from the last successful join
  // (WiFiManager's own wifiConnectNew() briefly flips WiFi.persistent(true)
  // around that save, regardless of our global persistent(false)). A manual
  // WiFi.begin() right before autoConnect() used to race its own internal
  // WiFi.mode(WIFI_OFF) reset (autoConnect() turns STA off and back on
  // before trying anything) - killing the connection attempt mid-handshake
  // was a likely cause of "often fails to connect".
  bool ok = tryAutoConnectFirst
    ? wm.autoConnect(getUniqueName().c_str(), "password123")
    : wm.startConfigPortal(getUniqueName().c_str(), "password123");

  // Mirror whatever WiFiManager ended up with into our own NVS, so the next
  // boot's seed step above has something to try.
  safeCopy(wifiSsid, WiFi.SSID().c_str(), sizeof(wifiSsid));
  safeCopy(wifiPass, WiFi.psk().c_str(), sizeof(wifiPass));
  saveWifiCreds();

  String ipStr = p_ip.getValue();
  if (isValidIP(ipStr.c_str())) {
    int port = atoi(p_port.getValue());
    safeCopy(config.osc_ip, ipStr.c_str(), sizeof(config.osc_ip));
    safeCopy(config.osc_address, p_addr.getValue(), sizeof(config.osc_address));
    safeCopy(config.panic_address, p_panic.getValue(), sizeof(config.panic_address));
    if (port >= 1 && port <= 65535) config.osc_port = port;
    saveOscConfig();
  }

  if (ok) {
    WiFi.setAutoReconnect(true);
    udp.begin(0);
  }
  return ok;
}

// ============================================================================
// WiFi / OSC
// ============================================================================
bool connectWiFiWithDisplay(bool allowPortal) {
  DBGLN("[WIFI] connectWiFiWithDisplay()");
  if (WiFi.status() == WL_CONNECTED) return true;

  if (allowPortal) {
    // Full experience: auto-reconnect first, portal if that fails.
    return runWiFiManager(true);
  }

  // Quiet background attempt: no portal, no long block, no saved network ->
  // don't even try. WiFi.setAutoReconnect(false) matters here: on failure,
  // the ESP-IDF driver otherwise retries a dead AP many times per second
  // forever in the background, which is what was making everything else
  // (button response, OLED redraws) feel laggy.
  if (!wifiSsid[0]) return false;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("WiFi", 4, 2);
  drawCenteredText("connecting...", 26, 1);
  drawCenteredText(wifiSsid, 40, 1);
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  WiFi.disconnect(false, true);  // clear stale association state first
  delay(100);
  WiFi.begin(wifiSsid, wifiPass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 6000) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setAutoReconnect(true);
    udp.begin(0);
    return true;
  }
  WiFi.setAutoReconnect(false);  // still not connected: do not retry-storm
  return false;
}

bool configureWiFi(bool rebootAfterSave) {
  DBGLN("[WIFI] configureWiFi() -> WiFiManager portal");
  bool ok = runWiFiManager(false);  // menu-triggered: always show the portal
  if (ok && rebootAfterSave) ESP.restart();
  return ok;
}

void sendOSC(const char* addr) {
  if (WiFi.status() != WL_CONNECTED) return;
  OSCMessage msg(addr);
  udp.beginPacket(config.osc_ip, config.osc_port);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
}
