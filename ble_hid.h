// BLE HID keyboard output (GO = SPACE, PANIC = ESC).
#pragma once
#include "config.h"

void startBLEMode();
void stopBLEMode();
void sendKeyPress(uint8_t keyCode);
void startUsbHidMode();
void sendUsbKeyPress(uint8_t hidCode);

// Remappable GO/PANIC keys
const char* keyName(uint8_t code);
void cycleGoKey();
void cyclePanicKey();
