// GO-GO — theatre remote (Heltec WiFi LoRa 32 V3)
// v15: replace menu Reboot with Power Off, force RX->OSC WiFi setup, improve LED diagnostics.
// Base: v14 led/menu fix for Heltec WiFi LoRa 32 V3.
// Goals: keep proven WiFi/OSC and BLE/HID, remove USB user mode,
// make LoRa TX/RX link state obvious on OLED and LED, avoid automatic
// long WiFi portals, and keep LoRa commands fire-and-forget for low latency.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Preferences.h>

#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>

// Heltec WiFi LoRa 32 V3 uses an on-board USB-to-UART chip.
// The stock USB-C port is reliable for Serial diagnostics, but not for HID keyboard.
// Keep this 0 for V3. For a native-USB board (e.g. V4 or another ESP32-S3 devkit),
// you may try setting it to 1.
#define ENABLE_USB_HID 0
#if ENABLE_USB_HID
  #include "USB.h"
  #include "USBHIDKeyboard.h"
#endif

#include <RadioLib.h>
#include <esp_sleep.h>
#include <stddef.h>
#include <math.h>

// ============================================================================
// Pins: Heltec WiFi LoRa 32 V3
// ============================================================================
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define VEXT_PIN 36

#define PIN_LORA_NSS   8
#define PIN_LORA_RST   12
#define PIN_LORA_BUSY  13
#define PIN_LORA_DIO1  14

#define BUTTON_PIN 0
#ifndef LED_BUILTIN
#define LED_BUILTIN 25
#endif
#define GO_LED_PIN LED_BUILTIN
#define GO_LED_ACTIVE_HIGH 1
#define PIN_BATTERY_CTRL 37
#define PIN_BATTERY_DATA 1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
Preferences prefs;
WiFiUDP udp;
SX1262 lora = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);

// ============================================================================
// Debug
// ============================================================================
#define DEBUG_SERIAL 0
#if DEBUG_SERIAL
  #define DBG(x) Serial.print(x)
  #define DBGLN(x) Serial.println(x)
#else
  #define DBG(x)
  #define DBGLN(x)
#endif

// ============================================================================
// Configs
// ============================================================================
struct OscConfig {
  char osc_ip[16];
  int osc_port;
  char osc_address[32];
  char panic_address[32];
} config;

struct RadioConfig {
  float freq;
  float bw;
  uint8_t sf;
  uint8_t cr;
  int8_t power;
  uint16_t counter;
} radioCfg;

enum OutputMode : uint8_t {
  OUT_BLE = 0,
  OUT_OSC = 1
};

uint8_t gatewayOutputMode = OUT_BLE;
uint8_t selectedOutputMode = OUT_BLE;

bool initSelectedMode(bool allowWifiPortal);

// ============================================================================
// Control modes
// ============================================================================
enum ControlMode : uint8_t {
  MODE_OSC_WIFI = 0,
  MODE_BLE_HID = 1,
  MODE_LORA_REMOTE = 2,
  MODE_LORA_GATEWAY = 3
};

ControlMode controlMode = MODE_OSC_WIFI;
ControlMode selectedMode = MODE_OSC_WIFI;
bool modeConfigured = false;

// ============================================================================
// Screens and menu
// ============================================================================
enum Screen : uint8_t {
  SCREEN_BOOT,
  SCREEN_MODE_SELECT,
  SCREEN_GO,
  SCREEN_STATUS,
  SCREEN_MENU,
  SCREEN_MODE_CONFIRM,
  SCREEN_MODE_INFO,
  SCREEN_OUTPUT_SELECT,
  SCREEN_PAIR_SELECT,
  SCREEN_LORA_SEARCH,
  SCREEN_LORA_LINK_OK,
  SCREEN_LORA_LINK_LOST,
  SCREEN_GO_SENT,
  SCREEN_PANIC_SENT,
  SCREEN_NO_CONNECTION
};

Screen currentScreen = SCREEN_BOOT;
Screen previousScreen = SCREEN_GO;
unsigned long screenChangedAt = 0;

