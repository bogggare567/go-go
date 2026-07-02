// GO-GO — ui_screens module (split 1:1 from v15 monolith).
#include "gogo.h"

void setScreen(uint8_t s) {
  currentScreen = (Screen)s;
  screenChangedAt = millis();
}

const uint8_t MENU_OSC_ITEMS[] = {
  MENU_MODE, MENU_SETUP, MENU_STATUS, MENU_RESET, MENU_REBOOT, MENU_BACK
};
const uint8_t MENU_BLE_ITEMS[] = {
  MENU_MODE, MENU_SETUP, MENU_STATUS, MENU_RESET, MENU_REBOOT, MENU_BACK
};
const uint8_t MENU_LORA_REMOTE_ITEMS[] = {
  MENU_PAIR, MENU_MODE, MENU_SETUP, MENU_STATUS, MENU_LORA_FREQ, MENU_LORA_POWER, MENU_RESET, MENU_REBOOT, MENU_BACK
};
const uint8_t MENU_LORA_GATEWAY_ITEMS[] = {
  MENU_MODE, MENU_SETUP, MENU_STATUS, MENU_LORA_FREQ, MENU_LORA_POWER, MENU_OUTPUT, MENU_RESET, MENU_REBOOT, MENU_BACK
};

// ============================================================================
// Boot animation
// ============================================================================
enum BootPhase : uint8_t {
  BOOT_DOT,
  BOOT_LINE_GROW,
  BOOT_TEXT_GROW,
  BOOT_PEAK,
  BOOT_TEXT_SHRINK,
  BOOT_LINE_SHRINK,
  BOOT_OFF,
  BOOT_DONE
};

BootPhase bootPhase = BOOT_DOT;
unsigned long bootTimer = 0;
int bootStep = 0;
int textX = 0;
int textY = 0;

const char* modeLabel(uint8_t mode) {
  switch (mode) {
    case MODE_OSC_WIFI:     return "OSC / WiFi";
    case MODE_BLE_HID:      return "BLE / HID";
    case MODE_LORA_REMOTE:  return "LoRa TX";
    case MODE_LORA_GATEWAY: return "LoRa RX";
    default:                return "Unknown";
  }
}

const char* shortModeLabel(uint8_t mode) {
  switch (mode) {
    case MODE_OSC_WIFI:     return "OSC";
    case MODE_BLE_HID:      return "BLE";
    case MODE_LORA_REMOTE:  return "TX";
    case MODE_LORA_GATEWAY: return "RX";
    default:                return "?";
  }
}

const char* modeHint(uint8_t mode) {
  switch (mode) {
    case MODE_OSC_WIFI:     return "direct OSC by WiFi";
    case MODE_BLE_HID:      return "keyboard SPACE/ESC";
    case MODE_LORA_REMOTE:  return "send to LoRa RX";
    case MODE_LORA_GATEWAY: return "receiver: choose out";
    default:                return "";
  }
}

uint8_t nextMode(uint8_t mode) {
  uint8_t v = (uint8_t)mode;
  v = (v + 1) % 4;
  return (ControlMode)v;
}

const char* outputLabel(uint8_t m) {
  switch (m) {
    case OUT_BLE:    return "BLE";
    case OUT_OSC:    return "OSC";
    default:         return "?";
  }
}

const char* outputFullLabel(uint8_t m) {
  switch (m) {
    case OUT_BLE:    return "BLE HID";
    case OUT_OSC:    return "OSC WiFi";
    default:         return "?";
  }
}

const char* outputHint(uint8_t m) {
  switch (m) {
    case OUT_BLE:    return "RX sends SPACE/ESC";
    case OUT_OSC:    return "RX sends OSC";
    default:         return "";
  }
}

uint8_t nextOutput(uint8_t out) {
  return (uint8_t)((out + 1) % 2);
}

void showSimpleMessage(const char* line1, const char* line2, unsigned long waitMs) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, line2 ? 24 : 30);
  display.print(line1);

  if (line2) {
    display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 38);
    display.print(line2);
  }

  display.display();
  delay(waitMs);
}

