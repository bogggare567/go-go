// WiFi (WiFiManager portal) and OSC output.
#pragma once
#include "config.h"

bool configureWiFi(bool rebootAfterSave = false);
bool connectWiFiWithDisplay(bool allowPortal = true);
void sendOSC(const char* addr);