void setScreen(uint8_t s) {
  currentScreen = (Screen)s;
  screenChangedAt = millis();
}

enum : uint8_t {
  MENU_MODE,
  MENU_SETUP,
  MENU_STATUS,
  MENU_LORA_FREQ,
  MENU_LORA_POWER,
  MENU_PAIR,
  MENU_OUTPUT,
  MENU_RESET,
  MENU_REBOOT,
  MENU_BACK
};

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
uint8_t currentMenuIndex = 0;
int menuScroll = 0;

// ============================================================================
// Button timing
// ============================================================================
bool buttonPressed = false;
bool longActionFired = false;
unsigned long pressStart = 0;
unsigned long lastScreenUpdate = 0;

unsigned long lastShortPressTime = 0;
int shortPressCount = 0;
const unsigned long SHORT_PRESS_TIMEOUT = 260;       // enough time for 3 fast clicks; GO still feels quick
const uint8_t MENU_CLICK_COUNT = 3;                 // fast 3-5 clicks opens menu
const unsigned long PANIC_HOLD_MS = 1400;           // hold on GO screen = PANIC
const unsigned long HOLD_SELECT_MS = 1000;

unsigned long messageTime = 0;
const unsigned long MESSAGE_DURATION = 700;

// ============================================================================
// Battery / LED
// ============================================================================
bool batteryPresent = false;
int batteryPercent = 0;
unsigned long lastBatteryRead = 0;
const unsigned long BATTERY_READ_INTERVAL = 10000;

unsigned long lastLedToggle = 0;
bool ledState = false;
enum LedMode { LED_OFF, LED_FAST_BLINK, LED_SLOW_BLINK, LED_SOLID };
LedMode ledMode = LED_OFF;
LedMode lastAppliedLedMode = LED_OFF;
unsigned long lastLedModeUpdate = 0;

// ============================================================================
// BLE HID
// ============================================================================
NimBLEHIDDevice *hid = nullptr;
NimBLECharacteristic *inputKeyboard = nullptr;
NimBLEServer *bleServer = nullptr;
bool bleStarted = false;
bool bleConnected = false;
volatile bool bleConnectionChanged = false;

#if ENABLE_USB_HID
USBHIDKeyboard usbKeyboard;
bool usbHidStarted = false;
#endif

// GO = SPACE, PANIC = ESC
static const uint8_t keyboardReportMap[] = {
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07,
  0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
  0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01,
  0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
  0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06,
  0x75, 0x08, 0x15, 0x00, 0x25, 0xFF, 0x05, 0x07,
  0x19, 0x00, 0x29, 0xFF, 0x81, 0x00, 0xC0
};

class BleHIDCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    bleConnected = true;
    bleConnectionChanged = true;
    DBGLN("[BLE] connected");
  }

  void onDisconnect(NimBLEServer* pServer) {
    bleConnected = false;
    bleConnectionChanged = true;
    DBGLN("[BLE] disconnected");
    NimBLEDevice::startAdvertising();
  }
};

// ============================================================================
// LoRa packets
// ============================================================================
enum PacketType : uint8_t {
  PKT_PING  = 0x01,
  PKT_PONG  = 0x02,
  PKT_GO    = 0x03,
  PKT_PANIC = 0x04,
  PKT_ACK   = 0x05,
  PKT_BEACON = 0x06
};

#pragma pack(push, 1)
struct LoRaPacket {
  uint8_t  version;
  uint8_t  type;
  uint32_t senderId;
  uint32_t targetId;
  uint16_t counter;
  uint8_t  ackCommand;
  uint16_t crc;
};
#pragma pack(pop)

volatile bool loraEventFlag = false;
bool loraInitialized = false;
bool loraInitFailed = false;
uint32_t deviceId = 0;
LoRaPacket lastPacket = {};

bool linkOK = false;
unsigned long lastLinkSeen = 0;
unsigned long lastPingTime = 0;
unsigned long lastBeaconTime = 0;
const unsigned long PING_INTERVAL = 2000;
const unsigned long BEACON_INTERVAL = 2000;
const unsigned long LINK_TIMEOUT = 2800;

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
const unsigned long ACK_TIMEOUT = 700;
const int MAX_RETRIES = 3;

