#!/usr/bin/env python3
"""GO-GO hero animation v2: real board photo + MacBook mockup.
OLED screens are pixel-exact renders of the firmware draw functions (OSC mode),
overlaid on the photo's actual OLED glass (measured x340..623, y274..422)."""

# Usage: python3 docs/generate_demo.py  (run from the repo root)
# Needs: img/heltec.png, img/macbook.png, and glcdfont.c from Adafruit-GFX
# (auto-downloaded on first run). The macbook image is downscaled via sips (macOS).

import re, os, base64, subprocess, urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.path.join(REPO, "docs", ".demo_cache")
os.makedirs(TMP, exist_ok=True)

font_path = os.path.join(TMP, "glcdfont.c")
if not os.path.exists(font_path):
    urllib.request.urlretrieve(
        "https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/glcdfont.c",
        font_path)

mac_small = os.path.join(TMP, "macbook_s.png")
if not os.path.exists(mac_small):
    subprocess.run(["sips", "-Z", "1400", f"{REPO}/img/macbook.png", "--out", mac_small],
                   check=True, capture_output=True)

src = open(font_path).read()
FONT = [int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", src)]
W, H = 128, 64

class FB:
    def __init__(self, fill=0):
        self.px = [[fill] * W for _ in range(H)]
    def set(self, x, y, c=1):
        if 0 <= x < W and 0 <= y < H: self.px[y][x] = c
    def fill_rect(self, x, y, w, h, c=1):
        for yy in range(y, y + h):
            for xx in range(x, x + w): self.set(xx, yy, c)
    def draw_rect(self, x, y, w, h, c=1):
        for xx in range(x, x + w): self.set(xx, y, c); self.set(xx, y + h - 1, c)
        for yy in range(y, y + h): self.set(x, yy, c); self.set(x + w - 1, yy, c)
    def draw_char(self, x, y, ch, size, c):
        code = ord(ch)
        for i in range(5):
            line = FONT[code * 5 + i]
            for j in range(8):
                if line & 1:
                    if size == 1: self.set(x + i, y + j, c)
                    else: self.fill_rect(x + i * size, y + j * size, size, size, c)
                line >>= 1
    def text(self, x, y, s, size=1, c=1):
        for ch in s:
            self.draw_char(x, y, ch, size, c); x += 6 * size
    def centered(self, s, y, size=1, c=1):
        self.text((W - len(s) * 6 * size) // 2, y, s, size, c)
    def runs_path(self):
        parts = []
        for y in range(H):
            x = 0
            while x < W:
                if self.px[y][x]:
                    x0 = x
                    while x < W and self.px[y][x]: x += 1
                    parts.append(f"M{x0} {y}h{x - x0}v1h-{x - x0}z")
                else: x += 1
        return "".join(parts)

def chrome_osc(fb):
    # drawPowerIcon(2,2), battery ~75%
    fb.draw_rect(2, 2, 10, 5); fb.fill_rect(12, 3, 2, 3); fb.fill_rect(3, 3, 6, 3)
    # drawWiFiIndicator(106,0), RSSI >= -40 -> 4 filled bars
    for i in range(4):
        h = 2 + i * 2
        fb.fill_rect(106 + i * 4, 8 - h, 3, h)

boot = FB(fill=1); boot.text(10, 24, "soundkorb", 2, 0)

go = FB(); chrome_osc(go)
go.centered("OSC OK", 2, 1)
go.fill_rect(34, 14, 60, 32, 1); go.text(46, 18, "GO", 3, 0)
go.centered("soundkorb.ru", 54, 1)

gosent = FB(); chrome_osc(gosent)
gosent.draw_rect(34, 12, 60, 28, 1); gosent.text(46, 14, "GO", 3, 1)
gosent.centered("SENT OSC", 50, 1)

panic = FB(); chrome_osc(panic)
panic.text(19, 12, "PANIC", 3, 1)
panic.centered("SENT OSC", 48, 1)

paths = {k: fb.runs_path() for k, fb in
         {"boot": boot, "go": go, "gosent": gosent, "panic": panic}.items()}

b64_board = base64.b64encode(open(f"{REPO}/img/heltec.png", "rb").read()).decode()
b64_mac = base64.b64encode(open(mac_small, "rb").read()).decode()

OLED = "#e6f3ff"
DUR = "12s"

def layer(name):
    return f'<g class="scr scr-{name}"><path d="{paths[name]}" fill="{OLED}"/></g>'

# Board photo space: OLED glass x340..623 (283w), y274..422 (149h)
# -> screen transform: translate(341,275) scale(2.20,2.31)
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1600 800" font-family="Menlo, Consolas, monospace">
<style>
.scr {{ opacity: 0; }}
@keyframes k-boot   {{ 0%,10% {{opacity:1}} 12%,100% {{opacity:0}} }}
@keyframes k-go     {{ 0%,11% {{opacity:0}} 13%,32% {{opacity:1}} 34%,46% {{opacity:0}} 48%,60% {{opacity:1}} 62%,86% {{opacity:0}} 88%,100% {{opacity:1}} }}
@keyframes k-gosent {{ 0%,33% {{opacity:0}} 35%,46% {{opacity:1}} 48%,100% {{opacity:0}} }}
@keyframes k-panic  {{ 0%,61% {{opacity:0}} 63%,86% {{opacity:1}} 88%,100% {{opacity:0}} }}
.scr-boot   {{ animation: k-boot   {DUR} step-end infinite; }}
.scr-go     {{ animation: k-go     {DUR} step-end infinite; }}
.scr-gosent {{ animation: k-gosent {DUR} step-end infinite; }}
.scr-panic  {{ animation: k-panic  {DUR} step-end infinite; }}

@keyframes k-led {{ 0%,33% {{opacity:0}} 34%,37% {{opacity:1}} 38%,61% {{opacity:0}} 62%,72% {{opacity:1}} 73%,100% {{opacity:0}} }}
.led {{ animation: k-led {DUR} linear infinite; }}

@keyframes k-press {{ 0%,32% {{opacity:0; transform:scale(.5)}} 33% {{opacity:.95}} 37% {{opacity:0; transform:scale(1.6)}} 38%,59% {{opacity:0; transform:scale(.5)}} 60% {{opacity:.95}} 61% {{opacity:.95; transform:scale(1.2)}} 66% {{opacity:0; transform:scale(1.7)}} 67%,100% {{opacity:0; transform:scale(.5)}} }}
.press {{ animation: k-press {DUR} ease-out infinite; transform-origin: 106px 470px; transform-box: view-box; }}

@keyframes k-wave {{ 0%,33% {{opacity:0; transform:scale(.4)}} 35% {{opacity:.9}} 44% {{opacity:0; transform:scale(1.6)}} 45%,61% {{opacity:0; transform:scale(.4)}} 63% {{opacity:.9}} 72% {{opacity:0; transform:scale(1.6)}} 73%,100% {{opacity:0; transform:scale(.4)}} }}
.wave {{ animation: k-wave {DUR} ease-out infinite; transform-origin: 560px 480px; transform-box: view-box; }}
.wave2 {{ animation-delay: .3s; }}

@keyframes k-cue1 {{ 0%,34% {{opacity:1}} 36%,98% {{opacity:0}} 100% {{opacity:1}} }}
@keyframes k-cue2 {{ 0%,34% {{opacity:0}} 36%,62% {{opacity:1}} 64%,100% {{opacity:0}} }}
.cue1sel {{ animation: k-cue1 {DUR} step-end infinite; }}
.cue2sel {{ animation: k-cue2 {DUR} step-end infinite; }}
@keyframes k-goflash {{ 0%,33% {{opacity:0}} 34%,40% {{opacity:1}} 44%,100% {{opacity:0}} }}
.goflash {{ animation: k-goflash {DUR} linear infinite; }}
@keyframes k-stop {{ 0%,61% {{opacity:0}} 63%,84% {{opacity:1}} 87%,100% {{opacity:0}} }}
.stop {{ animation: k-stop {DUR} step-end infinite; }}
@keyframes k-bar {{ 0%,34% {{width:120px}} 35%,36% {{width:0}} 62% {{width:120px}} 63%,99% {{width:0}} 100% {{width:120px}} }}
.bar {{ animation: k-bar {DUR} linear infinite; }}
</style>

<rect width="1600" height="800" rx="24" fill="#101826"/>
<text x="800" y="58" text-anchor="middle" fill="#e8eef7" font-size="34" font-weight="bold">GO-GO &#183; one-button show control</text>
<text x="800" y="772" text-anchor="middle" fill="#5d6c80" font-size="22">1 click = GO &#160;&#183;&#160; hold 1.4s = PANIC &#160;&#183;&#160; WiFi&#8209;OSC / BLE keyboard / LoRa</text>

<!-- MacBook -->
<image href="data:image/png;base64,{b64_mac}" x="480" y="80" width="1060"/>
<!-- QLab-style window on the Mac screen (screen area ~x556..1473, y50..624) -->
<g transform="translate(640,120)">
  <rect width="740" height="460" rx="14" fill="#171d27" stroke="#2b3950" stroke-width="2"/>
  <rect width="740" height="52" rx="14" fill="#222c3b"/>
  <circle cx="30" cy="26" r="8" fill="#ff5f57"/><circle cx="56" cy="26" r="8" fill="#febc2e"/><circle cx="82" cy="26" r="8" fill="#28c840"/>
  <text x="370" y="34" text-anchor="middle" fill="#8fa1b8" font-size="22">cue list</text>

  <rect class="cue1sel" x="20" y="74" width="700" height="62" rx="8" fill="#2b4a75"/>
  <rect class="cue2sel" x="20" y="146" width="700" height="62" rx="8" fill="#2b4a75"/>
  <text x="46" y="114" fill="#dfe8f4" font-size="28">1&#160;&#160;Intro music</text>
  <text x="46" y="186" fill="#dfe8f4" font-size="28">2&#160;&#160;Thunder SFX</text>
  <text x="46" y="258" fill="#9fb0c5" font-size="28">3&#160;&#160;Blackout</text>

  <rect class="bar" x="580" y="94" width="120" height="22" rx="6" fill="#3fae5f"/>

  <g class="goflash">
    <rect x="20" y="330" width="700" height="110" rx="10" fill="#1f5c33"/>
    <text x="370" y="402" text-anchor="middle" fill="#8ef0ae" font-size="52" font-weight="bold">GO &#9654;</text>
  </g>
  <g class="stop">
    <rect x="20" y="330" width="700" height="110" rx="10" fill="#5c1f1f"/>
    <text x="370" y="400" text-anchor="middle" fill="#ff9d9d" font-size="48" font-weight="bold">&#9632; ALL STOPPED</text>
  </g>
</g>

<!-- radio waves from board antenna toward the Mac -->
<g stroke="#6fd18c" stroke-width="5" fill="none">
  <path class="wave" d="M570 435 a55 55 0 0 1 0 90"/>
  <path class="wave wave2" d="M592 415 a85 85 0 0 1 0 130"/>
</g>

<!-- Heltec board (photo space 800x800 scaled 0.78, placed at 0,250) -->
<g transform="translate(0,250) scale(0.78)">
  <image href="data:image/png;base64,{b64_board}" width="800" height="800"/>
  <!-- live screen on the OLED glass -->
  <g transform="translate(341,275) scale(2.20,2.31)">
    {layer("boot")}{layer("go")}{layer("gosent")}{layer("panic")}
  </g>
  <!-- LED glow (photo LED at 195,478) -->
  <circle class="led" cx="195" cy="478" r="14" fill="#ffd76a" opacity="0"/>
  <circle class="led" cx="195" cy="478" r="26" fill="#ffd76a" opacity="0" style="filter:blur(6px)"/>
</g>
<!-- press ring around PRG button (scene coords) -->
<circle class="press" cx="106" cy="470" r="34" fill="none" stroke="#7fb4ff" stroke-width="5"/>
</svg>
'''

out = f"{REPO}/img/gogo-demo.svg"
open(out, "w").write(svg)
print(f"wrote {out}: {len(svg)/1024:.0f} KB")
