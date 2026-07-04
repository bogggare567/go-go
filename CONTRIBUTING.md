# Contributing to GO-GO

Thanks for considering a contribution. GO-GO is a small, single-maintainer
firmware project — this guide keeps things easy to review and easy to keep
working on real hardware.

## Before you start

- Skim [docs/ROADMAP.md](docs/ROADMAP.md) — it tracks what's already decided,
  in progress, or deliberately rejected. Please don't propose something
  that's covered there without reading it first.
- For anything bigger than a small fix, open an issue describing the change
  before writing code. It saves everyone a rewritten PR.
- Look at [docs/ISSUES.md](docs/ISSUES.md) for a list of scoped, beginner-friendly
  tasks if you're not sure where to start.

## Hardware you'll need

GO-GO targets one specific board: **Heltec WiFi LoRa 32 V3** (ESP32-S3 +
SX1262 + SSD1306 OLED). You don't strictly need a second board to test
OSC/WiFi or BLE HID modes, but testing the LoRa modes (remote ↔ gateway)
requires two boards paired together.

## Setting up the build environment

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (2.x) or just
   [arduino-cli](https://arduino.github.io/arduino-cli/) — the project is
   plain Arduino sketch code, no build system beyond that.
2. Add the **Heltec ESP32 Dev-Boards** board package (Boards Manager URL is
   in Heltec's docs).
3. Install libraries: `OSCMessage` (CNMAT), `NimBLE-Arduino`, `RadioLib`,
   `Adafruit SSD1306`, `Adafruit GFX`.
4. Select board **Heltec WiFi LoRa 32 V3** and open `LoRa_soundkorb.ino`.

## Compiling before you submit anything

Every firmware change must compile clean. This is the exact command used to
verify the build (arduino-cli 1.5.1):

```bash
arduino-cli compile --fqbn "Heltec-esp32:esp32:heltec_wifi_lora_32_V3:UploadSpeed=921600,CPUFreq=240,DebugLevel=none,LoopCore=1,EventsCore=1,LORAWAN_REGION=0,LoRaWanDebugLevel=0,LORAWAN_PREAMBLE_LENGTH=0,SLOW_CLK_TPYE=0" .
```

Run this from the repo root before opening a PR and paste the result (or at
least confirm success) in the PR description. There's no unit test suite —
this compile check plus a description of what you tested on real hardware is
the bar for review.

> If your toolchain is missing the standard `<numbers>` header (a known
> packaging bug in some Heltec core releases), see the note in
> [CLAUDE.md](CLAUDE.md) — restoring the header manually is the workaround.

## Code style

- Match the existing style in the file you're editing — this codebase
  doesn't use a formatter or linter, consistency is judged by eye.
- Keep the module boundaries from the v16 split: `config.h` for pins,
  constants, enums and `extern` globals; `gogo.h` as the umbrella include
  each `.cpp` uses; one concern per module (`radio`, `ble_hid`, `osc_wifi`,
  `battery`, `settings`, `modes`, `ui_screens`, `util`). Don't reintroduce
  logic into the monolithic `.ino` beyond `setup()`/`loop()` and the button
  state machine that already lives there.
- Comments should explain *why*, not restate the line below them — see
  existing files for the tone.
- No dynamic memory churn in hot paths (loop, radio ISR context) — this runs
  on a microcontroller with no heap to spare.

## Making a pull request

1. Fork, branch, make your change.
2. Compile-check (see above) and test on real hardware if the change touches
   button logic, radio, BLE, or the display — simulators don't exist for this
   board combination, so describe what you physically tested (e.g. "GO/PANIC
   still fires in OSC mode on a Heltec V3, verified against QLab").
3. Keep PRs focused — one feature or fix per PR. Unrelated formatting changes
   make review harder, please leave them out.
4. Write a clear commit message and PR description: what changed, why, and
   how you tested it.
5. Do not add co-authorship trailers (e.g. `Co-Authored-By: ...` for AI
   tools) to commits in this repo.

## Reporting bugs

Open an issue with: firmware version (`GOGO_VERSION` in `config.h`), control
mode you were in, what you expected vs. what happened, and OLED screen text
if relevant. Serial debug output is only available if you rebuild with
`DEBUG_SERIAL 1` in `config.h`.

## Security issues

Please don't open a public issue for a security concern — see
[SECURITY.md](SECURITY.md).