bool gatewayWifiReady = false;

struct PeerState {
  uint32_t senderId;
  uint16_t lastCounter;
  bool used;
};
PeerState peerTable[8];

struct DiscoveredPeer {
  uint32_t id;
  int16_t rssi;
  float snr;
  unsigned long lastSeen;
  bool used;
};

DiscoveredPeer discoveredPeers[6];
uint8_t selectedPeerIndex = 0;
uint32_t pairedRxId = 0;   // 0 = ANY RX on this channel

void IRAM_ATTR loraSetFlag() {
  loraEventFlag = true;
}

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

// ============================================================================
// Helpers
// ============================================================================
void safeCopy(char* dst, const char* src, size_t dstSize) {
  if (!dst || dstSize == 0) return;
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

void VextON() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
}

uint32_t hardwareId32() {
  uint64_t mac = ESP.getEfuseMac();
  return (uint32_t)((mac & 0xFFFFFFFFULL) ^ ((mac >> 24) & 0xFFFFFFFFULL));
}

String shortIdString(uint32_t id) {
  char buf[10];
  snprintf(buf, sizeof(buf), "%06lX", (unsigned long)(id & 0xFFFFFF));
  return String(buf);
}

String getUniqueName() {
  return "GO-GO-" + shortIdString(hardwareId32());
}

// Forward declarations used by lightweight UI/link helpers.
bool isLoRaMode();
bool isLoRaRemote();
bool isLoRaGateway();

String peerTargetString() {
  if (pairedRxId == 0) return String("ANY");
  return shortIdString(pairedRxId);
}

int discoveredPeerCount() {
  int n = 0;
  unsigned long now = millis();
  for (auto &p : discoveredPeers) {
    if (p.used && (now - p.lastSeen < 10000)) n++;
  }
  return n;
}

int discoveredIndexByVisibleOrder(int visibleIndex) {
  int n = 0;
  unsigned long now = millis();
  for (int i = 0; i < 6; i++) {
    if (discoveredPeers[i].used && (now - discoveredPeers[i].lastSeen < 10000)) {
      if (n == visibleIndex) return i;
      n++;
    }
  }
  return -1;
}

void rememberDiscoveredPeer(uint32_t id, int16_t rssi, float snr) {
  if (id == 0 || id == deviceId) return;
  unsigned long now = millis();

  for (auto &p : discoveredPeers) {
    if (p.used && p.id == id) {
      p.rssi = rssi;
      p.snr = snr;
      p.lastSeen = now;
      return;
    }
  }

  for (auto &p : discoveredPeers) {
    if (!p.used) {
      p.used = true;
      p.id = id;
      p.rssi = rssi;
      p.snr = snr;
      p.lastSeen = now;
      return;
    }
  }

  // Replace the oldest entry if the list is full.
  int oldest = 0;
  for (int i = 1; i < 6; i++) {
    if (discoveredPeers[i].lastSeen < discoveredPeers[oldest].lastSeen) oldest = i;
  }
  discoveredPeers[oldest] = {id, rssi, snr, now, true};
}

void savePairedRxId(uint32_t id) {
  prefs.begin("radio", false);
  prefs.putUInt("pair_rx", id);
  prefs.end();
  pairedRxId = id;
}

void loadPairedRxId() {
  prefs.begin("radio", true);
  pairedRxId = prefs.getUInt("pair_rx", 0);
  prefs.end();
}

bool loraPeerFound() {
  if (!isLoRaMode() || !loraInitialized) return false;
  if (lastPeerId == 0) return false;
  if (millis() - lastLinkSeen > LINK_TIMEOUT) return false;
  if (isLoRaRemote() && pairedRxId != 0 && lastPeerId != pairedRxId) return false;
  return true;
}


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

bool outputWantsOsc() {
  return gatewayOutputMode == OUT_OSC;
}

bool outputWantsBle() {
  return gatewayOutputMode == OUT_BLE;
}