void drawCenteredText(const char* text, int y, int size, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.setTextColor(color);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

// ============================================================================
// Drawing helpers
// ============================================================================
void drawPowerIcon(int x, int y) {
  if (batteryPresent) {
    display.drawRect(x, y, 10, 5, SSD1306_WHITE);
    display.fillRect(x + 10, y + 1, 2, 3, SSD1306_WHITE);
    int fillW = map(batteryPercent, 0, 100, 0, 8);
    if (fillW > 0) display.fillRect(x + 1, y + 1, fillW, 3, SSD1306_WHITE);
  } else {
    display.drawLine(x + 3, y, x + 3, y + 6, SSD1306_WHITE);
    display.drawLine(x + 1, y, x + 5, y, SSD1306_WHITE);
    display.drawLine(x + 1, y + 1, x + 5, y + 1, SSD1306_WHITE);
  }
}

void drawWiFiIndicator(int x, int y) {
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    int bars;
    if (rssi >= -40) bars = 4;
    else if (rssi >= -55) bars = 3;
    else if (rssi >= -70) bars = 2;
    else if (rssi >= -85) bars = 1;
    else bars = 0;

    for (int i = 0; i < 4; i++) {
      int h = 2 + i * 2;
      int bx = x + i * 4;
      int by = y + 8 - h;
      if (i < bars) display.fillRect(bx, by, 3, h, SSD1306_WHITE);
      else display.drawRect(bx, by, 3, h, SSD1306_WHITE);
    }
  } else {
    display.drawCircle(x + 6, y + 4, 5, SSD1306_WHITE);
    display.drawLine(x + 2, y + 1, x + 10, y + 9, SSD1306_WHITE);
  }
}

void drawBLEIndicator(int x, int y) {
  if (bleConnected) {
    display.drawLine(x + 3, y,     x + 3, y + 8, SSD1306_WHITE);
    display.drawLine(x + 3, y,     x + 6, y + 2, SSD1306_WHITE);
    display.drawLine(x + 6, y + 2, x,     y + 6, SSD1306_WHITE);
    display.drawLine(x,     y + 2, x + 6, y + 6, SSD1306_WHITE);
    display.drawLine(x + 6, y + 6, x + 3, y + 8, SSD1306_WHITE);
  } else {
    display.drawLine(x + 3, y,     x + 3, y + 8, SSD1306_WHITE);
    display.drawLine(x,     y,     x + 6, y + 8, SSD1306_WHITE);
    display.drawLine(x + 6, y,     x,     y + 8, SSD1306_WHITE);
  }
}

void drawLoRaIndicator(int x, int y) {
  bool ok = loraInitialized && (isLoRaGateway() || linkOK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y + 1);
  display.print(isLoRaRemote() ? "TX" : "RX");

  // Tiny status mark: filled dot = link/radio OK, X = not ready.
  if (ok) {
    display.fillCircle(x + 17, y + 5, 2, SSD1306_WHITE);
  } else {
    display.drawLine(x + 14, y + 1, x + 20, y + 7, SSD1306_WHITE);
    display.drawLine(x + 20, y + 1, x + 14, y + 7, SSD1306_WHITE);
  }
}

void drawUSBIndicator(int x, int y) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y + 1);
  display.print("USB");
}

void drawConnectionIndicator(int x, int y) {
  if (controlMode == MODE_OSC_WIFI) drawWiFiIndicator(x, y);
  else if (controlMode == MODE_BLE_HID) drawBLEIndicator(x + 4, y);
  else drawLoRaIndicator(x - 2, y);
}

// ============================================================================
// Screens
// ============================================================================
void drawModeSelect() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCenteredText(modeConfigured ? "CHANGE MODE" : "SELECT MODE", 0, 1);

  display.fillRect(0, 13, SCREEN_WIDTH, 18, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  drawCenteredText(modeLabel(selectedMode), 17, 1, SSD1306_BLACK);

  display.setTextColor(SSD1306_WHITE);
  drawCenteredText(modeHint(selectedMode), 34, 1);
  if (selectedMode == MODE_LORA_GATEWAY) {
    drawCenteredText("next: RX output", 45, 1);
  } else {
    drawCenteredText("clean GO screen", 45, 1);
  }
  drawCenteredText("click next hold ok", 56, 1);

  display.display();
}


