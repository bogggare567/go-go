// Mode lifecycle: init/switch/reset, GO/PANIC actions, menu actions.
#pragma once
#include "config.h"

bool outputWantsOsc();
bool outputWantsBle();
bool bleOutputActive();

bool initSelectedMode(bool allowWifiPortal = true);
void factoryReset();
void startModeNow(uint8_t mode);
void startRxNow(uint8_t out);
void restartIntoMode(uint8_t mode);
void restartIntoRxOutput(uint8_t out);
void afterBoot();

// Hold actions fired from loop() while the button is still held
void handleModeSelectHold();
void handleOutputSelectHold();
void handlePairSelectHold();
void confirmRegionSelection();

bool isConnectionReady();
void performGO();
void performPanic();
void powerOffDevice();
void handleMenuSelect();
void retryConnection();
