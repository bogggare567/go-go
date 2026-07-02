// GO-GO — osc_wifi module (split 1:1 from v15 monolith).
#include "gogo.h"

// ============================================================================
// WiFi / OSC
// ============================================================================
bool configureWiFi(bool rebootAfterSave) {
  DBGLN("[WIFI] configureWiFi()");

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("WiFi Setup", 6, 2);
  display.setTextSize(1);
  drawCenteredText("Connect to AP", 28, 1);
  drawCenteredText(getUniqueName().c_str(), 40, 1);
  drawCenteredText("pass: password123", 52, 1);
  display.display();

  WiFi.mode(WIFI_STA);

  char portBuf[8];
  snprintf(portBuf, sizeof(portBuf), "%d", config.osc_port);

  WiFiManager wm;
  WiFiManagerParameter p_ip("osc_ip", "OSC IP", config.osc_ip, sizeof(config.osc_ip));
  WiFiManagerParameter p_port("osc_port", "OSC Port", portBuf, sizeof(portBuf));
  WiFiManagerParameter p_addr("osc_addr", "GO OSC Addr", config.osc_address, sizeof(config.osc_address));
  WiFiManagerParameter p_panic("panic_addr", "Panic OSC Addr", config.panic_address, sizeof(config.panic_address));

  wm.addParameter(&p_ip);
  wm.addParameter(&p_port);
  wm.addParameter(&p_addr);
  wm.addParameter(&p_panic);
  wm.setConfigPortalTimeout(90);

  bool ok = wm.startConfigPortal(getUniqueName().c_str(), "password123");
  DBG("[WIFI] portal result: "); DBGLN(ok ? "OK" : "FAIL/TIMEOUT");
  if (!ok) {
    showSimpleMessage("Setup cancelled", nullptr, 1200);
    return false;
  }

  String ipStr = p_ip.getValue();
  if (!isValidIP(ipStr.c_str())) {
    showSimpleMessage("Invalid IP!", nullptr, 1600);
    return false;
  }

  int port = atoi(p_port.getValue());
  if (port < 1 || port > 65535) {
    showSimpleMessage("Invalid port!", nullptr, 1600);
    return false;
  }

  safeCopy(config.osc_ip, ipStr.c_str(), sizeof(config.osc_ip));
  safeCopy(config.osc_address, p_addr.getValue(), sizeof(config.osc_address));
  safeCopy(config.panic_address, p_panic.getValue(), sizeof(config.panic_address));
  config.osc_port = port;
  saveOscConfig();

  showSimpleMessage("Saved", rebootAfterSave ? "Rebooting..." : "WiFi ready", 900);
  if (rebootAfterSave) ESP.restart();
  return (WiFi.status() == WL_CONNECTED);
}

bool connectWiFiWithDisplay(bool allowPortal) {
  DBGLN("[WIFI] connectWiFiWithDisplay()");
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);

  if (!allowPortal) {
    WiFi.begin();
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 2200) {
      delay(100);
    }
    return WiFi.status() == WL_CONNECTED;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("WiFi", 4, 2);
  drawCenteredText("connect/setup", 20, 1);
  drawCenteredText(getUniqueName().c_str(), 34, 1);
  drawCenteredText("pass: password123", 47, 1);
  drawCenteredText("hold later: menu", 56, 1);
  display.display();

  char portBuf[8];
  snprintf(portBuf, sizeof(portBuf), "%d", config.osc_port);

  WiFiManager wm;
  WiFiManagerParameter p_ip("osc_ip", "OSC IP", config.osc_ip, sizeof(config.osc_ip));
  WiFiManagerParameter p_port("osc_port", "OSC Port", portBuf, sizeof(portBuf));
  WiFiManagerParameter p_addr("osc_addr", "GO OSC Addr", config.osc_address, sizeof(config.osc_address));
  WiFiManagerParameter p_panic("panic_addr", "Panic OSC Addr", config.panic_address, sizeof(config.panic_address));

  wm.addParameter(&p_ip);
  wm.addParameter(&p_port);
  wm.addParameter(&p_addr);
  wm.addParameter(&p_panic);
  wm.setConfigPortalTimeout(45);

  bool ok = wm.autoConnect(getUniqueName().c_str(), "password123");
  DBG("[WIFI] autoConnect: "); DBGLN(ok ? "OK" : "FAIL/TIMEOUT");
  if (!ok) return false;

  String ipStr = p_ip.getValue();
  int port = atoi(p_port.getValue());
  if (isValidIP(ipStr.c_str()) && port >= 1 && port <= 65535) {
    safeCopy(config.osc_ip, ipStr.c_str(), sizeof(config.osc_ip));
    safeCopy(config.osc_address, p_addr.getValue(), sizeof(config.osc_address));
    safeCopy(config.panic_address, p_panic.getValue(), sizeof(config.panic_address));
    config.osc_port = port;
    saveOscConfig();
  }

  udp.begin(0);
  showSimpleMessage("WiFi connected", nullptr, 700);
  return true;
}

void sendOSC(const char* addr) {
  if (WiFi.status() != WL_CONNECTED) return;
  OSCMessage msg(addr);
  udp.beginPacket(config.osc_ip, config.osc_port);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
}
