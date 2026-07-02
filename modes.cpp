// GO-GO — modes module (split 1:1 from v15 monolith).
#include "gogo.h"

bool outputWantsOsc() {
  return gatewayOutputMode == OUT_OSC;
}

bool outputWantsBle() {
  return gatewayOutputMode == OUT_BLE;
}

bool bleOutputActive() {
  return controlMode == MODE_BLE_HID || (controlMode == MODE_LORA_GATEWAY && outputWantsBle());
}

// ============================================================================
// Mode init / reset
// ============================================================================
bool initSelectedMode(bool allowWifiPortal) {
  stopBLEMode();
  if (!isLoRaMode()) stopLoRa();

  if (controlMode == MODE_OSC_WIFI) {
    WiFi.mode(WIFI_STA);
    return connectWiFiWithDisplay(allowWifiPortal);
  }

  if (controlMode == MODE_BLE_HID) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    startBLEMode();
    return true;
  }

  if (controlMode == MODE_LORA_REMOTE) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    bool ok = initLoRa();
    linkOK = false;
    lastPingTime = 0;
    lastLinkSeen = 0;
    lastPeerId = 0;
    rxPacketCount = 0;
    txPacketCount = 0;
    return ok;
  }

  if (controlMode == MODE_LORA_GATEWAY) {
    bool ok = initLoRa();

    if (outputWantsOsc()) {
      // RX -> OSC tries saved WiFi only unless user explicitly opens WiFi Setup.
      // No long captive portal during normal mode changes.
      stopBLEMode();
      WiFi.mode(WIFI_STA);
      gatewayWifiReady = connectWiFiWithDisplay(allowWifiPortal);
      if (gatewayWifiReady) udp.begin(0);
    } else {
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      gatewayWifiReady = false;
    }

    if (outputWantsBle()) {
      startBLEMode();
    }

    return ok;
  }

  return false;
}

void factoryReset() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("RESET", 16, 2);
  drawCenteredText("Erasing config", 40, 1);
  display.display();

  WiFiManager wm;
  wm.resetSettings();

  prefs.begin("osc", false); prefs.clear(); prefs.end();
  prefs.begin("system", false); prefs.clear(); prefs.end();
  prefs.begin("radio", false); prefs.clear(); prefs.end();

  WiFi.disconnect(true, true);
  delay(1200);
  ESP.restart();
}

void startModeNow(uint8_t mode) {
  if (mode == MODE_LORA_GATEWAY) {
    selectedOutputMode = gatewayOutputMode;
    setScreen(SCREEN_OUTPUT_SELECT);
    return;
  }

  saveControlMode(mode);
  controlMode = (ControlMode)mode;
  selectedMode = (ControlMode)mode;
  modeConfigured = true;

  showSimpleMessage("Mode saved", modeLabel(mode), 500);
  bool ready = initSelectedMode(mode == MODE_OSC_WIFI);

  // Important UX rule: after choosing LoRa TX, immediately choose/pair the RX.
  // Do not drop the user to the GO screen before target selection.
  if (mode == MODE_LORA_REMOTE && ready) {
    selectedPeerIndex = 0;
    setScreen(SCREEN_PAIR_SELECT);
    return;
  }

  if (ready) {
    setScreen(SCREEN_GO);
  } else {
    setScreen(SCREEN_NO_CONNECTION);
    messageTime = millis();
  }
}

void startRxNow(uint8_t out) {
  gatewayOutputMode = out;
  selectedOutputMode = out;
  saveControlModeAndOutput(MODE_LORA_GATEWAY, out);
  controlMode = MODE_LORA_GATEWAY;
  selectedMode = MODE_LORA_GATEWAY;
  modeConfigured = true;

  String msg = String("RX -> ") + String(outputFullLabel(out));
  showSimpleMessage("Mode saved", msg.c_str(), 500);

  bool ready = false;
  if (out == OUT_OSC) {
    // Explicit first-time setup path for RX->OSC:
    // 1) start LoRa receiver, 2) try saved WiFi briefly, 3) if missing, open WiFiManager AP.
    stopBLEMode();
    bool radioOk = initLoRa();
    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) {
      bool quickConnect = connectWiFiWithDisplay(false);
      if (!quickConnect) {
        configureWiFi(false);
      }
    }
    gatewayWifiReady = (WiFi.status() == WL_CONNECTED);
    if (gatewayWifiReady) udp.begin(0);
    ready = radioOk && gatewayWifiReady;
  } else {
    ready = initSelectedMode(false);
  }

  if (ready) {
    setScreen(SCREEN_GO);
  } else {
    setScreen(SCREEN_NO_CONNECTION);
    messageTime = millis();
  }
}

