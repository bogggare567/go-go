# OSC / WiFi setup

GO-GO's OSC mode sends UDP OSC messages straight to QLab (or any other
OSC-capable show-control software). This doc covers the fields you'll fill
in, what they mean, and the known size limit worth knowing about.

## What gets sent

Two OSC addresses, configurable, sent as bare OSC messages (no arguments) by
default:

| Action | Default address | Typical QLab mapping |
|--------|-----------------|-----------------------|
| GO     | `/go`           | Workspace → GO |
| PANIC  | `/panic`        | Workspace → Panic (stop everything, faded) |

The four fields that make up the OSC target live in `OscConfig`
(`config.h`): `osc_ip`, `osc_port`, `osc_address`, `panic_address`. They're
set either through the on-device menu, or more comfortably through the web
panel (see below).

## Step by step

1. **Power on** and select **OSC / WiFi** mode (first boot, or via the menu).
2. **Join a network.** On first use the board opens its own setup AP
   (`GO-GO-XXXXXX`, password `password123`) and the WiFiManager portal pops
   up automatically once you join it: pick your venue's WiFi, enter the
   password — the same page also has the OSC target fields from step 3, so
   you can fill everything in one pass. The board reboots onto that network
   and shows its new IP on the OLED.
3. **Open the web panel** at that IP from any device on the same network
   (phone, laptop). Set:
   - **OSC target IP** — the IP address of the computer running QLab, on
     the same network as the board.
   - **OSC port** — QLab's default OSC input port is `53000` (Workspace
     Settings → Network → enable OSC input, confirm the port there).
   - **GO / PANIC addresses** — leave as `/go` and `/panic` unless you've
     mapped custom OSC triggers in QLab.
4. **In QLab:** Workspace Settings → Network → enable "OSC and Web
   Dashboard" (or the equivalent input toggle for your QLab version), note
   the port matches what you set in step 3.
5. **Test:** click the button — OLED shows `GO` → `SENT OSC`; QLab's next
   cue should fire. Hold 1.4s for PANIC.

## The QLab bridge tab

The web panel also has a **QLab** tab that talks to QLab's own HTTP/OSC
bridge directly from your browser (not through the board's GO/PANIC
buttons): transport controls (GO/Panic/Pause/Resume/Stop/Prev/Next/Load/
Reset), a live cue list, and jump-to-cue by number. This is a convenience
window into QLab, separate from the physical button's job.

> **Known limit:** the cue list endpoint (`/cueLists`) replies over UDP,
> which caps a single response around ~1.4 KB. On a workspace with a lot of
> cues, the list shown in the panel may be truncated. This is tracked in
> `docs/ROADMAP.md` (§3) — a TCP-based OSC path would remove the cap but
> isn't implemented yet.

## Troubleshooting

- **`NO WIFI` on the OLED:** click to retry / reopen the setup portal, hold
  to go to the menu. Check the venue network is actually in range and the
  password was typed correctly during onboarding.
- **GO/PANIC sent but QLab doesn't react:** confirm QLab's OSC input is
  enabled and on the port you configured (Workspace Settings → Network) —
  this is the most common miss. Also confirm the computer running QLab and
  the board are on the *same* network/subnet; OSC here is direct UDP, not
  routed across networks or VPNs.
- **Wrong IP typed:** the panel currently doesn't validate the IP/port
  fields before saving (see `docs/ISSUES.md` #5) — a typo will silently fail
  at send time rather than at save time. Double check the digits.
- **Bars show WiFi connected but nothing happens:** verify the board's OSC
  target IP is still correct — if QLab's machine got a new DHCP lease since
  you configured it, update the target IP in the panel.
