// Preferences (NVS) load/save.
#pragma once
#include "config.h"

void loadOscConfig();
void saveOscConfig();
bool loadControlMode();
void saveControlMode(uint8_t mode);
void saveGatewayOutputMode();
void saveControlModeAndOutput(uint8_t mode, uint8_t out);
void loadRadioConfig();
void saveRadioConfig();
void savePairedRxId(uint32_t id);
void loadPairedRxId();
void saveKeymap();
void saveWifiCreds();
