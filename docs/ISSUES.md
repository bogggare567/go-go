# Good first issues

Scoped, self-contained tasks for a first contribution. Each one is grounded
in the current code (v16.15) — file names and line-level pointers below so
you don't have to go hunting. GitHub Issues aren't set up with these yet
(see maintainer note at the bottom); treat this file as the issue tracker
until they are.

Read [CONTRIBUTING.md](../CONTRIBUTING.md) first for the compile-check
requirement and code style notes.

---

## 1. BLE key presets (QLab / Ableton / Presentation)

**Where:** `ble_hid.cpp`, `ui_screens.cpp` (menu `MENU_GO_KEY` / `MENU_PANIC_KEY`),
`config.h` (`goKeyCode`, `panicKeyCode`).

**What's there now:** the menu already lets you pick a GO key and a PANIC key
independently from a fixed list (Space, Enter, Esc, arrows, PgUp/PgDn, F5,
`B`). That's flexible but requires knowing which two keys a given piece of
software expects.

**What to add:** a "preset" quick-pick above the per-key menu — e.g. `QLab`
(Space/Esc, the current default), `Ableton` (Space/Tab or whatever fits
Ableton's transport shortcuts), `Slides` (Right Arrow/Esc for
PowerPoint/Keynote/Google Slides). Selecting a preset should just set both
`goKeyCode` and `panicKeyCode` in one step and still allow dropping into the
existing per-key menu afterward for manual override. Store the last-used
preset name in Preferences (`system` namespace) so it survives reboot.

**Good for:** someone comfortable with the menu/screen state machine in
`ui_screens.cpp` but not touching radio or BLE internals.

---

## 2. ~~mDNS hostname for the web panel~~ — done in v16.15

`http://gogo.local` now works on the venue LAN (`MDNS.begin("gogo")` in
`web_ui.cpp`, started once the STA link is up). Left here crossed out so a
contributor doesn't duplicate the work; a good follow-up is handling two
GO-GO boards on the same network (currently the second one just fails to
claim the `gogo` hostname — could suffix with the device ID).

---

## 3. Password-protect the web panel in venue-network (STA) mode

**Where:** `web_ui.cpp` (all `server.on(...)` handlers).

**What's there now:** the provisioning AP has a fixed WiFi password
(`password123`), but once the board is on the venue's own network, the panel
itself (status, GO/PANIC buttons, all settings, OTA update) is open to
anyone on that network. Documented as a known limitation in `SECURITY.md`.

**Tried and reverted (v16.15 → v16.16):** HTTP Basic Auth with a PIN (login
`gogo`, default `0000`) was added, then removed at the owner's request —
it added friction (a login prompt on every device that opens the panel,
including `gogo.local`) without buying much on a network you already trust
enough to run a wireless GO/PANIC remote on. If you pick this up again,
make it **opt-in and off by default** (a toggle in Settings), so the
default experience stays frictionless.

**What to add:** simple HTTP Basic Auth (`server.requireAuthentication(...)`
in the ESP32 `WebServer` library) gated behind a password set once in the
panel and stored in Preferences (`system` namespace), defaulting to *off*.
Needs a "set/change panel password" field in the settings page and a way to
reset it from the on-device menu (factory-reset already exists — hook into
that).

---

## 4. Enforce duty-cycle limits for EU868 / RU864

**Where:** `radio.cpp` (`REGIONS[]` table, `sendCommand`/TX path).

**What's there now:** the EU868 and RU864 region entries transmit at max
allowed power but there's no on-air-time budget enforcement — `docs/ROADMAP.md`
notes the legal ≤1% duty cycle for EU868 as a constraint but the firmware
doesn't track or enforce it. In practice GO-GO's traffic is low-volume
(button presses, pings, beacons) so it's unlikely to violate this today, but
there's no guard if beacon/ping intervals are ever tightened.

**What to add:** track cumulative airtime per rolling window (e.g. 1 hour)
for regions that need it, and skip/delay a non-critical transmission (ping,
beacon) if the budget is exhausted — GO/PANIC commands should still always
get through since they're the point of the device, but the periodic
housekeeping traffic (`PING_INTERVAL`, `BEACON_INTERVAL` in `config.h`)
is the place to throttle.

**Good for:** someone who wants a self-contained, math-not-hardware task —
no need for a second board to test the accounting logic in isolation
(though verify on hardware before a PR).

---

## 5. Validate the OSC IP/port fields in the web panel

**Where:** `web_ui.cpp`, around the `/api/config` POST handler
(`config.osc_ip`, `config.osc_port` — currently just `safeCopy`'d from the
raw form field with no format check).

**What's there now:** the original WiFiManager-based onboarding portal (now
retired, per `docs/ROADMAP.md` v16.7) used to reject a malformed IP with
"Invalid IP!". The current web panel's `/api/config` handler writes whatever
string comes in directly into `config.osc_ip` with no validation, and an
invalid IP will silently fail at `udp.beginPacket()` at GO/PANIC time instead
of failing at settings-save time.

**What to add:** validate the IP (four octets, 0–255) and port (1–65535) in
`applyOscConfig` (or wherever the POST is handled) before writing to
`config`, and return an error the panel's JS can show inline instead of
saving invalid values. Bonus: same for the go/panic OSC address fields
(should start with `/`).

**Note (v16.15):** the panel's Settings page is now split into per-mode
cards (only the OSC card shows in OSC mode) — if you pick this issue up,
keep that layout, just add validation to the existing `oscIp`/`oscPort`
inputs.

**Note (v16.17):** WiFi Setup is back to the real WiFiManager portal
(`osc_wifi.cpp`, `runWiFiManager()`), which collects the same OSC target
fields on its page and already checks the IP with `isValidIP()` before
saving — only the panel's own `/api/config` handler still needs this fix.

**Good for:** someone starting with the web panel's request-handling code —
narrow, testable without any radio/BLE hardware quirks.

---

## Note on GitHub Issues

These aren't filed as GitHub Issues yet — do that if/when you're picking one
up, referencing this file, so discussion happens in one place per task
instead of splitting across a static doc and a thread.
