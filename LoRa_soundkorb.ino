// GO-GO — theatre remote (Heltec WiFi LoRa 32 V3)
// v16: split the v15 monolith into modules 1:1, then re-applied the June
//      review fixes: no NVS write per packet (counter block reservation),
//      GO/PANIC sent 3x with shared counter bypassing the heartbeat throttle,
//      uint16 counter-wraparound-safe dedupe, calibrated battery ADC,
//      silent WiFi reconnect in loop, Reboot menu item renamed to Power Off.
// Modules: config.h (types/pins/state), util, settings, battery, osc_wifi,
// ble_hid, radio, modes, ui_screens. This file owns global state + setup/loop.
// v15: replace menu Reboot with Power Off, force RX->OSC WiFi setup, improve LED diagnostics.
// Goals: keep proven WiFi/OSC and BLE/HID, remove USB user mode,
// make LoRa TX/RX link state obvious on OLED and LED, avoid automatic
// long WiFi portals, and keep LoRa commands fire-and-forget for low latency.

#include "gogo.h"

// ============================================================================
// Global objects
// ============================================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
Preferences prefs;
WiFiUDP udp;
SX1262 lora = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);

// ============================================================================
// Global state
// ============================================================================
OscConfig config;
RadioConfig radioCfg;

uint8_t gatewayOutputMode = OUT_BLE;
uint8_t selectedOutputMode = OUT_BLE;

ControlMode controlMode = MODE_OSC_WIFI;
ControlMode selectedMode = MODE_OSC_WIFI;
bool modeConfigured = false;

Screen currentScreen = SCREEN_BOOT;
Screen previousScreen = SCREEN_GO;
unsigned long screenChangedAt = 0;

uint8_t currentMenuIndex = 0;
int menuScroll = 0;

bool buttonPressed = false;
bool longActionFired = false;
unsigned long pressStart = 0;
unsigned long lastScreenUpdate = 0;

unsigned long lastShortPressTime = 0;
int shortPressCount = 0;
unsigned long messageTime = 0;

bool batteryPresent = false;
int batteryPercent = 0;
unsigned long lastBatteryRead = 0;

unsigned long lastLedToggle = 0;
bool ledState = false;
LedMode ledMode = LED_OFF;
LedMode lastAppliedLedMode = LED_OFF;
unsigned long lastLedModeUpdate = 0;

NimBLEHIDDevice *hid = nullptr;
NimBLECharacteristic *inputKeyboard = nullptr;
NimBLEServer *bleServer = nullptr;
bool bleStarted = false;
bool bleConnected = false;
volatile bool bleConnectionChanged = false;

volatile bool loraEventFlag = false;
bool loraInitialized = false;
bool loraInitFailed = false;
uint32_t deviceId = 0;
LoRaPacket lastPacket = {};

bool linkOK = false;
unsigned long lastLinkSeen = 0;
unsigned long lastPingTime = 0;
unsigned long lastBeaconTime = 0;

uint32_t lastPeerId = 0;
int16_t lastPeerRssi = 0;
float lastPeerSnr = 0.0f;
uint32_t rxPacketCount = 0;
uint32_t txPacketCount = 0;
unsigned long lastTxTimestamp = 0;
uint8_t lastRxCommand = 0;
unsigned long lastCommandSeen = 0;

bool waitingForAck = false;
bool ackOk = false;
uint8_t pendingCommand = 0;
uint32_t pendingTarget = 0xFFFFFFFF;
int retryCount = 0;
unsigned long lastTxTime = 0;

bool gatewayWifiReady = false;

DiscoveredPeer discoveredPeers[6];
uint8_t selectedPeerIndex = 0;
uint32_t pairedRxId = 0;   // 0 = ANY RX on this channel

// ============================================================================
// Setup / Loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(180);
  DBGLN("\n[BOOT] GO-GO Universal v15 poweroff-wifi-led-fix");

  VextON();
  delay(100);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;) {}
  }
  display.clearDisplay();
  display.display();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GO_LED_PIN, OUTPUT);
  pinMode(PIN_BATTERY_CTRL, OUTPUT);
  ledWrite(false);
  digitalWrite(PIN_BATTERY_CTRL, LOW);
  ledSelfTest();

  analogReadResolution(12);

  uint64_t mac = ESP.getEfuseMac();
  deviceId = hardwareId32();

  loadOscConfig();
  loadControlMode();
  loadRadioConfig();
  loadPairedRxId();

  updateBattery();
  lastBatteryRead = millis();

  setScreen(SCREEN_BOOT);
  startBootAnimation();
}

