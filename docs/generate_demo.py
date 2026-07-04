#!/usr/bin/env python3
"""GO-GO hero animation: real board photo + MacBook mockup + QLab-5-style workspace.
OLED screens are pixel-exact renders of the firmware draw functions (OSC mode),
overlaid on the photo's actual OLED glass (measured x340..623, y274..422).

Usage: python3 docs/generate_demo.py  (run from the repo root)
Needs: img/heltec.png, img/macbook.png; glcdfont.c from Adafruit-GFX is
auto-downloaded on first run. The macbook image is downscaled via sips (macOS).
QLab look reference: img/qlab.png, https://qlab.app/docs/v5/

Shares its OLED renderer / photo embedding / QLab window mockup with
docs/generate_scenario_demos.py via docs/demo_lib.py — see that module if
you're adding a new scenario animation rather than touching this hero one.
"""

from demo_lib import (REPO, FB, layer, qlab_window, b64_file,
                       cached_macbook, cached_brand_font)

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

b64_board = b64_file(f"{REPO}/img/heltec.png")
b64_mac = b64_file(cached_macbook())
b64_font = b64_file(cached_brand_font())

DUR = "12s"

svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1600 800" font-family="Menlo, Consolas, monospace">
<style>
@font-face {{
  font-family: 'Unbounded';
  src: url(data:font/woff2;base64,{b64_font}) format('woff2');
  font-weight: 200 900;
}}
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

/* QLab window: standby moves 1 -> 2 on GO, back at loop end */
@keyframes k-qsel1 {{ 0%,34% {{opacity:1}} 35%,97% {{opacity:0}} 98%,100% {{opacity:1}} }}
@keyframes k-qsel2 {{ 0%,34% {{opacity:0}} 35%,97% {{opacity:1}} 98%,100% {{opacity:0}} }}
.q-sel1, .q-ph1, .q-std1 {{ animation: k-qsel1 {DUR} step-end infinite; }}
.q-sel2, .q-ph2, .q-std2 {{ opacity:0; animation: k-qsel2 {DUR} step-end infinite; }}
.q-rowtx text {{ fill: currentColor; }}
.q-rowtx .q-ic path {{ fill: currentColor; stroke: currentColor; }}
.q-rowtx circle {{ stroke: currentColor; fill: none; }}
.q-rowtx circle + circle {{ fill: currentColor; stroke: none; }}
@keyframes k-rowtx0 {{ 0%,34% {{color:#141414}} 35%,97% {{color:#e4e4e6}} 98%,100% {{color:#141414}} }}
@keyframes k-rowtx1 {{ 0%,34% {{color:#e4e4e6}} 35%,97% {{color:#141414}} 98%,100% {{color:#e4e4e6}} }}
.q-rowtx0 {{ animation: k-rowtx0 {DUR} step-end infinite; }}
.q-rowtx1 {{ animation: k-rowtx1 {DUR} step-end infinite; }}

.q-goflash {{ opacity:0; animation: k-qgoflash {DUR} linear infinite; }}
@keyframes k-qgoflash {{ 0%,32% {{opacity:0}} 33.5%,36% {{opacity:.85}} 40%,100% {{opacity:0}} }}
.q-panicflash {{ opacity:0; animation: k-qpanicflash {DUR} linear infinite; }}
@keyframes k-qpanicflash {{ 0%,60% {{opacity:0}} 62%,66% {{opacity:.85}} 71%,100% {{opacity:0}} }}
.q-playtint {{ opacity:0; animation: k-qplaytint {DUR} step-end infinite; }}
@keyframes k-qplaytint {{ 0%,34% {{opacity:0}} 35%,62% {{opacity:.16}} 63%,100% {{opacity:0}} }}
.q-playbar {{ animation: k-qplaybar {DUR} linear infinite; }}
@keyframes k-qplaybar {{ 0%,35% {{width:0}} 62% {{width:640px}} 63%,100% {{width:0}} }}
.q-panicrow {{ opacity:0; animation: k-qpanicrow {DUR} linear infinite; }}
@keyframes k-qpanicrow {{ 0%,61% {{opacity:0}} 63%,66% {{opacity:.4}} 71%,100% {{opacity:0}} }}
</style>

<rect width="1600" height="800" rx="24" fill="#101826"/>
<text x="800" y="58" text-anchor="middle" fill="#e8eef7" font-size="30" font-weight="700" font-family="Unbounded, Menlo, sans-serif">GO-GO &#183; one-button show control</text>
<text x="800" y="770" text-anchor="middle" fill="#5d6c80" font-size="17" font-weight="400" font-family="Unbounded, Menlo, sans-serif">1 click = GO &#160;&#183;&#160; hold 1.4s = PANIC &#160;&#183;&#160; WiFi&#8209;OSC / BLE keyboard / LoRa</text>

<!-- MacBook -->
<image href="data:image/png;base64,{b64_mac}" x="480" y="80" width="1060"/>
<!-- QLab-5-style workspace on the Mac screen (screen area ~x587..1432, y98..627) -->
<g transform="translate(628,112)" font-family="Helvetica, Arial, sans-serif">
  {qlab_window()}
</g>

<!-- radio waves from board antenna toward the Mac -->
<g stroke="#6fd18c" stroke-width="5" fill="none">
  <path class="wave" d="M570 435 a55 55 0 0 1 0 90"/>
  <path class="wave wave2" d="M592 415 a85 85 0 0 1 0 130"/>
</g>

<!-- Heltec board (photo space 800x800 scaled 0.78, placed at 0,250) -->
<g transform="translate(0,250) scale(0.78)">
  <image href="data:image/png;base64,{b64_board}" width="800" height="800"/>
  <g transform="translate(341,275) scale(2.20,2.31)">
    {layer(paths, "boot")}{layer(paths, "go")}{layer(paths, "gosent")}{layer(paths, "panic")}
  </g>
  <circle class="led" cx="195" cy="478" r="14" fill="#ffd76a" opacity="0"/>
  <circle class="led" cx="195" cy="478" r="26" fill="#ffd76a" opacity="0" style="filter:blur(6px)"/>
</g>
<circle class="press" cx="106" cy="470" r="34" fill="none" stroke="#7fb4ff" stroke-width="5"/>
</svg>
'''

out = f"{REPO}/img/gogo-demo.svg"
open(out, "w").write(svg)
print(f"wrote {out}: {len(svg)/1024:.0f} KB")
