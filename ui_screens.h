// OLED UI: screens, indicators, menu drawing, boot animation.
#pragma once
#include "config.h"

void setScreen(uint8_t s);
void showSimpleMessage(const char* line1, const char* line2 = nullptr, unsigned long waitMs = 1000);
void drawCenteredText(const char* text, int y, int size, uint16_t color = SSD1306_WHITE);

const char* modeLabel(uint8_t mode);
const char* shortModeLabel(uint8_t mode);
const char* modeHint(uint8_t mode);
uint8_t nextMode(uint8_t mode);
const char* outputLabel(uint8_t m);
const char* outputFullLabel(uint8_t m);
const char* outputHint(uint8_t m);
uint8_t nextOutput(uint8_t out);

void drawPowerIcon(int x, int y);
void drawWiFiIndicator(int x, int y);
void drawBLEIndicator(int x, int y);
void drawLoRaIndicator(int x, int y);
void drawUSBIndicator(int x, int y);
void drawConnectionIndicator(int x, int y);

void drawModeSelect();
void drawOutputSelect();
void drawPairSelect();
void drawGoScreen();
void drawGoSent();
void drawPanicSent();
void drawLoraSearch();
void drawLoraLinkOK();
void drawLoraLinkLost();
void drawNoConnection();
void drawStatusScreen();

const uint8_t* activeMenuItems();
int activeMenuCount();
uint8_t activeMenuItem();
void printMenuLabel(uint8_t item);
void drawMenu();
void drawModeInfo();
void drawRegionSelect();
void drawSpectrum();
void drawWebSetup();
void drawHoldProgress(unsigned long maxMs = HOLD_SELECT_MS);

void startBootAnimation();
void updateBootAnimation();
void updateLoRaLinkScreen();