void loop() {
  unsigned long now = millis();

  if (now - lastBatteryRead >= BATTERY_READ_INTERVAL) {
    lastBatteryRead = now;
    updateBattery();
  }

  if (currentScreen == SCREEN_BOOT) {
    updateBootAnimation();
    return;
  }

  // Silent WiFi reconnect for OSC outputs: no captive portal, no UI blocking.
  static unsigned long lastWifiRetry = 0;
  if ((controlMode == MODE_OSC_WIFI || (controlMode == MODE_LORA_GATEWAY && outputWantsOsc())) &&
      WiFi.status() != WL_CONNECTED && now - lastWifiRetry >= 15000) {
    lastWifiRetry = now;
    WiFi.reconnect();
  }

  // Runtime mode checks
  if (bleOutputActive() && bleServer != nullptr) {
    bool newBleState = (bleServer->getConnectedCount() > 0);
    if (newBleState != bleConnected) {
      bleConnected = newBleState;
      bleConnectionChanged = true;
      if (!bleConnected) NimBLEDevice::startAdvertising();
    }
  }

  if (bleOutputActive() && bleConnectionChanged) {
    bleConnectionChanged = false;
    if (currentScreen == SCREEN_GO || currentScreen == SCREEN_STATUS || currentScreen == SCREEN_NO_CONNECTION) {
      // live redraw below
    }
  }

  if (isLoRaMode() && loraInitialized && loraEventFlag) {
    processLoRaPacket();
  }

  if (isLoRaRemote()) {
    sendHeartbeat();
    // Fire-and-forget commands in v7; ACK retry is intentionally disabled.
  }

  if (isLoRaGateway()) {
    sendGatewayBeacon();
    gatewayWifiReady = outputWantsOsc() && (WiFi.status() == WL_CONNECTED);
  }

  updateLoRaLinkScreen();
  if (now - lastLedModeUpdate >= 100) {
    lastLedModeUpdate = now;
    updateLedMode();
  }
  updateLed();

  // Short-click resolver for main work screens.
  // 1 click = GO, fast 3+ clicks = MENU. Panic is no longer a click sequence.
  if (shortPressCount > 0 && (now - lastShortPressTime >= SHORT_PRESS_TIMEOUT)) {
    if (currentScreen == SCREEN_GO || currentScreen == SCREEN_LORA_LINK_OK) {
      if (shortPressCount >= MENU_CLICK_COUNT) {
        previousScreen = currentScreen;
        currentMenuIndex = 0;
        menuScroll = 0;
        setScreen(SCREEN_MENU);
      } else if (shortPressCount == 1) {
        performGO();
      }
      // 2 clicks are intentionally ignored to avoid accidental commands.
    }
    shortPressCount = 0;
  }

  bool state = digitalRead(BUTTON_PIN) == LOW;

  if (state && !buttonPressed) {
    buttonPressed = true;
    longActionFired = false;
    pressStart = now;
  }

  // Panic is a deliberate hold on the main work screens.
  // It fires while the button is still held, so release will not trigger GO/menu.
  if (state && buttonPressed && !longActionFired &&
      (currentScreen == SCREEN_GO || currentScreen == SCREEN_LORA_LINK_OK) &&
      (now - pressStart >= PANIC_HOLD_MS)) {
    longActionFired = true;
    shortPressCount = 0;
    performPanic();
  }

  if (!state && buttonPressed) {
    unsigned long dur = now - pressStart;
    buttonPressed = false;
    DBG("[BTN] release dur="); DBGLN(dur);

    if (longActionFired) {
      longActionFired = false;
      return;
    }

    if (currentScreen == SCREEN_MODE_SELECT) {
      if (dur < HOLD_SELECT_MS) {
        selectedMode = (ControlMode)nextMode(selectedMode);
        drawModeSelect();
      } else {
        if (!modeConfigured) {
          startModeNow(selectedMode);
        } else if (selectedMode == controlMode) {
          if (selectedMode == MODE_LORA_GATEWAY) {
            selectedOutputMode = gatewayOutputMode;
            setScreen(SCREEN_OUTPUT_SELECT);
          } else {
            setScreen(SCREEN_MENU);
          }
        } else {
          setScreen(SCREEN_MODE_CONFIRM);
        }
      }
    }

    else if (currentScreen == SCREEN_OUTPUT_SELECT) {
      if (dur < HOLD_SELECT_MS) {
        selectedOutputMode = nextOutput(selectedOutputMode);
        drawOutputSelect();
      } else {
        if (!modeConfigured || controlMode != MODE_LORA_GATEWAY) {
          startRxNow(selectedOutputMode);
        } else {
          restartIntoRxOutput(selectedOutputMode);
        }
      }
    }

    else if (currentScreen == SCREEN_PAIR_SELECT) {
      int count = discoveredPeerCount();
      if (dur < HOLD_SELECT_MS) {
        if (count > 0) selectedPeerIndex = (selectedPeerIndex + 1) % count;
        drawPairSelect();
      } else {
        if (count > 0) {
          int idx = discoveredIndexByVisibleOrder(selectedPeerIndex);
          if (idx >= 0) savePairedRxId(discoveredPeers[idx].id);
          showSimpleMessage("RX paired", peerTargetString().c_str(), 800);
        } else {
          savePairedRxId(0);
          showSimpleMessage("Pair cleared", "Target: ANY", 800);
        }
        setScreen(SCREEN_GO);
      }
    }

    else if (currentScreen == SCREEN_MODE_CONFIRM) {
      if (dur < HOLD_SELECT_MS) {
        setScreen(SCREEN_MENU);
      } else {
        restartIntoMode(selectedMode);
      }
    }

    else if (currentScreen == SCREEN_GO) {
      if (dur < PANIC_HOLD_MS) {
        shortPressCount++;
        lastShortPressTime = now;
        // Open MENU immediately on the third fast click; do not wait for timeout.
        if (shortPressCount >= MENU_CLICK_COUNT) {
          shortPressCount = 0;
          previousScreen = currentScreen;
          currentMenuIndex = 0;
          menuScroll = 0;
          setScreen(SCREEN_MENU);
        }
      }
    }

    else if (currentScreen == SCREEN_STATUS) {
      if (dur < HOLD_SELECT_MS) {
        previousScreen = SCREEN_STATUS;
        currentMenuIndex = 0;
        menuScroll = 0;
        setScreen(SCREEN_MENU);
      } else {
        setScreen(SCREEN_GO);
      }
    }

    else if (currentScreen == SCREEN_MENU) {
      if (dur < HOLD_SELECT_MS) {
        int count = activeMenuCount();
        currentMenuIndex = (currentMenuIndex + 1) % count;
      } else {
        handleMenuSelect();
      }
    }

    else if (currentScreen == SCREEN_MODE_INFO) {
      if (dur < HOLD_SELECT_MS) setScreen(SCREEN_MENU);
      else setScreen(SCREEN_GO);
    }

    else if (currentScreen == SCREEN_LORA_LINK_OK) {
      if (dur < PANIC_HOLD_MS) {
        shortPressCount++;
        lastShortPressTime = now;
        // Open MENU immediately on the third fast click; do not wait for timeout.
        if (shortPressCount >= MENU_CLICK_COUNT) {
          shortPressCount = 0;
          previousScreen = currentScreen;
          currentMenuIndex = 0;
          menuScroll = 0;
          setScreen(SCREEN_MENU);
        }
      }
    }

    else if (currentScreen == SCREEN_LORA_SEARCH || currentScreen == SCREEN_LORA_LINK_LOST) {
      if (dur < HOLD_SELECT_MS) {
        retryConnection();
      } else {
        previousScreen = currentScreen;
        currentMenuIndex = 0;
        menuScroll = 0;
        setScreen(SCREEN_MENU);
      }
    }

    else if (currentScreen == SCREEN_NO_CONNECTION) {
      if (dur < HOLD_SELECT_MS) {
        retryConnection();
      } else {
        previousScreen = SCREEN_NO_CONNECTION;
        currentMenuIndex = 0;
        menuScroll = 0;
        setScreen(SCREEN_MENU);
      }
    }
  }

  if ((currentScreen == SCREEN_GO_SENT || currentScreen == SCREEN_PANIC_SENT) &&
      !waitingForAck && (now - messageTime >= MESSAGE_DURATION)) {
    setScreen(SCREEN_GO);
  }

  if ((currentScreen == SCREEN_GO_SENT || currentScreen == SCREEN_PANIC_SENT) &&
      waitingForAck && (now - messageTime >= 4500)) {
    waitingForAck = false;
    ackOk = false;
    setScreen(SCREEN_NO_CONNECTION);
    messageTime = now;
  }

  // Live redraw
  if (now - lastScreenUpdate >= 200) {
    lastScreenUpdate = now;

    switch (currentScreen) {
      case SCREEN_GO:
        drawGoScreen();
        break;
      case SCREEN_STATUS:
        drawStatusScreen();
        break;
      case SCREEN_MENU:
        drawMenu();
        break;
      case SCREEN_MODE_SELECT:
        drawModeSelect();
        break;
      case SCREEN_OUTPUT_SELECT:
        drawOutputSelect();
        break;
      case SCREEN_PAIR_SELECT:
        drawPairSelect();
        break;
      case SCREEN_LORA_SEARCH:
        drawLoraSearch();
        break;
      case SCREEN_LORA_LINK_OK:
        drawLoraLinkOK();
        break;
      case SCREEN_LORA_LINK_LOST:
        drawLoraLinkLost();
        break;
      case SCREEN_MODE_CONFIRM:
        drawModeConfirm();
        break;
      case SCREEN_MODE_INFO:
        drawModeInfo();
        break;
      case SCREEN_GO_SENT:
        drawGoSent();
        break;
      case SCREEN_PANIC_SENT:
        drawPanicSent();
        break;
      case SCREEN_NO_CONNECTION:
        drawNoConnection();
        break;
      default:
        break;
    }
  }
}
