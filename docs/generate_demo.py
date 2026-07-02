#!/usr/bin/env python3
"""GO-GO hero animation: real board photo + MacBook mockup + QLab-5-style workspace.
OLED screens are pixel-exact renders of the firmware draw functions (OSC mode),
overlaid on the photo's actual OLED glass (measured x340..623, y274..422).

Usage: python3 docs/generate_demo.py  (run from the repo root)
Needs: img/heltec.png, img/macbook.png; glcdfont.c from Adafruit-GFX is
auto-downloaded on first run. The macbook image is downscaled via sips (macOS).
QLab look reference: img/qlab.png, https://qlab.app/docs/v5/"""

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

# ============================ OLED framebuffer ==============================
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

# ============================ QLab 5 style window ===========================
# Palette sampled from a real QLab 5 workspace (img/qlab.png)
Q = {
    "win": "#2b2b2d", "title": "#3b3b3d", "titletxt": "#cfcfd2",
    "field": "#1c1c1e", "fieldtxt": "#e8e8ea", "muted": "#77777c",
    "hdr": "#39393b", "hdrtxt": "#d2d2d5",
    "row": "#181818", "rowalt": "#141414", "rowtxt": "#e4e4e6",
    "sel": "#f2c53d", "seltxt": "#141414",
    "gofill": "#98989d", "gostroke": "#39d353",
    "green": "#2fbf5b", "red": "#e5484d",
}

CUES = [  # number, name, duration
    ("1", "Intro music",     "01:14.32"),
    ("2", "Thunder SFX",     "00:16.43"),
    ("3", "Rain loop",       "00:45.00"),
    ("4", "Lightning flash", "00:01.20"),
    ("5", "House to half",   "00:05.00"),
    ("6", "Blackout",        "00:03.00"),
]

def speaker_icon(x, y, cls):
    return (f'<g transform="translate({x},{y})" class="{cls}">'
            f'<path d="M0 4h4l5-4v14l-5-4H0z"/>'
            f'<path d="M11 3a5.5 5.5 0 0 1 0 8" fill="none" stroke="currentColor" stroke-width="1.6"/>'
            f'</g>')

def target_icon(x, y):
    return (f'<circle cx="{x}" cy="{y}" r="6" fill="none" stroke="currentColor" stroke-width="1.5"/>'
            f'<circle cx="{x}" cy="{y}" r="2.2" fill="currentColor"/>')

