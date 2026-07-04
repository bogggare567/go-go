# Security Policy

GO-GO is firmware for a physical show-control remote. It is **not** designed
to be exposed to untrusted networks or the public internet, and its current
threat model assumes a trusted venue LAN or a point-to-point radio link
between two devices you own. Please read the known limitations below before
relying on it for anything safety-critical.

## Known limitations (by design, for now)

These are real, current gaps — not hidden. They're tracked in
[docs/ROADMAP.md](docs/ROADMAP.md) and some are listed as
[good first issues](docs/ISSUES.md):

- **LoRa packets are not authenticated.** Any device on the same channel and
  region can send a `GO` or `PANIC` command that a paired gateway will act
  on. There's a per-packet counter (anti-replay groundwork) but no shared
  key or signature yet. Don't rely on the LoRa link in a hostile RF
  environment for anything where a spoofed PANIC would be dangerous.
- **The web panel has no password in venue-network (STA) mode.** Anyone on
  the same WiFi network can reach the panel and send GO/PANIC or change
  settings. A PIN gate was tried in v16.15 and reverted per owner
  preference (friction wasn't worth it for a trusted venue LAN) — see
  `docs/ISSUES.md` #3 if you want to pick this up properly (e.g. an
  opt-in toggle, off by default). The initial provisioning AP
  (`GO-GO-XXXXXX`) has a separate fixed WiFi password and stays open by
  design (it must be frictionless to join).
- **Firmware OTA updates (`/update`) are not authenticated or signed.**
  Anyone who can reach the panel can push a new `.bin` to the device.
- **OSC has no transport security.** It's plain UDP, matching how QLab and
  most OSC-capable software expect it.

If your use case is a public venue with untrusted network guests, or radio
spectrum you don't control, treat GO-GO as convenience automation — not as a
safety interlock — until these are addressed.

## Reporting a vulnerability

If you find a security issue (e.g. a way to trigger PANIC/GO remotely
without authorization beyond what's documented above, a crash from malformed
input, or a way to brick the device via OTA), please report it privately
rather than opening a public issue:

**bogggare@gmail.com**

Include: firmware version (`GOGO_VERSION` in `config.h`), the mode you were
in (OSC/BLE/LoRa TX/LoRa RX), and steps to reproduce. I'll acknowledge
reports and, for anything that needs a fix, credit you in the release notes
if you'd like.

## Supported versions

Only the latest tagged release/beta on `main` is supported. There is no
long-term-support branch — this is a single-maintainer hobby-scale project.
