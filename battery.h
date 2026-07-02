// Battery monitoring and status LED.
#pragma once
#include "config.h"

void updateBattery();
void ledWrite(bool on);
void blinkLedOnce();
void ledSelfTest();
bool isOutputReadyForLed();
bool isSearchingForLed();
bool hasRadioLinkButOutputMissing();
void updateLedMode();
void updateLed();