bool bleOutputActive() {
  return controlMode == MODE_BLE_HID || (controlMode == MODE_LORA_GATEWAY && outputWantsBle());
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

bool isValidIP(const char* ip) {
  int parts[4];
  int count = sscanf(ip, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]);
  if (count != 4) return false;
  for (int i = 0; i < 4; i++) {
    if (parts[i] < 0 || parts[i] > 255) return false;
  }
  return true;
}

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else crc <<= 1;
    }
  }
  return crc;
}

void ledWrite(bool on) {
#if GO_LED_ACTIVE_HIGH
  digitalWrite(GO_LED_PIN, on ? HIGH : LOW);
#else
  digitalWrite(GO_LED_PIN, on ? LOW : HIGH);
#endif
}

void blinkLedOnce() {
  ledWrite(true);
  delay(45);
  ledWrite(false);
}

void ledSelfTest() {
  // Long visible test. If the standalone Arduino Blink sketch works, this must blink too.
  // If it does not, change GO_LED_PIN above to the exact LED pin for your board profile.
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(16, 18);
  display.print("LED TEST");
  display.setTextSize(1);
  display.setCursor(29, 42);
  display.print("watch diode");
  display.display();
  for (int i = 0; i < 4; i++) {
    ledWrite(true);
    delay(180);
    ledWrite(false);
    delay(180);
  }
}

