# Hardware assembly

GO-GO needs no custom PCB and no soldering to get running — it's firmware
for a single off-the-shelf dev board. This doc covers the board, optional
battery, antenna handling, and some field-tested mounting ideas for actual
theatre/live use.

## The board

**Heltec WiFi LoRa 32 V3** — ESP32-S3 + SX1262 LoRa radio + SSD1306 OLED, all
on one board. Pin usage is fixed in `config.h`; you don't need to know these
unless you're modifying the firmware for different hardware:

| Function | Pin |
|---|---|
| OLED (I²C) | SDA 17, SCL 18, RST 21 |
| Vext (peripheral power) | 36 |
| LoRa (SPI) | NSS 8, RST 12, BUSY 13, DIO1 14 |
| Button | 0 (the board's built-in **PRG** button) |
| Status LED | `LED_BUILTIN` (25) |
| Battery sense | control 37, ADC 1 |

Get one board for a **BLE HID** or **OSC/WiFi** remote. Get **two** for a
**LoRa** pair (one remote + one gateway) — see `docs/lora_configuration.md`.

## Button

GO-GO uses the board's onboard **PRG** button (GPIO 0) — no wiring needed.
Click = GO, hold 1.4 s = PANIC, 3 fast clicks = menu. If you want a bigger,
more stage-friendly physical button than the tiny onboard tactile switch,
wire an external momentary switch in parallel across the PRG button pads (or
GPIO 0 to GND) rather than replacing the firmware's button pin — this keeps
the software unchanged.

## Battery (optional)

The board has an onboard 2-pin JST 1.25 connector for a single-cell 3.7 V
LiPo battery, with onboard charge management — plug in and it charges over
USB-C automatically. Firmware reads the battery via a voltage divider
(`PIN_BATTERY_CTRL`/`PIN_BATTERY_DATA`) and shows a percentage on the OLED
and in the web panel; below ~3.0 V it reports "no battery" rather than 0%,
so a genuinely flat/disconnected battery doesn't show as false 0%.

Running off USB power only (no battery) works fine for a stationary gateway
sitting next to the computer — you don't need a battery on the RX unit
unless you want it portable too.

## Antenna — read this before powering on

**Attach the LoRa antenna before powering on**, every time. Transmitting
through the SX1262's power amplifier with no antenna load can damage the
radio over repeated use. The antenna connector on the Heltec V3 is the small
U.FL/IPEX-style connector near the LoRa side of the board, usually shipped
with a matching stub or whip antenna for your region's band. If you don't
see one included with your board, get one rated for your operating region's
band (see the table in `docs/lora_configuration.md`) before running LoRa
mode — WiFi/BLE modes don't need it (WiFi/BT use the board's onboard
antenna trace).

## Mounting for actual shows

Field notes from using this as a real cue-fire remote, not just a bench
prototype:

- **Music stand / belt clip:** the board is light enough to tape (gaffer
  tape, always) to a music stand edge or a belt clip case with the OLED
  facing the operator and the PRG button reachable by thumb.
- **Handing it to a stage manager:** if someone other than the sound
  operator holds the remote, orient the board so the OLED is readable at
  arm's length in stage-managed low light — the OLED is not backlit
  brightly, dim booth lighting is usually fine but a fully dark backstage
  corridor may not be.
- **Protect the USB-C port** if the unit will be handled a lot during a run
  — a small 3D-printed or off-the-shelf project box with just the antenna,
  button, and OLED window cut out avoids stress on the port from repeated
  charging cable insertion.
- **Two units, same room, LoRa mode:** keep the paired remote and gateway's
  antenna polarizations roughly aligned (both vertical is simplest) for the
  most consistent range indoors.

## First boot

With the board powered (battery or USB) and antenna attached if you'll use
LoRa: it boots into a `SELECT MODE` screen the very first time (click =
next option, hold = confirm). After that, it remembers the last mode and
boots straight into it — hold the button during power-on if you ever need
to get back to mode selection.
