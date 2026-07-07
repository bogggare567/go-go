# Audit process & checklists

This is a living checklist, not a dated report — earlier versions of this
file were one-off snapshots ("Аудит v16.13 → v16.14") that went stale fast
(referenced debug commands and library choices that later changed). Use
this doc by **running the checklists**, not by reading about a past run.
If you do a big audit pass and want to record what you found, put dated
findings in `docs/ISSUES.md` or a commit message instead — this file stays
current.

## How to use this

- Before a release you're calling "stable": run the **Pre-release
  checklist** in full.
- After touching WiFi/BLE/LoRa code: run that scenario's checklist before
  calling it done.
- After touching any screen or the web panel: run the **Ergonomics
  checklist** — most regressions here are silent (nothing crashes, it's
  just annoying to use).
- `docs/generate_manual_slides.py` and the animation checkers under
  `docs/tests/` (see bottom) catch some of this automatically — run them
  first, they're faster than a human pass.

## Pre-release checklist

- [ ] `arduino-cli compile` with the exact FQBN in `CLAUDE.md` — zero warnings
      you don't already recognize.
- [ ] `DEBUG_SERIAL` is `0` and FQBN `DebugLevel=none` in the build you
      actually flash/ship (debug builds are for testing only).
- [ ] Flash to real hardware, boot from cold, confirm no black screen /
      crash loop / stuck boot animation.
- [ ] `firmware/go-go.bin` in the repo matches a fresh build with the
      documented `UploadSpeed=921600` command (not a `115200`-flashed
      binary — those differ, see git history if this trips you up).
- [ ] `firmware/version.json` version + notes match what you're about to
      tag/release.
- [ ] `python3 docs/tests/check_animations.py` passes (no keyframe overlaps
      or unexplained gaps in `img/*.svg`).
- [ ] Every doc that references a UI element, debug command, or filename
      still matches the code — `grep` for filenames you deleted/renamed
      this release across `*.md`.
- [ ] README, MANUAL.md, PRESENTATION.md don't claim anything not actually
      verified this release (see **Honesty checklist** below).

## WiFi scenario checklist

Run with `DEBUG_SERIAL 1` + FQBN `DebugLevel=info`, serial console open
(see `CLAUDE.md` for the capture command). Don't touch the Mac's own WiFi
while doing this.

- [ ] Fresh boot, no saved network: `NO WIFI` appears within ~1-2s, no long
      black screen.
- [ ] Menu → WiFi Setup opens the WiFiManager portal; join a **2.4 GHz**
      network with the correct password → connects.
- [ ] Same, with a **wrong** password → portal reports failure, doesn't
      silently hang.
- [ ] Reboot after a successful join → device reconnects on its own
      (`connectWiFiWithDisplay(false)` quiet path), no portal, no more than
      a few seconds to `OSC OK`.
- [ ] Serial log during a failed connect attempt: `ASSOC_FAIL`/`NO_AP_FOUND`
      events do **not** repeat faster than the ~15s background-retry
      interval — a tight repeating loop here means the retry-storm bug is
      back (see git log for `WiFi.setAutoReconnect` — this has regressed
      once already).
- [ ] GO on the GO screen while connected → `GO SENT` visibly appears for
      ~700ms before returning to GO (this is the screen that "disappeared"
      once due to the retry storm — if it's flaky again, suspect the same
      class of bug).
- [ ] Menu → Reset → confirm WiFi credentials are actually gone (fresh
      portal shows no pre-filled network, `wm.resetSettings()` ran).

## BLE scenario checklist

- [ ] Pair `GO-GO-XXXXXX` as a Bluetooth keyboard from a phone/laptop.
- [ ] GO / PANIC keystrokes land in a focused text field or QLab — confirm
      with something that visibly reacts (cursor move, cue advance), not
      just the OLED.
- [ ] Disconnect and let it re-advertise (`onDisconnect` → `startAdvertising`)
      — reconnects without a factory reset.
- [ ] Web panel → Settings → GO/PANIC key: **dropdown** picks a preset
      correctly, **"Press a key…"** captures an arbitrary key from a real
      keyboard and it's usable after Save (see `docs/tests/` for a browser
      test of the capture logic itself).
- [ ] Status screen and web panel status both show the correct key names
      after a custom (non-preset) key is set — should show `#XX` hex, not
      a blank/`?`.

## LoRa scenario checklist

Needs two boards (remote + gateway). Note in the report if you only had
one — code review isn't a substitute for a real pairing test.

- [ ] RX (gateway) beacons; TX (remote) finds it in `PAIR RX` scan within a
      few seconds on **Auto** frequency.
- [ ] Hold to pair; both sides show `LINK OK` / `TX OK` with plausible RSSI.
- [ ] GO / PANIC from the remote reaches the gateway and forwards correctly
      (OSC or BLE, whichever the gateway output is set to).
- [ ] Walk the remote out of range → `LINK LOST` within ~5s (2 missed
      pings + margin), then back in range → recovers without a reboot.
- [ ] If you get a real range measurement out of this (meters, indoor or
      outdoor, line-of-sight or not) — **write it into
      `docs/PRESENTATION.md` and `docs/MANUAL.md` replacing the
      datasheet-only figure.** This is the single most-requested "real
      data" gap in the project right now.

## Ergonomics checklist

Aimed at "does this feel obvious to someone who's never seen the device,"
not "does the code work."

- [ ] First boot, no manual: can you get to a working mode without reading
      anything, just clicking/holding?
- [ ] Every screen with a hold-action shows the hold-progress bar (grep
      `ui_screens.cpp` for `drawHoldProgress` calls if a screen was added —
      this has been missed before).
- [ ] Web panel: default landing tab is Status (not Settings) — confirm
      after any tab-related change.
- [ ] Web panel: no field silently fails — bad IP/port should be visibly
      rejected or clearly explained, not just not-work later (tracked gap:
      `docs/ISSUES.md` #5).
- [ ] Error screens (`NO WIFI`, `NO BLE`, `NO LINK`, `NO CONNECTION`) each
      tell you what to press next, not just that something's wrong.
- [ ] Nothing on the OLED requires reading small text at a distance during
      an actual show — the GO/PANIC state should be readable at a glance.

## Honesty checklist (before publishing anything)

- Don't state a distance, timing, or capability you haven't personally
  measured on real hardware this release. "Datasheet says ~2-3km" is fine
  if labeled as datasheet, not our measurement — "the range is 2-3km" is not.
- Don't stage a "successful" demo shot/recording beyond what actually
  happened — a failed take gets recut, not implied-successful.
- If a claim in README/MANUAL/PRESENTATION turns out to be untested,
  either test it or soften the wording — don't leave it phrased as fact.

## Automated tests

`docs/tests/` (not part of the Arduino build — run with plain `python3`):

- `check_animations.py` — parses every `@keyframes` block in `img/*.svg`
  and flags overlapping or unexplained-gap states between mutually
  exclusive screen layers (`k-boot`/`k-go`/`k-gosent`/`k-panic` etc.).
- `test_logic.py` — host-side tests for the pure-logic bits worth covering
  without hardware: CRC16, `isValidIP`, and the browser key-capture map
  cross-checked against `ble_hid.cpp`'s `KEY_OPTIONS`.

Neither needs a board connected. Run both before a release; CI does not
run them yet (they're new — wire into `.github/workflows/ci.yml` once
they've proven stable across a few runs).