void showSimpleMessage(const char* line1, const char* line2 = nullptr, unsigned long waitMs = 1000) {
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

void drawCenteredText(const char* text, int y, int size, uint16_t color = SSD1306_WHITE) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.setTextColor(color);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

// ============================================================================
// Preferences
// ============================================================================
void loadOscConfig() {
  prefs.begin("osc", true);
  String ip = prefs.getString("ip", "192.168.1.100");
  String addr = prefs.getString("addr", "/go");
  String panic = prefs.getString("panic", "/panic");
  int port = prefs.getInt("port", 53000);
  prefs.end();

  safeCopy(config.osc_ip, ip.c_str(), sizeof(config.osc_ip));
  safeCopy(config.osc_address, addr.c_str(), sizeof(config.osc_address));
  safeCopy(config.panic_address, panic.c_str(), sizeof(config.panic_address));
  config.osc_port = port;
}

void saveOscConfig() {
  prefs.begin("osc", false);
  prefs.putString("ip", config.osc_ip);
  prefs.putString("addr", config.osc_address);
  prefs.putString("panic", config.panic_address);
  prefs.putInt("port", config.osc_port);
  prefs.end();
}

bool loadControlMode() {
  prefs.begin("system", true);
  bool saved = prefs.getBool("mode_set", false);
  int value = prefs.getInt("mode", MODE_OSC_WIFI);
  gatewayOutputMode = prefs.getUChar("out", OUT_BLE);
  prefs.end();

  if (!saved) {
    modeConfigured = false;
    controlMode = MODE_OSC_WIFI;
    selectedMode = MODE_OSC_WIFI;
    return false;
  }

  if (value < MODE_OSC_WIFI || value > MODE_LORA_GATEWAY) value = MODE_OSC_WIFI;
  controlMode = (ControlMode)value;
  selectedMode = controlMode;
  modeConfigured = true;

  if (gatewayOutputMode > OUT_OSC) gatewayOutputMode = OUT_BLE;
  return true;
}

void saveControlMode(uint8_t mode) {
  prefs.begin("system", false);
  prefs.putBool("mode_set", true);
  prefs.putInt("mode", (int)mode);
  prefs.end();
}

void saveGatewayOutputMode() {
  prefs.begin("system", false);
  prefs.putUChar("out", gatewayOutputMode);
  prefs.end();
}


void saveControlModeAndOutput(uint8_t mode, uint8_t out) {
  prefs.begin("system", false);
  prefs.putBool("mode_set", true);
  prefs.putInt("mode", (int)mode);
  prefs.putUChar("out", out);
  prefs.end();
}

void loadRadioConfig() {
  prefs.begin("radio", true);
  radioCfg.freq = prefs.getFloat("freq", 868.0f);
  radioCfg.bw = prefs.getFloat("bw", 125.0f);
  radioCfg.sf = prefs.getUChar("sf", 7);
  radioCfg.cr = prefs.getUChar("cr", 5);
  radioCfg.power = prefs.getChar("pwr", 14);
  radioCfg.counter = prefs.getUShort("cnt", 0);
  prefs.end();
}

void saveRadioConfig() {
  prefs.begin("radio", false);
  prefs.putFloat("freq", radioCfg.freq);
  prefs.putFloat("bw", radioCfg.bw);
  prefs.putUChar("sf", radioCfg.sf);
  prefs.putUChar("cr", radioCfg.cr);
  prefs.putChar("pwr", radioCfg.power);
  prefs.putUShort("cnt", radioCfg.counter);
  prefs.end();
}

// ============================================================================
// Battery
// ============================================================================
void updateBattery() {
  digitalWrite(PIN_BATTERY_CTRL, HIGH);
  delay(10);
  int raw = analogRead(PIN_BATTERY_DATA);
  float voltage = (raw * 490.0f / 100.0f) / 1000.0f;
  digitalWrite(PIN_BATTERY_CTRL, LOW);

  if (voltage > 3.0f) {
    batteryPresent = true;
    batteryPercent = constrain((int)((voltage - 3.5f) / (4.2f - 3.5f) * 100.0f), 0, 100);
  } else {
    batteryPresent = false;
    batteryPercent = 0;
  }
}

// ============================================================================
// WiFi / OSC
// ============================================================================
bool configureWiFi(bool rebootAfterSave = false) {
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

bool connectWiFiWithDisplay(bool allowPortal = true) {
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

// ============================================================================
// BLE / HID
// ============================================================================
void startBLEMode() {
  if (bleStarted) return;

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  String deviceName = getUniqueName();
  DBGLN("[BLE] start");

  NimBLEDevice::init(deviceName.c_str());
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new BleHIDCallbacks());

  hid = new NimBLEHIDDevice(bleServer);
  inputKeyboard = hid->getInputReport(0);
  hid->setReportMap((uint8_t*)keyboardReportMap, sizeof(keyboardReportMap));
  hid->setManufacturer("soundkorb");
  hid->setPnp(0x02, 0x1234, 0x5678, 0x0001);
  hid->setHidInfo(0x00, 0x01);
  hid->startServices();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(hid->getHidService()->getUUID());
  pAdvertising->setAppearance(0x03C1);
  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x40);
  pAdvertising->start();

  bleStarted = true;
  bleConnected = false;
}

void stopBLEMode() {
  if (!bleStarted) return;
  if (bleServer) bleServer->stopAdvertising();
  NimBLEDevice::deinit();
  bleStarted = false;
  bleConnected = false;
  hid = nullptr;
  inputKeyboard = nullptr;
  bleServer = nullptr;
}

void sendKeyPress(uint8_t keyCode) {
  if (!bleConnected || inputKeyboard == nullptr) return;

  uint8_t report[8] = {0};
  report[2] = keyCode;
  inputKeyboard->setValue(report, 8);
  inputKeyboard->notify();
  delay(20);

  report[2] = 0;
  inputKeyboard->setValue(report, 8);
  inputKeyboard->notify();
  delay(10);
}

void startUsbHidMode() {
#if ENABLE_USB_HID
  if (usbHidStarted) return;
  USB.begin();
  usbKeyboard.begin();
  usbHidStarted = true;
#endif
}

void sendUsbKeyPress(uint8_t hidCode) {
#if ENABLE_USB_HID
  // Native USB HID boards only. Heltec V3 stock USB is CP210x Serial, not HID.
  if (hidCode == 0x2C) {
    usbKeyboard.press(' ');
  } else if (hidCode == 0x29) {
    usbKeyboard.press(0xB1);
  }
  delay(25);
  usbKeyboard.releaseOne();
#else
  // Reliable fallback for Heltec V3: USB serial text for diagnostics / bridge.
  Serial.println((hidCode == 0x2C) ? "GO" : "PANIC");
  Serial.flush();
#endif
}

// ============================================================================
// LoRa
// ============================================================================
bool isLoRaMode() {
  return controlMode == MODE_LORA_REMOTE || controlMode == MODE_LORA_GATEWAY;
}

bool isLoRaRemote() {
  return controlMode == MODE_LORA_REMOTE;
}

bool isLoRaGateway() {
  return controlMode == MODE_LORA_GATEWAY;
}

bool initLoRa() {
  if (loraInitialized) return true;

  DBGLN("[LORA] begin");
  int state = lora.begin(radioCfg.freq, radioCfg.bw, radioCfg.sf, radioCfg.cr);
  if (state != RADIOLIB_ERR_NONE) {
    DBG("[LORA] begin failed: ");
    DBGLN(state);
    loraInitialized = false;
    loraInitFailed = true;
    return false;
  }

  lora.setOutputPower(radioCfg.power);
  lora.setDio1Action(loraSetFlag);
  state = lora.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    DBG("[LORA] RX failed: ");
    DBGLN(state);
    loraInitialized = false;
    loraInitFailed = true;
    return false;
  }

  loraInitialized = true;
  loraInitFailed = false;
  lastLinkSeen = millis();
  return true;
}

