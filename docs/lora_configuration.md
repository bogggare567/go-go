# LoRa configuration

LoRa mode needs **two** GO-GO boards: one **remote** (`LoRa TX`, the button
you hold) and one **gateway** (`LoRa RX`, sits by the computer and outputs
OSC or BLE). This doc covers region/frequency setup, pairing, and what the
link-health indicators mean.

## Region and frequency plan

LoRa radio (SX1262) is capable of 150–960 MHz, but antenna matching on the
Heltec WiFi LoRa 32 V3 and local spectrum law both narrow that down. GO-GO
ships with channel tables for eight ISM regions (`radio.cpp`, `REGIONS[]`):

| Region | Band | Channels | Max TX power |
|--------|------|----------|---------------|
| EU868  | 863–870 MHz | 5 (legacy v15 list, unchanged for compatibility) | 14 dBm |
| RU864  | 864–870 MHz | 5 | 14 dBm |
| US915  | 902–928 MHz | 5 | 22 dBm |
| AU915  | 915–928 MHz | 5 | 22 dBm |
| AS923  | 920–925 MHz | 5 | 16 dBm |
| IN865  | 865–867 MHz | 3 | 20 dBm |
| KR920  | 920.9–923.3 MHz | 5 | 14 dBm |
| CN470  | 470–490 MHz | 5 | 17 dBm — needs the 470–510 MHz hardware variant of the board |

**Both the remote and the gateway must be set to the same region.** Set it
in the on-device menu (`Region`) or the web panel; it's saved to
Preferences (`radio` namespace) and persists across reboots.

> EU868 and RU864 fall under ISM duty-cycle rules (≤1% airtime for EU868).
> The firmware doesn't currently track or enforce this budget — see
> `docs/ISSUES.md` #4 if you want to help close that gap.

### Auto vs. fixed channel

- **Auto (default):** on boot, the gateway scans its region's channel grid
  in 0.2 MHz steps and settles on the quietest one it finds (background RSSI
  scan). The remote scans the same grid listening for the gateway's beacon.
  If RF conditions change mid-show (persistent noise on the current channel,
  no traffic for a while), the gateway can signal a channel hop and both
  units follow.
- **Fixed:** pick a specific channel from the menu (`Freq` setting cycles
  `Auto → <region's channels> → Auto`). Fixed mode disables the automatic
  hopping entirely — useful if you've already surveyed the venue's RF noise
  and want a predictable, unchanging channel.

### Fine considerations

- TX power is always the region's legal maximum — there's no manual power
  dial (an earlier fine-tune-power feature was removed by design; see
  `docs/ROADMAP.md` §1).
- `tuneKhz` in `RadioConfig` allows a small ±100 kHz offset in 25 kHz steps
  for edge-case antenna/crystal drift compensation; you generally shouldn't
  need to touch this on stock hardware.

## Pairing a remote to a gateway

1. On the **gateway**, select `LoRa RX`, then choose its output —
   `OSC WiFi` or `BLE HID` (see `docs/osc_setup.md` / main README for what
   each needs on the QLab/computer side). The gateway starts beaconing every
   ~2 seconds.
2. On the **remote**, select `LoRa TX`. It immediately starts a `PAIR RX`
   scan.
3. The remote lists any gateways it hears (ID + RSSI). **Hold** to pair with
   the highlighted one, or hold on an empty list to pair with *any* gateway
   heard (useful with just one gateway around).
4. Once paired, the remote shows `TX OK xxxxxx` and the LED goes solid. The
   gateway shows its own `RX ... OK` state once its OSC/BLE output is also
   ready.
5. Click for GO, hold 1.4 s for PANIC — the remote sends the command over
   LoRa, waits for an ACK (retried up to 3 times, ~700 ms timeout each), and
   the gateway forwards it as OSC or a BLE keypress.

## Link health

- **`NO CONNECTION`** — no ACK came back after 3 retries. Click to retry.
  Usually means out of range, wrong region/channel between the two units, or
  the gateway isn't powered/paired.
- **`LINK LOST`** — no packets (pings/beacons) seen for 5.2 s (2 missed
  pings plus margin — a single dropped packet won't trigger this). Click to
  retry the search.
- **RSSI/SNR** — shown on the status screen and in the web panel; useful for
  finding a physical position with a cleaner path between remote and
  gateway before a show.

## Security note

LoRa packets are **not authenticated** today — anything transmitting on the
same channel/region that matches the packet format can trigger a GO or
PANIC on a paired gateway. See `SECURITY.md` for the current threat model
and `docs/ISSUES.md` #4 area if you want to help with duty-cycle work (a
separate, related hardening item — shared-key packet auth is tracked
directly in `docs/ROADMAP.md` §4).