void drawOutputSelect() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCenteredText("RX OUTPUT", 0, 1);
  drawCenteredText("Choose ONE output", 10, 1);

  display.fillRect(0, 23, SCREEN_WIDTH, 17, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  drawCenteredText(outputFullLabel(selectedOutputMode), 27, 1, SSD1306_BLACK);

  display.setTextColor(SSD1306_WHITE);
  drawCenteredText(outputHint(selectedOutputMode), 43, 1);
  drawCenteredText("click next hold ok", 56, 1);

  display.display();
}


void drawPairSelect() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("PAIR RX", 0, 2);
  display.setTextSize(1);

  int count = discoveredPeerCount();
  if (count == 0) {
    drawCenteredText("Scanning RX...", 20, 1);
    drawCenteredText("LED blinks", 32, 1);
    String cur = String("Current: ") + peerTargetString();
    drawCenteredText(cur.c_str(), 43, 1);
    drawCenteredText("hold: ANY", 54, 1);
  } else {
    if (selectedPeerIndex >= count) selectedPeerIndex = 0;
    int idx = discoveredIndexByVisibleOrder(selectedPeerIndex);
    String title = String(selectedPeerIndex + 1) + String("/") + String(count) + String("  hold pair");
    drawCenteredText(title.c_str(), 17, 1);
    if (idx >= 0) {
      String id = String("RX ") + shortIdString(discoveredPeers[idx].id);
      drawCenteredText(id.c_str(), 29, 1);
      String rssi = String("RSSI ") + String(discoveredPeers[idx].rssi) + String(" dBm");
      drawCenteredText(rssi.c_str(), 41, 1);
    }
    drawCenteredText("click next", 54, 1);
  }
  display.display();
}