void stopLoRa() {
  if (!loraInitialized) return;
  lora.sleep();
  loraInitialized = false;
  loraInitFailed = false;
}

void restartLoRa() {
  stopLoRa();
  delay(50);
  initLoRa();
}

bool canSendNow() {
  return (millis() - lastTxTimestamp) >= 100;
}

bool sendPacket(uint8_t type, uint32_t targetId, uint8_t ackCmd = 0) {
  if (!loraInitialized) return false;
  if (!canSendNow()) return false;
  lastTxTimestamp = millis();

  LoRaPacket p{};
  p.version = 1;
  p.type = type;
  p.senderId = deviceId;
  p.targetId = targetId;
  p.counter = radioCfg.counter++;
  p.ackCommand = ackCmd;
  p.crc = crc16_ccitt((const uint8_t*)&p, offsetof(LoRaPacket, crc));

  int state = lora.transmit((uint8_t*)&p, sizeof(p));
  lora.startReceive();
  saveRadioConfig();

  if (state != RADIOLIB_ERR_NONE) {
    DBG("[LORA] TX failed: ");
    DBGLN(state);
    return false;
  }

  txPacketCount++;
  return true;
}

bool acceptNewPacket(uint32_t sender, uint16_t counter) {
  for (auto &peer : peerTable) {
    if (peer.used && peer.senderId == sender) {
      if (counter <= peer.lastCounter) return false;
      peer.lastCounter = counter;
      return true;
    }
  }
  for (auto &peer : peerTable) {
    if (!peer.used) {
      peer.used = true;
      peer.senderId = sender;
      peer.lastCounter = counter;
      return true;
    }
  }
  return true;
}

void emitGatewayAction(uint8_t type, uint32_t fromSender) {
  lastLinkSeen = millis();
  linkOK = true;

  const char* cmd = (type == PKT_GO) ? "GO" : "PANIC";

  if (DEBUG_SERIAL) {
    DBG("[RX LORA] ");
    DBG(cmd);
    DBG(" from ");
    DBGLN(shortIdString(fromSender));
  }


  if (outputWantsOsc() && WiFi.status() == WL_CONNECTED) {
    sendOSC((type == PKT_GO) ? config.osc_address : config.panic_address);
  }

  if (outputWantsBle() && bleConnected) {
    sendKeyPress((type == PKT_GO) ? 0x2C : 0x29);
  }

  blinkLedOnce();
}

