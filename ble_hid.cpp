// GO-GO — ble_hid module (split 1:1 from v15 monolith).
#include "gogo.h"

#if ENABLE_USB_HID
  #include "USB.h"
  #include "USBHIDKeyboard.h"
#endif

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
