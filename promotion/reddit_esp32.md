<!--
Draft for r/esp32. Not posted automatically — review and post manually.
Fill in [DEMO VIDEO LINK] once docs/video_script.md has been shot and uploaded.
-->

# Title
GO-GO: Heltec WiFi LoRa 32 V3 firmware with 4 runtime-switchable modes (WiFi-OSC, BLE HID, LoRa TX, LoRa RX gateway) — auto channel selection, web control panel with OTA, open source

# Body

Sharing a firmware project for the **Heltec WiFi LoRa 32 V3**
(ESP32-S3 + SX1262 + SSD1306) that ended up being a decent showcase of a few
ESP32 techniques, even if the use case (theatre/live-show remote control) is
niche.

**The pitch:** a single physical button that sends "GO" or "PANIC" (long
press) commands, switchable at runtime between four transport modes:

- **OSC over UDP** to QLab/Ableton/anything OSC-capable
- **BLE HID keyboard emulation** (NimBLE) — GO/PANIC map to configurable
  keystrokes, so it works with software that has no OSC or network API at
  all
- **LoRa TX** (remote) and **LoRa RX** (gateway that forwards to OSC or BLE)
  — for when WiFi isn't trustworthy at the venue

**Some of the more interesting bits under the hood:**

- **Automatic clean-channel selection for LoRa** — the gateway does a
  spectral scan across its region's channel grid (0.2 MHz steps) on boot and
  settles on the quietest bin; the remote scans the same grid listening for
  the gateway's beacon. `RadioLib`'s `scanChannel()` (CAD) plus RX-mode RSSI
  sampling does the legwork.
- **8 region frequency plans** (EU868/RU864/US915/AU915/AS923/IN865/KR920/
  CN470) baked into one table (`radio.cpp`), selectable from the menu or web
  panel, each with its own channel list and legal max TX power.
- **A custom pairing protocol over LoRa** — beacon/ping/ack packet types
  with a 16-bit counter (anti-replay groundwork), RSSI/SNR reporting, and a
  master/slave channel-hop mechanism so a paired pair can jump off a
  suddenly-noisy channel mid-show without operator intervention.
- **Built-in web control panel** — no external `WiFiManager` dependency
  (dropped it in favor of a from-scratch captive-portal onboarding flow),
  live status, a live RSSI spectrum view mirrored between OLED and browser,
  and **OTA firmware updates** via `Update.h` straight from the panel
  (`.bin` upload, no USB cable needed after first flash).
- **NimBLE HID** keyboard device with configurable per-action keycodes,
  stored in `Preferences` (NVS).

Firmware is split into per-concern modules (`radio`, `ble_hid`, `osc_wifi`,
`web_ui`, `battery`, `settings`, `ui_screens`) rather than one giant `.ino` —
worth a look if you're structuring your own multi-mode Arduino-framework
project and want a plain-Arduino-IDE-compatible way to do it (no PlatformIO
required, though it'd work there too).

MIT licensed, arduino-cli compile-checked in CI. Repo, pinout, and a
scoped list of beginner-friendly issues (BLE key presets, mDNS, duty-cycle
enforcement, panel auth) are here:

**https://github.com/bogggare567/go-go**

Demo: **[DEMO VIDEO LINK]**

Happy to go into more detail on the channel-hopping protocol or the CAD
scanning approach if anyone's interested — and PRs/issues welcome.