void processLoRaPacket() {
  if (!loraEventFlag || !loraInitialized) return;
  loraEventFlag = false;

  LoRaPacket p{};
  int state = lora.readData((uint8_t*)&p, sizeof(p));
  if (state != RADIOLIB_ERR_NONE) return;

  uint16_t calc = crc16_ccitt((const uint8_t*)&p, offsetof(LoRaPacket, crc));
  if (calc != p.crc || p.version != 1) return;
  if (p.targetId != 0xFFFFFFFF && p.targetId != deviceId) return;
  if (!acceptNewPacket(p.senderId, p.counter)) return;

  lastPacket = p;
  lastPeerId = p.senderId;
  lastPeerRssi = (int16_t)lora.getRSSI();
  lastPeerSnr = lora.getSNR();
  rememberDiscoveredPeer(p.senderId, lastPeerRssi, lastPeerSnr);
  lastLinkSeen = millis();
  linkOK = true;
  rxPacketCount++;

  if (isLoRaGateway()) {
    if (p.type == PKT_PING) {
      sendPacket(PKT_PONG, p.senderId, 0);
    } else if (p.type == PKT_GO || p.type == PKT_PANIC) {
      lastRxCommand = p.type;
      lastCommandSeen = millis();
      emitGatewayAction(p.type, p.senderId);
      setScreen((p.type == PKT_GO) ? SCREEN_GO_SENT : SCREEN_PANIC_SENT);
      messageTime = millis();
    }
  } else if (isLoRaRemote()) {
    if (p.type == PKT_PONG || p.type == PKT_BEACON) {
      if (pairedRxId == 0 || pairedRxId == p.senderId) {
        linkOK = true;
        lastLinkSeen = millis();
      }
    } else if (p.type == PKT_ACK && waitingForAck && p.targetId == deviceId && p.ackCommand == pendingCommand) {
      waitingForAck = false;
      retryCount = 0;
      ackOk = true;
      messageTime = millis();
      setScreen((pendingCommand == PKT_GO) ? SCREEN_GO_SENT : SCREEN_PANIC_SENT);
    }
  }
}

void sendHeartbeat() {
  if (!isLoRaRemote() || !loraInitialized) return;
  unsigned long now = millis();

  if (now - lastPingTime >= PING_INTERVAL) {
    lastPingTime = now;
    sendPacket(PKT_PING, pairedRxId ? pairedRxId : 0xFFFFFFFF, 0);
  }

  if (lastLinkSeen == 0 || now - lastLinkSeen > LINK_TIMEOUT) {
    linkOK = false;
  }
}

void sendGatewayBeacon() {
  if (!isLoRaGateway() || !loraInitialized) return;
  unsigned long now = millis();
  if (now - lastBeaconTime >= BEACON_INTERVAL) {
    lastBeaconTime = now;
    sendPacket(PKT_BEACON, 0xFFFFFFFF, 0);
  }
  if (lastLinkSeen == 0 || now - lastLinkSeen > LINK_TIMEOUT) {
    linkOK = false;
  }
}

void retryPendingAck() {
  if (!waitingForAck || !isLoRaRemote()) return;

  unsigned long now = millis();
  if (now - lastTxTime < ACK_TIMEOUT) return;

  if (retryCount < MAX_RETRIES - 1) {
    retryCount++;
    lastTxTime = now;
    sendPacket(pendingCommand, pendingTarget, 0);
  } else {
    waitingForAck = false;
    ackOk = false;
    linkOK = false;
    setScreen(SCREEN_NO_CONNECTION);
    messageTime = now;
  }
}

void cycleFrequency() {
  static const float freqs[] = {868.0f, 868.1f, 868.3f, 868.5f, 869.0f};
  const int n = sizeof(freqs) / sizeof(freqs[0]);
  int idx = 0;
  for (int i = 0; i < n; i++) {
    if (fabsf(radioCfg.freq - freqs[i]) < 0.01f) {
      idx = i;
      break;
    }
  }
  idx = (idx + 1) % n;
  radioCfg.freq = freqs[idx];
  saveRadioConfig();
  restartLoRa();
}

void cyclePower() {
  static const int8_t powers[] = {10, 14, 17, 20};
  const int n = sizeof(powers) / sizeof(powers[0]);
  int idx = 0;
  for (int i = 0; i < n; i++) {
    if (powers[i] == radioCfg.power) {
      idx = i;
      break;
    }
  }
  idx = (idx + 1) % n;
  radioCfg.power = powers[idx];
  saveRadioConfig();
  restartLoRa();
}