void restartIntoMode(uint8_t mode) {
  // v11: switch mode in-place. No forced reboot unless user chooses Reboot.
  if (mode == MODE_LORA_GATEWAY) {
    selectedOutputMode = gatewayOutputMode;
    setScreen(SCREEN_OUTPUT_SELECT);
    return;
  }
  saveControlMode(mode);
  controlMode = (ControlMode)mode;
  selectedMode = (ControlMode)mode;
  modeConfigured = true;
  showSimpleMessage("Mode saved", modeLabel(mode), 500);
  bool ready = initSelectedMode(mode == MODE_OSC_WIFI);
  if (mode == MODE_LORA_REMOTE && ready) {
    selectedPeerIndex = 0;
    setScreen(SCREEN_PAIR_SELECT);
  } else {
    setScreen(ready ? SCREEN_GO : SCREEN_NO_CONNECTION);
    messageTime = millis();
  }
}

void restartIntoRxOutput(uint8_t out) {
  // v11: change RX output in-place. No forced reboot.
  saveControlModeAndOutput(MODE_LORA_GATEWAY, out);
  gatewayOutputMode = out;
  selectedOutputMode = out;
  controlMode = MODE_LORA_GATEWAY;
  selectedMode = MODE_LORA_GATEWAY;
  modeConfigured = true;
  showSimpleMessage("RX output", outputFullLabel(out), 500);
  bool ready = false;
  if (out == OUT_OSC) {
    // Explicit setup path when switching RX output to OSC from the menu.
    stopBLEMode();
    bool radioOk = initLoRa();
    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) {
      bool quickConnect = connectWiFiWithDisplay(false);
      if (!quickConnect) {
        configureWiFi(false);
      }
    }
    gatewayWifiReady = (WiFi.status() == WL_CONNECTED);
    if (gatewayWifiReady) udp.begin(0);
    ready = radioOk && gatewayWifiReady;
  } else {
    ready = initSelectedMode(false);
  }
  setScreen(ready ? SCREEN_GO : SCREEN_NO_CONNECTION);
  messageTime = millis();
}

void afterBoot() {
  DBGLN("[BOOT] afterBoot()");

  // Emergency mode selector: hold the hardware button while booting.
  // This is the second way to change mode if the saved mode is inconvenient.
  if (digitalRead(BUTTON_PIN) == LOW) {
    selectedMode = modeConfigured ? controlMode : MODE_OSC_WIFI;
    setScreen(SCREEN_MODE_SELECT);
    drawModeSelect();
    return;
  }

  if (!modeConfigured) {
    selectedMode = MODE_OSC_WIFI;
    setScreen(SCREEN_MODE_SELECT);
    drawModeSelect();
    return;
  }

  DBG("[MODE] saved: "); DBGLN(modeLabel(controlMode));

  // Saved mode must boot fast. Do not open long WiFi portals automatically.
  // If WiFi is missing, show NO WIFI; user can click retry or open menu -> WiFi Setup.
  bool ready = initSelectedMode(false);

  // If the saved mode is LoRa TX and no RX is paired yet, go straight to Pair RX.
  if (controlMode == MODE_LORA_REMOTE && pairedRxId == 0 && ready) {
    selectedPeerIndex = 0;
    setScreen(SCREEN_PAIR_SELECT);
    drawPairSelect();
    return;
  }

  if (ready) setScreen(SCREEN_GO);
  else {
    setScreen(SCREEN_NO_CONNECTION);
    messageTime = millis();
  }
}

// ============================================================================
// Actions
// ============================================================================
bool isConnectionReady() {
  if (controlMode == MODE_OSC_WIFI) return WiFi.status() == WL_CONNECTED;
  if (controlMode == MODE_BLE_HID) return bleConnected;
  if (controlMode == MODE_LORA_REMOTE) return loraInitialized;
  if (controlMode == MODE_LORA_GATEWAY) {
    return loraInitialized;
  }
  return false;
}

