// GO-GO — shared types, pins, constants and global state declarations.
#define GOGO_VERSION "v16.15"
// Split 1:1 from the v15 monolith; behavior must stay identical.
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Preferences.h>

#include <WiFi.h>
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
};

struct RadioConfig {
  float freq;        // computed: region channel + fine-tune offset
  float bw;
  uint8_t sf;
  uint8_t cr;
  int8_t power;
  uint16_t counter;
  uint8_t region;    // index into the region plan table (radio.cpp)
  uint8_t chan;      // channel index inside the region plan
  int16_t tuneKhz;   // fine-tune offset, ±100 kHz in 25 kHz steps
};

enum OutputMode : uint8_t {
  OUT_BLE = 0,
  OUT_OSC = 1
};

// ============================================================================
// Control modes
// ============================================================================
enum ControlMode : uint8_t {
  MODE_OSC_WIFI = 0,
  MODE_BLE_HID = 1,
  MODE_LORA_REMOTE = 2,
  MODE_LORA_GATEWAY = 3
};

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
  SCREEN_NO_CONNECTION,
  SCREEN_REGION_SELECT,
  SCREEN_SPECTRUM,
  SCREEN_WEB_SETUP
};

enum : uint8_t {
  MENU_MODE,
  MENU_SETUP,
  MENU_STATUS,
  MENU_LORA_FREQ,
  MENU_LORA_POWER,
  MENU_REGION,
  MENU_TUNE,
  MENU_GO_KEY,
  MENU_PANIC_KEY,
  MENU_SPECTRUM,
  MENU_WEB,
  MENU_PAIR,
  MENU_OUTPUT,
  MENU_RESET,
  MENU_POWER_OFF,
  MENU_BACK
};

// ============================================================================
// Timing constants
// ============================================================================
const unsigned long SHORT_PRESS_TIMEOUT = 260;       // enough time for 3 fast clicks; GO still feels quick
const uint8_t MENU_CLICK_COUNT = 3;                 // fast 3-5 clicks opens menu
const unsigned long PANIC_HOLD_MS = 1400;           // hold on GO screen = PANIC
const unsigned long HOLD_SELECT_MS = 1000;

const unsigned long MESSAGE_DURATION = 700;

const unsigned long BATTERY_READ_INTERVAL = 10000;

const unsigned long PING_INTERVAL = 2000;
const unsigned long BEACON_INTERVAL = 2000;
const unsigned long LINK_TIMEOUT = 5200;   // 2 missed pings + margin; single loss must not drop the link

const unsigned long ACK_TIMEOUT = 700;
const int MAX_RETRIES = 3;

// ============================================================================
// Battery / LED
// ============================================================================
enum LedMode { LED_OFF, LED_FAST_BLINK, LED_SLOW_BLINK, LED_SOLID };

// ============================================================================
// LoRa packets
// ============================================================================
enum PacketType : uint8_t {
  PKT_PING  = 0x01,
  PKT_PONG  = 0x02,
  PKT_GO    = 0x03,
  PKT_PANIC = 0x04,
  PKT_ACK   = 0x05,
  PKT_BEACON = 0x06,
  PKT_HOP   = 0x07   // master announces a channel change; ackCommand = grid index
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

struct DiscoveredPeer {
  uint32_t id;
  int16_t rssi;
  float snr;
  unsigned long lastSeen;
  bool used;
};

// ============================================================================
// Global objects and state (defined in LoRa_soundkorb.ino)
// ============================================================================
extern Adafruit_SSD1306 display;
extern Preferences prefs;
extern WiFiUDP udp;
extern SX1262 lora;

extern OscConfig config;
extern RadioConfig radioCfg;
extern char wifiSsid[33];      // venue network; set via the web panel
extern char wifiPass[65];
extern char webPin[9];         // panel PIN (HTTP auth, user "gogo"), default 0000

extern uint8_t gatewayOutputMode;
extern uint8_t selectedOutputMode;

extern ControlMode controlMode;
extern ControlMode selectedMode;
extern bool modeConfigured;

extern Screen currentScreen;
extern Screen previousScreen;
extern unsigned long screenChangedAt;

extern uint8_t currentMenuIndex;
extern int menuScroll;

extern bool buttonPressed;
extern bool longActionFired;
extern unsigned long pressStart;
extern unsigned long lastScreenUpdate;
extern unsigned long lastShortPressTime;
extern int shortPressCount;
extern unsigned long messageTime;

extern bool batteryPresent;
extern int batteryPercent;
extern unsigned long lastBatteryRead;

extern unsigned long lastLedToggle;
extern bool ledState;
extern LedMode ledMode;
extern LedMode lastAppliedLedMode;
extern unsigned long lastLedModeUpdate;

extern NimBLEHIDDevice *hid;
extern NimBLECharacteristic *inputKeyboard;
extern NimBLEServer *bleServer;
extern bool bleStarted;
extern bool bleConnected;
extern volatile bool bleConnectionChanged;
extern uint8_t goKeyCode;      // HID usage code typed on GO (default Space)
extern uint8_t panicKeyCode;   // HID usage code typed on PANIC (default Esc)

extern volatile bool loraEventFlag;
extern bool loraInitialized;
extern bool loraInitFailed;
extern uint32_t deviceId;
extern LoRaPacket lastPacket;

extern bool linkOK;
extern unsigned long lastLinkSeen;
extern unsigned long lastPingTime;
extern unsigned long lastBeaconTime;

extern uint32_t lastPeerId;
extern int16_t lastPeerRssi;
extern float lastPeerSnr;
extern uint32_t rxPacketCount;
extern uint32_t txPacketCount;
extern unsigned long lastTxTimestamp;
extern uint8_t lastRxCommand;
extern unsigned long lastCommandSeen;

extern bool waitingForAck;
extern bool ackOk;
extern uint8_t pendingCommand;
extern uint32_t pendingTarget;
extern int retryCount;
extern unsigned long lastTxTime;

extern bool gatewayWifiReady;

extern DiscoveredPeer discoveredPeers[6];
extern uint8_t selectedPeerIndex;
extern uint32_t pairedRxId;

extern bool regionConfigured;      // region was explicitly chosen once
extern uint8_t selectedRegion;     // cursor on the region select screen
extern bool freqAuto;              // Auto: RX picks the cleanest channel, TX follows
extern uint8_t spectrumLevels[64]; // live RSSI bars for the spectrum screen
extern uint8_t spectrumPos;