void cycleOutput() {
  gatewayOutputMode = (gatewayOutputMode + 1) % 2;
  saveGatewayOutputMode();
  if (controlMode == MODE_LORA_GATEWAY) {
    // Re-init so exactly one RX output is active: BLE HID OR OSC WiFi.
    initSelectedMode(false);
  }
}

// ============================================================================
// Mode init / reset
// ============================================================================
bool initSelectedMode(bool allowWifiPortal = true) {
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

bool isOutputReadyForLed() {
  if (!modeConfigured) return false;

  if (controlMode == MODE_OSC_WIFI) return WiFi.status() == WL_CONNECTED;
  if (controlMode == MODE_BLE_HID) return bleConnected;
  if (controlMode == MODE_LORA_REMOTE) {
    return loraInitialized && loraPeerFound();
  }

  if (controlMode == MODE_LORA_GATEWAY) {
    if (!loraInitialized || !loraPeerFound()) return false;
    if (outputWantsOsc()) return WiFi.status() == WL_CONNECTED;
    if (outputWantsBle()) return bleConnected;
    return false;
  }

  return false;
}

bool isSearchingForLed() {
  if (!modeConfigured) return false;
  if (currentScreen == SCREEN_BOOT || currentScreen == SCREEN_MODE_SELECT || currentScreen == SCREEN_MODE_CONFIRM) return false;
  // Pair screen should actively blink while searching/listing RX units.
  if (currentScreen == SCREEN_PAIR_SELECT) return true;
  return !isOutputReadyForLed();
}

bool hasRadioLinkButOutputMissing() {
  if (controlMode != MODE_LORA_GATEWAY) return false;
  if (!loraInitialized || !loraPeerFound()) return false;
  if (outputWantsBle()) return !bleConnected;
  if (outputWantsOsc()) return WiFi.status() != WL_CONNECTED;
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
    ackOk = sendPacket(PKT_GO, pairedRxId ? pairedRxId : 0xFFFFFFFF, 0);
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
    ackOk = sendPacket(PKT_PANIC, pairedRxId ? pairedRxId : 0xFFFFFFFF, 0);
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

    case MENU_REBOOT:
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

void updateLedMode() {
  if (!modeConfigured || currentScreen == SCREEN_BOOT || currentScreen == SCREEN_MODE_SELECT || currentScreen == SCREEN_MODE_CONFIRM) {
    ledMode = LED_OFF;
    return;
  }

  if (isLoRaRemote()) {
    ledMode = loraPeerFound() ? LED_SOLID : LED_FAST_BLINK;
    return;
  }

  if (isLoRaGateway()) {
    if (!loraPeerFound()) {
      ledMode = LED_FAST_BLINK;
      return;
    }
    if ((outputWantsBle() && bleConnected) || (outputWantsOsc() && WiFi.status() == WL_CONNECTED)) ledMode = LED_SOLID;
    else ledMode = LED_SLOW_BLINK;
    return;
  }

  ledMode = isConnectionReady() ? LED_SOLID : LED_FAST_BLINK;
}

void updateLed() {
  unsigned long now = millis();

  if (ledMode != lastAppliedLedMode) {
    lastAppliedLedMode = ledMode;
    lastLedToggle = now;
    // Start blinking from ON so the operator sees the change immediately.
    if (ledMode == LED_FAST_BLINK || ledMode == LED_SLOW_BLINK) {
      ledState = true;
      ledWrite(true);
    }
  }

  switch (ledMode) {
    case LED_OFF:
      ledState = false;
      ledWrite(false);
      break;
    case LED_SOLID:
      ledState = true;
      ledWrite(true);
      break;
    case LED_FAST_BLINK:
      if (now - lastLedToggle > 180) {
        lastLedToggle = now;
        ledState = !ledState;
        ledWrite(ledState);
      }
      break;
    case LED_SLOW_BLINK:
      if (now - lastLedToggle > 520) {
        lastLedToggle = now;
        ledState = !ledState;
        ledWrite(ledState);
      }
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