void drawGoScreen() {
  display.clearDisplay();
  drawPowerIcon(2, 2);
  drawConnectionIndicator(106, 0);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  String top;
  if (controlMode == MODE_OSC_WIFI) {
    top = WiFi.status() == WL_CONNECTED ? "OSC OK" : "OSC WAIT";
  } else if (controlMode == MODE_BLE_HID) {
    top = bleConnected ? "BLE OK" : "BLE WAIT";
  } else if (controlMode == MODE_LORA_REMOTE) {
    top = loraPeerFound() ? String("TX OK ") + shortIdString(lastPeerId) : "TX SEARCH";
  } else if (controlMode == MODE_LORA_GATEWAY) {
    if (!loraPeerFound()) {
      top = "RX LINK WAIT";
    } else if (outputWantsBle()) {
      top = bleConnected ? "RX BLE OK" : "RX BLE WAIT";
    } else if (outputWantsOsc()) {
      top = (WiFi.status() == WL_CONNECTED) ? "RX OSC OK" : "RX OSC WAIT";
    }
  }
  drawCenteredText(top.c_str(), 2, 1);

  int btnX = 34, btnY = 14, btnW = 60, btnH = 32;
  display.fillRect(btnX, btnY, btnW, btnH, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(3);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds("GO", 0, 0, &x1, &y1, &w, &h);
  int cx = btnX + (btnW - w) / 2;
  int cy = btnY + (btnH - h) / 2;
  display.setCursor(cx, cy);
  display.print("GO");

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  const char* bottom = "soundkorb.ru";

  display.getTextBounds(bottom, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 54);
  display.print(bottom);

  display.display();
}

void drawGoSent() {
  display.clearDisplay();
  drawPowerIcon(2, 2);
  drawConnectionIndicator(106, 0);

  int btnX = 34, btnY = 12, btnW = 60, btnH = 28;
  display.drawRect(btnX, btnY, btnW, btnH, SSD1306_WHITE);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds("GO", 0, 0, &x1, &y1, &w, &h);
  display.setCursor(btnX + (btnW - w) / 2, btnY + (btnH - h) / 2);
  display.print("GO");

  display.setTextSize(1);
  const char* line = "SENT";
  if (controlMode == MODE_OSC_WIFI) line = "SENT OSC";
  else if (controlMode == MODE_BLE_HID) line = "SENT BLE";
  else if (isLoRaRemote()) line = ackOk ? "SENT LORA" : "LORA TX FAIL";
  else if (isLoRaGateway()) line = "RX OUT";
  drawCenteredText(line, 50, 1);

  display.display();
}

void drawPanicSent() {
  display.clearDisplay();
  drawPowerIcon(2, 2);
  drawConnectionIndicator(106, 0);

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds("PANIC", 0, 0, &x1, &y1, &w, &h);
  int px = (SCREEN_WIDTH - w) / 2;
  int py = 12;
  display.setCursor(px, py);
  display.print("PANIC");

  display.setTextSize(1);
  const char* line = "SENT";
  if (controlMode == MODE_OSC_WIFI) line = "SENT OSC";
  else if (controlMode == MODE_BLE_HID) line = "SENT BLE";
  else if (isLoRaRemote()) line = ackOk ? "SENT LORA" : "LORA TX FAIL";
  else if (isLoRaGateway()) line = "RX OUT";
  drawCenteredText(line, 48, 1);

  display.display();
}

void drawLoraSearch() {
  display.clearDisplay();
  drawPowerIcon(2, 2);
  drawLoRaIndicator(106, 0);
  drawCenteredText(isLoRaRemote() ? "SEARCH RX" : "WAIT TX", 10, 2);
  display.setTextSize(1);
  if (isLoRaRemote()) {
    drawCenteredText("waiting for RX", 32, 1);
    drawCenteredText("Pair if needed", 45, 1);
  } else {
    drawCenteredText("waiting for TX", 32, 1);
    drawCenteredText(outputWantsBle() ? "Out: BLE" : "Out: OSC", 45, 1);
  }
  drawCenteredText("hold: menu", 55, 1);
  display.display();
}

void drawLoraLinkOK() {
  display.clearDisplay();
  drawPowerIcon(2, 2);
  drawLoRaIndicator(106, 0);
  drawCenteredText("LINK OK", 10, 2);
  display.setTextSize(1);
  String peer = String(isLoRaRemote() ? "RX " : "TX ") + shortIdString(lastPeerId);
  drawCenteredText(peer.c_str(), 32, 1);
  String rssi = String("RSSI ") + String(lastPeerRssi) + String(" dBm");
  drawCenteredText(rssi.c_str(), 45, 1);
  drawCenteredText("GO / 3x MENU", 55, 1);
  display.display();
}

void drawLoraLinkLost() {
  display.clearDisplay();
  drawPowerIcon(2, 2);
  drawLoRaIndicator(106, 0);
  drawCenteredText("LINK LOST", 10, 2);
  display.setTextSize(1);
  drawCenteredText(isLoRaRemote() ? "RX disconnected" : "TX disconnected", 32, 1);
  drawCenteredText("click: retry", 45, 1);
  drawCenteredText("hold: menu", 55, 1);
  display.display();
}

void drawNoConnection() {
  display.clearDisplay();
  drawPowerIcon(2, 2);
  drawConnectionIndicator(106, 0);

  if (controlMode == MODE_OSC_WIFI) {
    drawCenteredText("NO WIFI", 14, 2);
    drawCenteredText("click: retry setup", 40, 1);
    drawCenteredText("hold: menu", 52, 1);
  } else if (controlMode == MODE_BLE_HID) {
    drawCenteredText("NO BLE", 14, 2);
    drawCenteredText(getUniqueName().c_str(), 36, 1);
    drawCenteredText("hold: menu", 52, 1);
  } else if (isLoRaRemote()) {
    drawCenteredText("NO LINK", 14, 2);
    drawCenteredText("RX not seen", 34, 1);
    drawCenteredText("menu: Pair RX", 44, 1);
    drawCenteredText("click ping hold menu", 54, 1);
  } else if (isLoRaGateway()) {
    if (loraInitFailed) {
      drawCenteredText("NO LORA", 14, 2);
      drawCenteredText("check radio", 38, 1);
    } else if (outputWantsOsc() && WiFi.status() != WL_CONNECTED) {
      drawCenteredText("NO WIFI", 14, 2);
      drawCenteredText("OSC output off", 38, 1);
    } else if (outputWantsBle() && !bleConnected) {
      drawCenteredText("NO BLE", 14, 2);
      drawCenteredText(getUniqueName().c_str(), 38, 1);
    } else {
      drawCenteredText("NO OUT", 14, 2);
      drawCenteredText("check output", 38, 1);
    }
    drawCenteredText("hold: menu", 52, 1);
  } else {
    drawCenteredText("NO LINK", 14, 2);
    drawCenteredText("check mode", 38, 1);
    drawCenteredText("hold: menu", 52, 1);
  }

  display.display();
}

void drawStatusScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCenteredText("STATUS", 0, 2);
  display.setTextSize(1);
  int y = 16;

  display.setCursor(0, y); display.print("Mode: "); display.print(shortModeLabel(controlMode)); y += 8;

  if (controlMode == MODE_OSC_WIFI) {
    display.setCursor(0, y); display.print("WiFi: "); display.print(WiFi.status() == WL_CONNECTED ? "OK" : "OFF"); y += 8;
    display.setCursor(0, y); display.print("RSSI: ");
    if (WiFi.status() == WL_CONNECTED) { display.print(WiFi.RSSI()); display.print("dBm"); }
    else display.print("N/A");
    y += 8;
    display.setCursor(0, y); display.print("IP: "); display.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "N/A"); y += 8;
    display.setCursor(0, y); display.print("OSC: "); display.print(config.osc_ip); display.print(":"); display.print(config.osc_port);
  } else if (controlMode == MODE_BLE_HID) {
    display.setCursor(0, y); display.print("BLE: "); display.print(bleConnected ? "Connected" : "Waiting"); y += 8;
    display.setCursor(0, y); display.print("Device: "); display.print(getUniqueName()); y += 8;
    display.setCursor(0, y); display.print("GO: SPACE"); y += 8;
    display.setCursor(0, y); display.print("PANIC: ESC");
  } else if (isLoRaRemote()) {
    display.setCursor(0, y); display.print("Radio: "); display.print(loraInitialized ? "OK" : "FAIL"); y += 8;
    display.setCursor(0, y); display.print("Pair: "); display.print(peerTargetString()); y += 8;
    display.setCursor(0, y); display.print("RX: "); display.print(loraPeerFound() ? shortIdString(lastPeerId) : "WAIT"); y += 8;
    display.setCursor(0, y); display.print("RSSI: "); if (lastPeerId) display.print(lastPeerRssi); else display.print("--"); y += 8;
    display.setCursor(0, y); display.print("CH: "); display.print(radioCfg.freq, 1); display.print("MHz"); y += 8;
    display.setCursor(0, y); display.print("ID: "); display.print(shortIdString(deviceId));
  } else if (isLoRaGateway()) {
    display.setCursor(0, y); display.print("Radio: "); display.print(loraInitialized ? "OK" : "FAIL"); y += 8;
    display.setCursor(0, y); display.print("Link: "); display.print(loraPeerFound() ? "OK" : "WAIT"); y += 8;
    display.setCursor(0, y); display.print("TX: "); display.print(lastPeerId ? shortIdString(lastPeerId) : "WAIT"); y += 8;
    display.setCursor(0, y); display.print("Out: "); display.print(outputLabel(gatewayOutputMode)); display.print(" ");
    if (outputWantsOsc()) display.print(WiFi.status() == WL_CONNECTED ? "OK" : "WAIT");
    else if (outputWantsBle()) display.print(bleConnected ? "OK" : "WAIT");
    y += 8;
    display.setCursor(0, y); display.print("ID: "); display.print(shortIdString(deviceId));
  }

  // Keep status screen readable: menu/back hint is intentionally omitted here.
  display.display();
}

