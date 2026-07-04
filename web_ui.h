// Web configuration UI: JSON API + embedded page + OTA.
// Runs permanently when WiFi STA is connected (OSC modes); in other modes
// the menu item "Web Setup" raises a temporary access point.
#pragma once
#include "config.h"

void webLoop();            // call from loop(); starts/serves the HTTP server
void startWebSetup();      // menu action: ensure the panel is reachable (AP if needed)
void stopWebSetup();       // leave the web-setup screen: drop the AP we raised
bool webApActive();
String webUrl();           // address to show on the OLED
String webApPass();
