<!--
Draft for r/techtheatre. Not posted automatically — review and post manually.
Fill in [DEMO VIDEO LINK] once docs/video_script.md has been shot and uploaded.
-->

# Title
Built an open-source, one-button wireless GO/PANIC remote for QLab (WiFi-OSC, BLE keyboard, or long-range LoRa) — $20 board, MIT license

# Body

Every show I've run, the same problem: the operator's hands are on the
console, not on the laptop, and the two commands that actually matter in a
crisis are GO and PANIC. I got tired of fumbling for a keyboard or asking a
stage manager to relay "hit space," so I built **GO-GO** — firmware for a
$20 ESP32 dev board that turns one physical button into a dedicated
GO/PANIC remote.

**What it does:**

- **1 click = GO** (advances the cue / fires the effect)
- **hold 1.4s = PANIC** (stops everything, immediately)
- **3 fast clicks** = on-device menu (OLED screen shows status, battery, link)

**Three ways to talk to your show:**

- **OSC over WiFi** — straight into QLab (or Ableton, or anything else that
  takes OSC), no app on the computer
- **Bluetooth keyboard emulation** — zero setup, works with literally
  anything that takes a keypress (QLab, PowerPoint, Keynote, whatever)
- **LoRa radio** — for venues where WiFi is flaky or the operator is far
  from the booth: one remote in-hand, one gateway by the computer, using
  LoRa's long-range characteristics (actual range depends heavily on your
  venue — walls, RF noise, antenna placement)

There's also a built-in web control panel (no app, just a browser) with
live signal strength, battery, a spectrum view, and OTA firmware updates —
so you're not re-flashing over USB every time there's a fix.

It runs on the **Heltec WiFi LoRa 32 V3** (ESP32-S3 + SX1262 + OLED, roughly
$20 on AliExpress/Amazon), and it's fully open source (MIT). Flashing a
pre-built binary takes one `esptool.py` command and about 30 seconds — no
Arduino IDE required unless you want to modify the firmware yourself.

Repo, quick-start flashing instructions, and use-case walkthroughs (QLab,
Ableton, light board triggers) are here:

**https://github.com/bogggare567/go-go**

Demo video: **[DEMO VIDEO LINK]**

I'd love feedback from other techs on what a "GO/PANIC button" should
actually do in your rig, and issues/PRs are very welcome if you want to
extend it (LoRa security hardening, more BLE key presets, more regions —
see the repo's `docs/ISSUES.md` for scoped starter tasks). Fork it, tape a
bigger button to it, make it yours.
