// Web configuration UI: JSON API + embedded page + OTA.
// Runs permanently when WiFi STA is connected (OSC modes); in other modes
// the menu item "Web Setup" raises a temporary access point.
#pragma once
#include "config.h"

void webLoop();            // call from loop(); starts/serves the HTTP server
void startWebSetup();      // menu action: ensure the UI is reachable (AP if needed)
void startWifiOnboarding();// WiFi Setup: AP + scan/join page (captive portal)
void stopWebSetup();       // leave the web-setup screen: drop the AP we raised
bool webApActive();
bool wifiOnboardingActive();
String webUrl();           // address to show on the OLED
String webApPass();