const uint8_t* activeMenuItems() {
  if (controlMode == MODE_OSC_WIFI) return MENU_OSC_ITEMS;
  if (controlMode == MODE_BLE_HID) return MENU_BLE_ITEMS;
  if (controlMode == MODE_LORA_REMOTE) return MENU_LORA_REMOTE_ITEMS;
  if (controlMode == MODE_LORA_GATEWAY) return MENU_LORA_GATEWAY_ITEMS;
  return MENU_OSC_ITEMS;
}

int activeMenuCount() {
  if (controlMode == MODE_OSC_WIFI) return sizeof(MENU_OSC_ITEMS) / sizeof(MENU_OSC_ITEMS[0]);
  if (controlMode == MODE_BLE_HID) return sizeof(MENU_BLE_ITEMS) / sizeof(MENU_BLE_ITEMS[0]);
  if (controlMode == MODE_LORA_REMOTE) return sizeof(MENU_LORA_REMOTE_ITEMS) / sizeof(MENU_LORA_REMOTE_ITEMS[0]);
  if (controlMode == MODE_LORA_GATEWAY) return sizeof(MENU_LORA_GATEWAY_ITEMS) / sizeof(MENU_LORA_GATEWAY_ITEMS[0]);
  return sizeof(MENU_OSC_ITEMS) / sizeof(MENU_OSC_ITEMS[0]);
}