def qlab_window():
    Wq = 780
    s = [f'<rect width="{Wq}" height="490" rx="10" fill="{Q["win"]}"/>']
    # title bar
    s.append(f'<path d="M0 10a10 10 0 0 1 10-10h{Wq-20}a10 10 0 0 1 10 10v16H0z" fill="{Q["title"]}"/>')
    s.append('<circle cx="18" cy="13" r="6" fill="#ff5f57"/><circle cx="38" cy="13" r="6" fill="#febc2e"/><circle cx="58" cy="13" r="6" fill="#28c840"/>')
    s.append(f'<text x="{Wq/2}" y="18" text-anchor="middle" fill="{Q["titletxt"]}" font-size="13" font-family="Helvetica, Arial, sans-serif">My Show.qlab5 &#8212; Cue Lists</text>')

    # GO button (grey, green border) + flash overlays
    s.append(f'<rect x="14" y="38" width="92" height="74" rx="8" fill="{Q["gofill"]}" stroke="{Q["gostroke"]}" stroke-width="3"/>')
    s.append(f'<rect class="q-goflash" x="14" y="38" width="92" height="74" rx="8" fill="{Q["green"]}"/>')
    s.append(f'<rect class="q-panicflash" x="14" y="38" width="92" height="74" rx="8" fill="{Q["red"]}"/>')
    s.append('<text x="60" y="90" text-anchor="middle" fill="#fff" font-size="42" font-weight="600" font-family="Helvetica, Arial, sans-serif">GO</text>')

    # standby cue field + notes field
    s.append(f'<rect x="120" y="38" width="646" height="30" rx="6" fill="{Q["field"]}"/>')
    s.append(f'<text class="q-std1" x="134" y="59" fill="{Q["fieldtxt"]}" font-size="16" font-family="Helvetica, Arial, sans-serif">1 &#183; Intro music</text>')
    s.append(f'<text class="q-std2" x="134" y="59" fill="{Q["fieldtxt"]}" font-size="16" font-family="Helvetica, Arial, sans-serif">2 &#183; Thunder SFX</text>')
    s.append(f'<rect x="120" y="76" width="646" height="34" rx="6" fill="{Q["field"]}"/>')
    s.append(f'<text x="134" y="98" fill="{Q["muted"]}" font-size="15" font-family="Helvetica, Arial, sans-serif">Notes</text>')

    # table header
    hy = 124
    s.append(f'<rect x="0" y="{hy}" width="{Wq}" height="22" fill="{Q["hdr"]}"/>')
    cols = [(64, "Number", "middle"), (110, "Name", "start"), (470, "Target", "middle"),
            (545, "Pre-Wait", "middle"), (630, "Duration", "middle"), (715, "Post-Wait", "middle")]
    for cx, label, anc in cols:
        s.append(f'<text x="{cx}" y="{hy+16}" text-anchor="{anc}" fill="{Q["hdrtxt"]}" font-size="12.5" font-family="Helvetica, Arial, sans-serif">{label}</text>')

    # cue rows
    ry0, rh = hy + 22, 27
    for i, (num, name, dur) in enumerate(CUES):
        ry = ry0 + i * rh
        bg = Q["row"] if i % 2 == 0 else Q["rowalt"]
        s.append(f'<rect x="0" y="{ry}" width="{Wq}" height="{rh}" fill="{bg}"/>')
        if i == 0:
            s.append(f'<rect class="q-sel1" x="0" y="{ry}" width="{Wq}" height="{rh}" fill="{Q["sel"]}"/>')
            s.append(f'<rect class="q-playtint" x="0" y="{ry}" width="{Wq}" height="{rh}" fill="{Q["green"]}"/>')
            s.append(f'<rect class="q-playbar" x="0" y="{ry+rh-3}" width="0" height="3" fill="{Q["green"]}"/>')
            s.append(f'<rect class="q-panicrow" x="0" y="{ry}" width="{Wq}" height="{rh}" fill="{Q["red"]}"/>')
        if i == 1:
            s.append(f'<rect class="q-sel2" x="0" y="{ry}" width="{Wq}" height="{rh}" fill="{Q["sel"]}"/>')
        ty = ry + 19
        if i == 0:
            s.append(f'<path class="q-ph1" d="M4 {ry+7}l8 6-8 6z" fill="#101010"/>')
        if i == 1:
            s.append(f'<path class="q-ph2" d="M4 {ry+7}l8 6-8 6z" fill="#101010"/>')
        s.append(f'<g class="q-rowtx q-rowtx{i}" color="{Q["seltxt"] if i == 0 else Q["rowtxt"]}">'
                 + speaker_icon(26, ry + 6, "q-ic")
                 + f'<text x="64" y="{ty}" text-anchor="middle" font-size="14">{num}</text>'
                 + f'<text x="110" y="{ty}" font-size="14">{name}</text>'
                 + target_icon(470, ry + 13)
                 + f'<text x="545" y="{ty}" text-anchor="middle" font-size="13">00:00.00</text>'
                 + f'<text x="630" y="{ty}" text-anchor="middle" font-size="13">{dur}</text>'
                 + f'<text x="715" y="{ty}" text-anchor="middle" font-size="13">00:00.00</text>'
                 + '</g>')
    table_end = ry0 + len(CUES) * rh
    s.append(f'<rect x="0" y="{table_end}" width="{Wq}" height="{460-table_end}" fill="#121212"/>')

    # footer
    s.append(f'<rect x="0" y="460" width="{Wq}" height="30" fill="{Q["win"]}"/>')
    s.append('<rect x="12" y="466" width="88" height="19" rx="9" fill="#1c1c1e"/>')
    s.append('<rect x="14" y="467.5" width="42" height="16" rx="8" fill="#5a5a5f"/>')
    s.append('<text x="35" y="480" text-anchor="middle" fill="#fff" font-size="11" font-family="Helvetica, Arial, sans-serif">Edit</text>')
    s.append('<text x="78" y="480" text-anchor="middle" fill="#9a9a9e" font-size="11" font-family="Helvetica, Arial, sans-serif">Show</text>')
    s.append(f'<text x="{Wq/2}" y="480" text-anchor="middle" fill="#9a9a9e" font-size="12" font-family="Helvetica, Arial, sans-serif">6 cues in 1 cue list</text>')
    s.append(f'<circle cx="{Wq-24}" cy="475" r="7" fill="none" stroke="#9a9a9e" stroke-width="1.6"/>')
    s.append(f'<circle cx="{Wq-24}" cy="475" r="2.5" fill="#9a9a9e"/>')
    return "\n  ".join(s)

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

/* QLab window: standby moves 1 -> 2 on GO, back at loop end */
@keyframes k-qsel1 {{ 0%,34% {{opacity:1}} 35%,97% {{opacity:0}} 98%,100% {{opacity:1}} }}
@keyframes k-qsel2 {{ 0%,34% {{opacity:0}} 35%,97% {{opacity:1}} 98%,100% {{opacity:0}} }}
.q-sel1, .q-ph1, .q-std1 {{ animation: k-qsel1 {DUR} step-end infinite; }}
.q-sel2, .q-ph2, .q-std2 {{ opacity:0; animation: k-qsel2 {DUR} step-end infinite; }}
.q-rowtx text {{ fill: currentColor; }}
.q-rowtx .q-ic path {{ fill: currentColor; stroke: currentColor; }}
.q-rowtx circle {{ stroke: currentColor; fill: none; }}
.q-rowtx circle + circle {{ fill: currentColor; stroke: none; }}
@keyframes k-rowtx0 {{ 0%,34% {{color:{Q["seltxt"]}}} 35%,97% {{color:{Q["rowtxt"]}}} 98%,100% {{color:{Q["seltxt"]}}} }}
@keyframes k-rowtx1 {{ 0%,34% {{color:{Q["rowtxt"]}}} 35%,97% {{color:{Q["seltxt"]}}} 98%,100% {{color:{Q["rowtxt"]}}} }}
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
<text x="800" y="58" text-anchor="middle" fill="#e8eef7" font-size="34" font-weight="bold">GO-GO &#183; one-button show control</text>
<text x="800" y="772" text-anchor="middle" fill="#5d6c80" font-size="22">1 click = GO &#160;&#183;&#160; hold 1.4s = PANIC &#160;&#183;&#160; WiFi&#8209;OSC / BLE keyboard / LoRa</text>

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
    {layer("boot")}{layer("go")}{layer("gosent")}{layer("panic")}
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
