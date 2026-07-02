// GO-GO — osc_wifi module.
// v16.7: WiFiManager dropped. Credentials are stored in Preferences and set
// through the web panel; the old captive portal is replaced by the same
// panel served on the device's own access point (see web_ui.cpp).
#include "gogo.h"

// ============================================================================
// WiFi / OSC
// ============================================================================
bool connectWiFiWithDisplay(bool allowPortal) {
  DBGLN("[WIFI] connectWiFiWithDisplay()");
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  if (wifiSsid[0]) {
    WiFi.begin(wifiSsid, wifiPass);
  } else {
    // Fall back to credentials persisted by older firmware (WiFiManager era).
    WiFi.begin();
  }

  if (allowPortal) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    drawCenteredText("WiFi", 4, 2);
    drawCenteredText("connecting...", 26, 1);
    drawCenteredText(wifiSsid[0] ? wifiSsid : "(saved network)", 40, 1);
    display.display();
  }

  unsigned long start = millis();
  unsigned long budget = allowPortal ? 8000UL : 2200UL;
  while (WiFi.status() != WL_CONNECTED && millis() - start < budget) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    udp.begin(0);
    if (allowPortal) showSimpleMessage("WiFi connected", nullptr, 700);
    return true;
  }

  if (allowPortal) {
    // No captive portal anymore: raise our own AP with the web panel and let
    // the user enter the network + OSC settings there.
    startWebSetup();
    setScreen(SCREEN_WEB_SETUP);
  }
  return false;
}

bool configureWiFi(bool rebootAfterSave) {
  (void)rebootAfterSave;
  DBGLN("[WIFI] configureWiFi() -> web panel");
  startWebSetup();
  setScreen(SCREEN_WEB_SETUP);
  return WiFi.status() == WL_CONNECTED;
}

void sendOSC(const char* addr) {
  if (WiFi.status() != WL_CONNECTED) return;
  OSCMessage msg(addr);
  udp.beginPacket(config.osc_ip, config.osc_port);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
}