void performGO() {
  DBGLN("[ACTION] GO");

  if (!isConnectionReady()) {
    setScreen(SCREEN_NO_CONNECTION);
    messageTime = millis();
    return;
  }

  if (controlMode == MODE_OSC_WIFI) {
    sendOSC(config.osc_address);
    ackOk = true;
  } else if (controlMode == MODE_BLE_HID) {
    sendKeyPress(0x2C); // SPACE
    ackOk = true;
  } else if (isLoRaRemote()) {
    // Command path must be fast: fire-and-forget.
    // Link heartbeat/PONG runs in the background, but GO does not wait for ACK.
    waitingForAck = false;
    ackOk = sendCommand(PKT_GO, pairedRxId ? pairedRxId : 0xFFFFFFFF);
  } else if (isLoRaGateway()) {
    emitGatewayAction(PKT_GO, deviceId);
    ackOk = true;
  }

  blinkLedOnce();
  setScreen(SCREEN_GO_SENT);
  messageTime = millis();
}

void performPanic() {
  DBGLN("[ACTION] PANIC");

  if (!isConnectionReady()) {
    setScreen(SCREEN_NO_CONNECTION);
    messageTime = millis();
    return;
  }

  if (controlMode == MODE_OSC_WIFI) {
    sendOSC(config.panic_address);
    ackOk = true;
  } else if (controlMode == MODE_BLE_HID) {
    sendKeyPress(0x29); // ESC
    ackOk = true;
  } else if (isLoRaRemote()) {
    // Command path must be fast: fire-and-forget.
    waitingForAck = false;
    ackOk = sendCommand(PKT_PANIC, pairedRxId ? pairedRxId : 0xFFFFFFFF);
  } else if (isLoRaGateway()) {
    emitGatewayAction(PKT_PANIC, deviceId);
    ackOk = true;
  }

  blinkLedOnce();
  setScreen(SCREEN_PANIC_SENT);
  messageTime = millis();
}

void powerOffDevice() {
  showSimpleMessage("Power Off", "press RESET to wake", 700);

  // Stop active radios before sleep. The physical RESET button is the normal wake path.
  stopBLEMode();
  stopLoRa();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  ledWrite(false);
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  // Keep the BOOT/user button as an optional wake source as well.
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
  delay(100);
  esp_deep_sleep_start();
}

void handleMenuSelect() {
  uint8_t item = activeMenuItem();

  switch (item) {
    case MENU_MODE:
      selectedMode = controlMode;
      setScreen(SCREEN_MODE_SELECT);
      drawModeSelect();
      break;

    case MENU_SETUP:
      if (controlMode == MODE_OSC_WIFI || (controlMode == MODE_LORA_GATEWAY && outputWantsOsc())) {
        configureWiFi(false);
        setScreen(SCREEN_GO);
      } else {
        setScreen(SCREEN_MODE_INFO);
      }
      break;

    case MENU_STATUS:
      previousScreen = SCREEN_MENU;
      setScreen(SCREEN_STATUS);
      break;

    case MENU_LORA_FREQ:
      cycleFrequency();
      setScreen(SCREEN_MENU);
      break;

    case MENU_LORA_POWER:
      cyclePower();
      setScreen(SCREEN_MENU);
      break;

    case MENU_REGION:
      cycleRegion();
      setScreen(SCREEN_MENU);
      break;

    case MENU_TUNE:
      cycleTune();
      setScreen(SCREEN_MENU);
      break;

    case MENU_PAIR:
      selectedPeerIndex = 0;
      setScreen(SCREEN_PAIR_SELECT);
      break;

    case MENU_OUTPUT:
      selectedOutputMode = gatewayOutputMode;
      setScreen(SCREEN_OUTPUT_SELECT);
      break;

    case MENU_RESET:
      factoryReset();
      break;

    case MENU_POWER_OFF:
      powerOffDevice();
      break;

    case MENU_BACK:
      setScreen(SCREEN_GO);
      break;
  }
}

void retryConnection() {
  if (controlMode == MODE_OSC_WIFI) {
    bool ok = connectWiFiWithDisplay(true);
    setScreen(ok ? SCREEN_GO : SCREEN_NO_CONNECTION);
    messageTime = millis();
  } else if (controlMode == MODE_BLE_HID) {
    startBLEMode();
    setScreen(bleConnected ? SCREEN_GO : SCREEN_NO_CONNECTION);
    messageTime = millis();
  } else if (isLoRaRemote()) {
    if (!loraInitialized) initLoRa();
    sendPacket(PKT_PING, pairedRxId ? pairedRxId : 0xFFFFFFFF, 0);
    setScreen(SCREEN_GO);
  } else if (isLoRaGateway()) {
    if (!loraInitialized) initLoRa();
    if (outputWantsOsc() && WiFi.status() != WL_CONNECTED) connectWiFiWithDisplay(true);
    if (outputWantsBle()) startBLEMode();
    setScreen(loraInitialized ? SCREEN_GO : SCREEN_NO_CONNECTION);
  }
}
