#!/usr/bin/env python3
"""Host-side tests for logic that doesn't need real hardware.

Two things get checked:

1. crc16_ccitt() / isValidIP() in util.cpp - reimplemented here in Python
   (util.cpp needs the full ESP32 Arduino toolchain to compile, which this
   script deliberately avoids depending on) and cross-checked against the
   actual constants in the source so an algorithm change doesn't silently
   go untested.
2. The "press a key" HID_CODE map in web_html.h vs. the KEY_OPTIONS preset
   list in ble_hid.cpp - every preset the firmware/panel dropdown offers
   must also be reachable by pressing that physical key in a browser, or
   the two pickers would disagree with each other.

Usage: python3 docs/tests/test_logic.py  (run from anywhere)
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
failures = []


def check(name, cond, detail=""):
    if cond:
        print(f"  ok   {name}")
    else:
        print(f"  FAIL {name}  {detail}")
        failures.append(name)


# ============================================================================
# 1. crc16_ccitt / isValidIP
# ============================================================================
print("util.cpp logic")

util_src = open(os.path.join(REPO, "util.cpp"), encoding="utf-8").read()

# Guard against silent algorithm drift: if someone changes the poly/init in
# util.cpp without updating this mirror, fail loudly instead of testing the
# wrong algorithm.
check("crc16_ccitt still uses init 0xFFFF, poly 0x1021 (mirrored below)",
      "crc = 0xFFFF" in util_src and "0x1021" in util_src,
      "util.cpp's CRC constants changed - update the Python mirror in this file")


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


check("crc16 of empty input is the init value", crc16_ccitt(b"") == 0xFFFF)
check("crc16 is deterministic", crc16_ccitt(b"GO-GO") == crc16_ccitt(b"GO-GO"))
check("crc16 differs for different inputs", crc16_ccitt(b"GO") != crc16_ccitt(b"PANIC"))
check("crc16 differs for a single flipped bit (catches non-avalanching bugs)",
      crc16_ccitt(bytes([0x01, 0x02, 0x03])) != crc16_ccitt(bytes([0x01, 0x02, 0x02])))
check("crc16 known vector '123456789' == 0x29B1 (standard CRC-16/CCITT-FALSE)",
      crc16_ccitt(b"123456789") == 0x29B1)


def is_valid_ip(ip: str) -> bool:
    parts = ip.split(".")
    if len(parts) != 4:
        return False
    for p in parts:
        if not p.lstrip("-").isdigit():
            return False
        n = int(p)
        if n < 0 or n > 255:
            return False
    return True


check("isValidIP accepts a normal address", is_valid_ip("192.168.1.100"))
check("isValidIP accepts edges 0 and 255", is_valid_ip("0.0.0.0") and is_valid_ip("255.255.255.255"))
check("isValidIP rejects an octet > 255", not is_valid_ip("192.168.1.999"))
check("isValidIP rejects too few octets", not is_valid_ip("192.168.1"))
check("isValidIP rejects garbage", not is_valid_ip("not an ip"))
check("isValidIP rejects empty string", not is_valid_ip(""))

# ============================================================================
# 2. BLE key preset cross-check: ble_hid.cpp KEY_OPTIONS vs web_html.h's
#    browser-side HID_CODE capture map
# ============================================================================
print("\nBLE key preset <-> browser capture map")

ble_src = open(os.path.join(REPO, "ble_hid.cpp"), encoding="utf-8").read()
html_src = open(os.path.join(REPO, "web_html.h"), encoding="utf-8").read()

m = re.search(r"KEY_OPTIONS\[\]\s*=\s*\{(.*?)\};", ble_src, re.S)
check("found KEY_OPTIONS[] in ble_hid.cpp", m is not None)
presets = {}
if m:
    for pm in re.finditer(r"\{(0x[0-9A-Fa-f]+),\s*\"([^\"]+)\"\}", m.group(1)):
        presets[pm.group(2)] = int(pm.group(1), 16)
check(f"parsed {len(presets)} presets from KEY_OPTIONS", len(presets) > 0)

# Build the same code->HID map the browser JS computes, from web_html.h's
# literal table plus its generated letter/digit/F-key loops.
hid_code = {}
m = re.search(r"const HID_CODE=\{(.*?)\};", html_src, re.S)
check("found HID_CODE literal table in web_html.h", m is not None)
if m:
    for entry in re.findall(r"(\w+):(0x[0-9A-Fa-f]+)", m.group(1)):
        hid_code[entry[0]] = int(entry[1], 16)
for i in range(26):
    hid_code[f"Key{chr(65+i)}"] = 0x04 + i
for i in range(1, 10):
    hid_code[f"Digit{i}"] = 0x1E + (i - 1)
hid_code["Digit0"] = 0x27
for i in range(1, 13):
    hid_code[f"F{i}"] = 0x3A + (i - 1)

hid_values = set(hid_code.values())
for name, code in presets.items():
    check(f"preset '{name}' (0x{code:02X}) is reachable via browser key capture",
          code in hid_values, "no KeyboardEvent.code maps to this HID usage in web_html.h")

if failures:
    print(f"\nFAIL - {len(failures)} check(s) failed: {failures}")
    sys.exit(1)
print("\nOK - all logic checks passed.")