uint8_t activeMenuItem() {
  const uint8_t* items = activeMenuItems();
  int count = activeMenuCount();
  if (currentMenuIndex >= count) currentMenuIndex = 0;
  return items[currentMenuIndex];
}

void printMenuLabel(uint8_t item) {
  switch (item) {
    case MENU_MODE:
      display.print("Mode: "); display.print(shortModeLabel(controlMode));
      break;
    case MENU_SETUP:
      if (controlMode == MODE_OSC_WIFI || (controlMode == MODE_LORA_GATEWAY && outputWantsOsc())) display.print("WiFi Setup");
      else if (controlMode == MODE_BLE_HID) display.print("BLE Info");
      else if (controlMode == MODE_LORA_GATEWAY) display.print("RX Info");
      else display.print("TX Info");
      break;
    case MENU_STATUS:
      display.print("Status");
      break;
    case MENU_LORA_FREQ:
      display.print("Freq: "); display.print(radioCfg.freq, 1);
      break;
    case MENU_LORA_POWER:
      display.print("Power: "); display.print(radioCfg.power); display.print("dBm");
      break;
    case MENU_PAIR:
      display.print("Pair RX");
      break;
    case MENU_OUTPUT:
      display.print("RX->"); display.print(outputLabel(gatewayOutputMode));
      break;
    case MENU_RESET:
      display.print("Reset");
      break;
    case MENU_REBOOT:
      display.print("Power Off");
      break;
    case MENU_BACK:
      display.print("Back");
      break;
  }
}

void drawMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("MENU", 0, 2);

  int total = activeMenuCount();
  const int visible = 4;

  if (currentMenuIndex < menuScroll) menuScroll = currentMenuIndex;
  if (currentMenuIndex >= menuScroll + visible) menuScroll = currentMenuIndex - visible + 1;
  if (menuScroll < 0) menuScroll = 0;
  if (menuScroll > max(0, total - visible)) menuScroll = max(0, total - visible);

  const uint8_t* items = activeMenuItems();
  display.setTextSize(1);
  int y = 16;
  for (int i = 0; i < visible && (menuScroll + i) < total; i++) {
    int idx = menuScroll + i;
    if (idx == currentMenuIndex) {
      display.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(4, y);
    printMenuLabel(items[idx]);
    y += 10;
  }

  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("click next hold ok", 56, 1);
  display.display();
}

