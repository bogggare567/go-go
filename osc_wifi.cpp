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

  // Only OUR stored credentials, ever. The ESP WiFi stack's own persistence
  // is disabled in setup() - after a factory reset there is no ghost network
  // to silently reconnect to.
  if (!wifiSsid[0]) {
    if (allowPortal) {
      startWifiOnboarding();
      setScreen(SCREEN_WEB_SETUP);
    }
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPass);

  if (allowPortal) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    drawCenteredText("WiFi", 4, 2);
    drawCenteredText("connecting...", 26, 1);
    drawCenteredText(wifiSsid, 40, 1);
    display.display();
  }

  unsigned long start = millis();
  // A fresh association (no stack-persisted state) often needs 3-5 s.
  unsigned long budget = allowPortal ? 8000UL : 6000UL;
  while (WiFi.status() != WL_CONNECTED && millis() - start < budget) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    udp.begin(0);
    if (allowPortal) showSimpleMessage("WiFi connected", nullptr, 700);
    return true;
  }

  if (allowPortal) {
    // Could not join: open the WiFi onboarding page (scan + join).
    startWifiOnboarding();
    setScreen(SCREEN_WEB_SETUP);
  }
  return false;
}

bool configureWiFi(bool rebootAfterSave) {
  (void)rebootAfterSave;
  DBGLN("[WIFI] configureWiFi() -> WiFi onboarding");
  startWifiOnboarding();
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
