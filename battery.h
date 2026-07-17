// Battery monitoring and status LED.
#pragma once
#include "config.h"

void updateBattery();
void ledWrite(bool on);
void blinkLedOnce();
bool isOutputReadyForLed();
bool isSearchingForLed();
bool hasRadioLinkButOutputMissing();
void updateLedMode();
void updateLed();
void ledPlayMorseGo();   // identity blink when the slave finds its master