void drawModeConfirm() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCenteredText("SWITCH MODE?", 0, 1);

  String transition = String(shortModeLabel(controlMode)) + " -> " + String(shortModeLabel(selectedMode));
  drawCenteredText(transition.c_str(), 18, 2);
  drawCenteredText(selectedMode == MODE_LORA_GATEWAY ? "then choose output" : modeHint(selectedMode), 38, 1);
  drawCenteredText("hold: yes", 49, 1);
  drawCenteredText("click: no", 57, 1);

  display.display();
}

void drawModeInfo() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (controlMode == MODE_BLE_HID) {
    drawCenteredText("BLE INFO", 0, 2);
    display.setTextSize(1);
    display.setCursor(0, 18); display.print("Device:");
    display.setCursor(0, 29); display.print(getUniqueName());
    display.setCursor(0, 41); display.print(bleConnected ? "Connected" : "Waiting pair");
    display.setCursor(0, 54); display.print("GO=SPACE PANIC=ESC");
  } else if (controlMode == MODE_OSC_WIFI) {
    drawCenteredText("WiFi INFO", 0, 2);
    display.setTextSize(1);
    display.setCursor(0, 18); display.print("AP:");
    display.setCursor(0, 29); display.print(getUniqueName());
    display.setCursor(0, 41); display.print("Pass: password123");
    display.setCursor(0, 54); display.print("Setup in menu");
  } else if (isLoRaRemote()) {
    drawCenteredText("LoRa TX", 0, 2);
    display.setTextSize(1);
    display.setCursor(0, 18); display.print("Needs LoRa RX");
    display.setCursor(0, 30); display.print("Freq: "); display.print(radioCfg.freq, 1); display.print("MHz");
    display.setCursor(0, 42); display.print("GO sends LoRa pkt");
    display.setCursor(0, 54); display.print("No ACK delay");
  } else if (isLoRaGateway()) {
    drawCenteredText("LoRa RX", 0, 2);
    display.setTextSize(1);
    display.setCursor(0, 18); display.print("Receives TX");
    display.setCursor(0, 30); display.print("RX->"); display.print(outputFullLabel(gatewayOutputMode));
    display.setCursor(0, 42); display.print("Freq: "); display.print(radioCfg.freq, 1); display.print("MHz");
    display.setCursor(0, 54); display.print("Output in menu");
  } else {
    drawCenteredText("INFO", 0, 2);
    display.setTextSize(1);
    display.setCursor(0, 18); display.print("Mode not active");
    display.setCursor(0, 30); display.print("Use Mode menu");
  }

  display.display();
}

// ============================================================================
// Boot animation
// ============================================================================
void startBootAnimation() {
  display.setTextSize(2);
  display.setTextColor(SSD1306_BLACK);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds("soundkorb", 0, 0, &x1, &y1, &w, &h);
  textX = (SCREEN_WIDTH - w) / 2;
  textY = (SCREEN_HEIGHT - h) / 2;

  bootPhase = BOOT_DOT;
  bootTimer = millis();
  bootStep = 0;
}

