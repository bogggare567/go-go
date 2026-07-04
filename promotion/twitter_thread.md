<!--
Draft thread. Not posted automatically — review, add the demo video/GIF once
shot (docs/video_script.md), and post manually.
-->

1/
Built GO-GO: an open-source, one-button wireless remote for live shows.

Click = GO. Hold = PANIC. That's the whole interface.

Runs on a $20 ESP32 board. MIT licensed.

🧵

2/
Backstory: during a show, the operator's hands are on the console, not the
laptop. When something needs to stop *right now*, fumbling for a keyboard
shortcut is the wrong UX. One physical button, two commands, is the right
amount of interface.

3/
Three ways it talks to your show, switchable at runtime:

→ OSC over WiFi, straight into QLab/Ableton
→ Bluetooth keyboard emulation — works with anything that takes a keypress
→ LoRa radio, for venues where WiFi can't be trusted

4/
The LoRa mode does its own clean-channel scanning on boot — gateway and
remote both sweep the region's channel grid and settle on the quietest one,
with automatic mid-show channel hops if things get noisy. No manual channel
picking needed.

5/
There's also a web control panel (just open it in a browser) — live status,
signal strength, battery, a live spectrum view, and OTA firmware updates. No
re-flashing over USB after the first install.

6/
It's on Heltec's WiFi LoRa 32 V3 board (ESP32-S3 + SX1262 + OLED). Flashing
a pre-built binary is one `esptool.py` command. Fully open, MIT license,
issues tagged for first-time contributors if you want to dig in.

7/
Repo: https://github.com/bogggare567/go-go — the README has animated,
pixel-exact walkthroughs of the LoRa pair, the BLE mode and the WiFi
onboarding (real firmware screens, not mockups). A hands-on demo video is
still on the list — see [DEMO VIDEO LINK] once it's shot.

If you run sound/light for live shows — what would you want a single-button
remote to do that this doesn't yet?
