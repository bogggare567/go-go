// GO-GO — battery module (split 1:1 from v15 monolith).
#include "gogo.h"

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

// ============================================================================
// Battery
// ============================================================================
void updateBattery() {
  digitalWrite(PIN_BATTERY_CTRL, HIGH);
  delay(10);
  // Calibrated ADC reading; the board divider is ~390k/100k (factor 4.9).
  uint32_t mv = analogReadMilliVolts(PIN_BATTERY_DATA);
  float voltage = (mv * 4.9f) / 1000.0f;
  digitalWrite(PIN_BATTERY_CTRL, LOW);

  if (voltage > 3.0f) {
    batteryPresent = true;
    batteryPercent = constrain((int)((voltage - 3.5f) / (4.2f - 3.5f) * 100.0f), 0, 100);
  } else {
    batteryPresent = false;
    batteryPercent = 0;
  }
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