void updateBootAnimation() {
  if (currentScreen != SCREEN_BOOT) return;

  const int CENTER_Y = 32;

  switch (bootPhase) {
    case BOOT_DOT: {
      display.clearDisplay();
      display.fillRect(SCREEN_WIDTH / 2 - 1, CENTER_Y - 1, 2, 2, SSD1306_WHITE);
      display.display();
      if (millis() - bootTimer > 200) {
        bootPhase = BOOT_LINE_GROW;
        bootTimer = millis();
        bootStep = 2;
      }
      break;
    }

    case BOOT_LINE_GROW: {
      if (bootStep < SCREEN_WIDTH) {
        display.clearDisplay();
        int x = (SCREEN_WIDTH - bootStep) / 2;
        display.fillRect(x, CENTER_Y - 1, bootStep, 2, SSD1306_WHITE);
        display.display();
        bootStep += 6;
        if (bootStep > SCREEN_WIDTH) bootStep = SCREEN_WIDTH;
      } else {
        bootPhase = BOOT_TEXT_GROW;
        bootTimer = millis();
        bootStep = 31;
      }
      break;
    }

    case BOOT_TEXT_GROW: {
      if (bootStep > 0) {
        display.clearDisplay();
        display.fillRect(0, bootStep, SCREEN_WIDTH, SCREEN_HEIGHT - 2 * bootStep, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setTextSize(2);
        display.setCursor(textX, textY);
        display.print("soundkorb");
        display.fillRect(0, 0, SCREEN_WIDTH, bootStep, SSD1306_BLACK);
        display.fillRect(0, SCREEN_HEIGHT - bootStep, SCREEN_WIDTH, bootStep, SSD1306_BLACK);
        display.display();
        bootStep -= 3;
        if (bootStep < 0) bootStep = 0;
      } else {
        bootPhase = BOOT_PEAK;
        bootTimer = millis();
      }
      break;
    }

    case BOOT_PEAK: {
      display.clearDisplay();
      display.fillScreen(SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setTextSize(2);
      display.setCursor(textX, textY);
      display.print("soundkorb");
      display.display();
      if (millis() - bootTimer > 600) {
        bootPhase = BOOT_TEXT_SHRINK;
        bootTimer = millis();
        bootStep = 0;
      }
      break;
    }

    case BOOT_TEXT_SHRINK: {
      if (bootStep < 31) {
        display.clearDisplay();
        display.fillRect(0, bootStep, SCREEN_WIDTH, SCREEN_HEIGHT - 2 * bootStep, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setTextSize(2);
        display.setCursor(textX, textY);
        display.print("soundkorb");
        display.fillRect(0, 0, SCREEN_WIDTH, bootStep, SSD1306_BLACK);
        display.fillRect(0, SCREEN_HEIGHT - bootStep, SCREEN_WIDTH, bootStep, SSD1306_BLACK);
        display.display();
        bootStep += 3;
        if (bootStep > 31) bootStep = 31;
      } else {
        bootPhase = BOOT_LINE_SHRINK;
        bootTimer = millis();
        bootStep = SCREEN_WIDTH;
      }
      break;
    }

    case BOOT_LINE_SHRINK: {
      if (bootStep > 2) {
        display.clearDisplay();
        int x = (SCREEN_WIDTH - bootStep) / 2;
        display.fillRect(x, CENTER_Y - 1, bootStep, 2, SSD1306_WHITE);
        display.display();
        bootStep -= 8;
        if (bootStep < 2) bootStep = 2;
      } else {
        bootPhase = BOOT_OFF;
        bootTimer = millis();
      }
      break;
    }

    case BOOT_OFF: {
      display.clearDisplay();
      display.display();
      if (millis() - bootTimer > 200) {
        bootPhase = BOOT_DONE;
        afterBoot();
      }
      break;
    }

    case BOOT_DONE:
      break;
  }
}

void updateLoRaLinkScreen() {
  if (!isLoRaMode()) return;
  if (currentScreen == SCREEN_BOOT || currentScreen == SCREEN_MENU || currentScreen == SCREEN_STATUS ||
      currentScreen == SCREEN_MODE_SELECT || currentScreen == SCREEN_MODE_CONFIRM || currentScreen == SCREEN_OUTPUT_SELECT ||
      currentScreen == SCREEN_PAIR_SELECT || currentScreen == SCREEN_GO_SENT || currentScreen == SCREEN_PANIC_SENT ||
      currentScreen == SCREEN_MODE_INFO) return;

  bool found = loraPeerFound();
  if (!found) {
    if (lastPeerId == 0) {
      if (currentScreen != SCREEN_LORA_SEARCH) setScreen(SCREEN_LORA_SEARCH);
    } else {
      if (currentScreen != SCREEN_LORA_LINK_LOST) setScreen(SCREEN_LORA_LINK_LOST);
    }
    return;
  }

  if (currentScreen == SCREEN_LORA_SEARCH || currentScreen == SCREEN_LORA_LINK_LOST || currentScreen == SCREEN_NO_CONNECTION) {
    setScreen(SCREEN_LORA_LINK_OK);
    return;
  }

  if (currentScreen == SCREEN_LORA_LINK_OK && millis() - screenChangedAt > 900) {
    setScreen(SCREEN_GO);
  }
}
