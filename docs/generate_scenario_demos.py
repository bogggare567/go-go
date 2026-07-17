#!/usr/bin/env python3
"""GO-GO scenario animations: BLE/HID and LoRa TX+RX pair.

Companion to docs/generate_demo.py (the OSC/WiFi hero animation) — implements
the two animations planned in docs/SCENARIOS.md's "План анимаций по
сценариям" table (scenario-ble.svg, scenario-lora.svg). Shares the OLED
renderer, board/Mac photo embedding and QLab mockup with the hero animation
via docs/demo_lib.py, so OLED screens here are pixel-exact renders of the
same firmware draw functions (ui_screens.cpp: drawGoScreen/drawGoSent/
drawPanicSent share layout across modes — only status text and the chrome
icon at top-right differ, which is exactly what changes below).

Usage: python3 docs/generate_scenario_demos.py  (run from the repo root)
Needs the same cached assets as generate_demo.py (img/heltec.png,
img/macbook.png, docs/.demo_cache/*).
"""

from demo_lib import REPO, FB, layer, qlab_window, Q, b64_file, cached_macbook, cached_brand_font

b64_board = b64_file(f"{REPO}/img/heltec.png")
b64_mac = b64_file(cached_macbook())
b64_font = b64_file(cached_brand_font())

DUR = "12s"

# Shared timeline (matches the hero animation's rhythm):
#   0-12%   boot            35-46%  GO just sent
#  13-32%   idle, ready      48-60%  idle, ready (again)
#           (click at ~33%)  63-86%  PANIC sent (hold at ~61-66%)
#                             88-100% idle, ready (loop closes into boot)
STYLE_TIMELINE = f'''
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
'''


# Button position on the *unscaled* 800x800 board photo, derived from the
# hero animation's absolute press-ring placement (106,470) at its
# translate(0,250) scale(0.78): local = ((106-0)/0.78, (470-250)/0.78).
BUTTON_LOCAL = (135.9, 282.1)


def board_group(paths, tx, ty, scale, show_press=False):
    """One Heltec board photo + its OLED overlay layers + status LED.
    show_press draws the click/hold ring over the physical button — leave it
    off for a board that only *receives* (the LoRa gateway reacts to the
    paired remote, it doesn't get its own press animation)."""
    g = [f'<g transform="translate({tx},{ty}) scale({scale})">']
    g.append(f'<image href="data:image/png;base64,{b64_board}" width="800" height="800"/>')
    g.append(f'<g transform="translate(341,275) scale(2.20,2.31)">')
    g.append(layer(paths, "boot") + layer(paths, "go") + layer(paths, "gosent") + layer(paths, "panic"))
    g.append('</g>')
    g.append('<circle class="led" cx="195" cy="478" r="14" fill="#ffd76a" opacity="0"/>')
    g.append('<circle class="led" cx="195" cy="478" r="26" fill="#ffd76a" opacity="0" style="filter:blur(6px)"/>')
    g.append('</g>')
    if show_press:
        bx, by = BUTTON_LOCAL
        px, py = tx + bx * scale, ty + by * scale
        r = 34 * scale / 0.78
        g.append(f'<circle class="press" style="transform-origin:{px:.0f}px {py:.0f}px; transform-box: view-box;" '
                  f'cx="{px:.0f}" cy="{py:.0f}" r="{r:.0f}" fill="none" stroke="#7fb4ff" stroke-width="5"/>')
    return "\n".join(g)


def chrome_battery(fb):
    # drawPowerIcon(2,2), battery ~75% — same on every screen regardless of mode.
    fb.draw_rect(2, 2, 10, 5); fb.fill_rect(12, 3, 2, 3); fb.fill_rect(3, 3, 6, 3)


def chrome_ble(fb):
    chrome_battery(fb)
    # drawConnectionIndicator(106,0) -> drawBLEIndicator(110,0), connected glyph
    x, y = 110, 0
    fb.draw_line(x + 3, y,     x + 3, y + 8)
    fb.draw_line(x + 3, y,     x + 6, y + 2)
    fb.draw_line(x + 6, y + 2, x,     y + 6)
    fb.draw_line(x,     y + 2, x + 6, y + 6)
    fb.draw_line(x + 6, y + 6, x + 3, y + 8)


def chrome_lora(fb, label):
    chrome_battery(fb)
    # drawConnectionIndicator(106,0) -> drawLoRaIndicator(104,0): "TX"/"RX" text + ok dot
    fb.text(104, 1, label, 1, 1)
    fb.fill_rect(120, 3, 3, 3)  # filled "link OK" dot (approximating fillCircle at this scale)


# ============================================================================
# Scenario B — BLE / HID keyboard (single board + Mac showing the Bluetooth
# pairing + the keystroke reaching the focused app)
# ============================================================================
def build_ble():
    boot = FB(fill=1); boot.text(10, 24, "soundkorb", 2, 0)

    go = FB(); chrome_ble(go)
    go.centered("BLE OK", 2, 1)
    go.fill_rect(34, 14, 60, 32, 1); go.text(46, 18, "GO", 3, 0)
    go.centered("soundkorb.ru", 54, 1)

    gosent = FB(); chrome_ble(gosent)
    gosent.draw_rect(34, 12, 60, 28, 1); gosent.text(46, 14, "GO", 3, 1)
    gosent.centered("SENT BLE", 50, 1)

    panic = FB(); chrome_ble(panic)
    panic.text(19, 12, "PANIC", 3, 1)
    panic.centered("SENT BLE", 48, 1)

    paths = {k: fb.runs_path() for k, fb in
             {"boot": boot, "go": go, "gosent": gosent, "panic": panic}.items()}

    board = board_group(paths, 0, 250, 0.78, show_press=True)

    # Compact "macOS Bluetooth settings + keyboard" mock instead of the full
    # QLab window — BLE mode is keyboard emulation, not QLab-aware, so the
    # story here is: paired once in System Settings, then keystrokes land in
    # whatever app has focus (the README/SCENARIOS.md warning about that is
    # echoed in the caption).
    win_w, win_h = 700, 430
    panel = [f'<g transform="translate(700,150)">']
    panel.append(f'<rect width="{win_w}" height="{win_h}" rx="14" fill="{Q["win"]}"/>')
    panel.append(f'<path d="M0 14a14 14 0 0 1 14-14h{win_w-28}a14 14 0 0 1 14 14v18H0z" fill="{Q["title"]}"/>')
    panel.append('<circle cx="22" cy="17" r="6" fill="#ff5f57"/><circle cx="42" cy="17" r="6" fill="#febc2e"/><circle cx="62" cy="17" r="6" fill="#28c840"/>')
    panel.append(f'<text x="{win_w/2}" y="23" text-anchor="middle" fill="{Q["titletxt"]}" font-size="14" font-family="Helvetica, Arial, sans-serif">Bluetooth</text>')

    # device row
    panel.append(f'<rect x="24" y="50" width="{win_w-48}" height="60" rx="10" fill="{Q["field"]}"/>')
    panel.append('<circle cx="56" cy="80" r="16" fill="#3a3a3d"/>')
    panel.append('<text x="56" y="86" text-anchor="middle" fill="#8ab4ff" font-size="16" font-family="Helvetica, Arial, sans-serif">&#9776;</text>')
    panel.append(f'<text x="90" y="76" fill="{Q["fieldtxt"]}" font-size="17" font-family="Helvetica, Arial, sans-serif">GO-GO-4F9A2C</text>')
    panel.append(f'<text x="90" y="96" fill="{Q["muted"]}" font-size="12" font-family="Helvetica, Arial, sans-serif">Keyboard</text>')
    panel.append(f'<rect x="{win_w-160}" y="66" width="112" height="28" rx="14" fill="#1c3a24"/>')
    panel.append(f'<text x="{win_w-104}" y="85" text-anchor="middle" fill="{Q["green"]}" font-size="13" font-family="Helvetica, Arial, sans-serif">Connected</text>')

    panel.append(f'<text x="24" y="142" fill="{Q["muted"]}" font-size="13" font-family="Helvetica, Arial, sans-serif">Sends keystrokes to the focused app &#8212; keep QLab in focus during a show.</text>')

    # keyboard mock: a row of small keys, Space bar and Esc highlighted on GO/PANIC
    ky = 180
    panel.append(f'<rect x="24" y="{ky}" width="{win_w-48}" height="130" rx="10" fill="{Q["field"]}"/>')
    # Esc key (top-left of the mock)
    panel.append('<rect class="k-escflash" x="42" y="198" width="70" height="36" rx="6" fill="#3a3a3d"/>')
    panel.append('<rect class="k-escglow" x="42" y="198" width="70" height="36" rx="6" fill="#e5484d" opacity="0"/>')
    panel.append('<text x="77" y="221" text-anchor="middle" fill="#e8e8ea" font-size="13" font-family="Helvetica, Arial, sans-serif">esc</text>')
    # Space bar (wide key, bottom)
    panel.append(f'<rect x="42" y="250" width="{win_w-108}" height="36" rx="6" fill="#3a3a3d"/>')
    panel.append(f'<rect class="k-spaceglow" x="42" y="250" width="{win_w-108}" height="36" rx="6" fill="#2fbf5b" opacity="0"/>')
    panel.append(f'<text x="{(win_w-24)/2}" y="273" text-anchor="middle" fill="#e8e8ea" font-size="13" font-family="Helvetica, Arial, sans-serif">space</text>')

    # "active app" badge that flashes with each keystroke
    panel.append(f'<rect class="k-appflash" x="24" y="{ky}" width="{win_w-48}" height="130" rx="10" fill="#2fbf5b" opacity="0"/>')
    panel.append(f'<rect class="k-apppanic" x="24" y="{ky}" width="{win_w-48}" height="130" rx="10" fill="#e5484d" opacity="0"/>')
    panel.append('</g>')

    style = STYLE_TIMELINE + f'''
@keyframes k-spaceflash {{ 0%,32% {{opacity:0}} 33.5%,37% {{opacity:.9}} 40%,100% {{opacity:0}} }}
.k-spaceglow {{ animation: k-spaceflash {DUR} linear infinite; }}
@keyframes k-escflash {{ 0%,60% {{opacity:0}} 61%,66% {{opacity:.9}} 71%,100% {{opacity:0}} }}
.k-escglow {{ animation: k-escflash {DUR} linear infinite; }}
.k-appflash {{ animation: k-spaceflash {DUR} linear infinite; }}
.k-apppanic {{ animation: k-escflash {DUR} linear infinite; }}
'''

    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1500 750" font-family="Menlo, Consolas, monospace">
<style>{style}</style>
<rect width="1500" height="750" rx="24" fill="#101826"/>
<text x="750" y="55" text-anchor="middle" fill="#e8eef7" font-size="27" font-weight="700" font-family="Unbounded, Menlo, sans-serif">GO-GO &#183; BLE keyboard mode</text>
<text x="750" y="720" text-anchor="middle" fill="#5d6c80" font-size="15" font-family="Unbounded, Menlo, sans-serif">1 click = Space (GO) &#160;&#183;&#160; hold 1.4s = Esc (PANIC) &#160;&#183;&#160; works with anything that takes a keypress</text>
{board}
{"".join(panel)}
</svg>
'''
    out = f"{REPO}/img/scenario-ble.svg"
    open(out, "w").write(svg)
    print(f"wrote {out}: {len(svg)/1024:.0f} KB")


# ============================================================================
# Scenario C/D — LoRa pair: TX (remote, in hand) + RX (gateway, by the
# computer) forwarding to OSC/QLab. RX's screen changes lag TX's by ~1% of
# the timeline to show the causality (remote sends -> gateway receives).
# ============================================================================
def build_lora():
    def tx_frames():
        boot = FB(fill=1); boot.text(10, 24, "soundkorb", 2, 0)
        go = FB(); chrome_lora(go, "TX")
        go.centered("TX OK 4f9a2c", 2, 1)
        go.fill_rect(34, 14, 60, 32, 1); go.text(46, 18, "GO", 3, 0)
        go.centered("soundkorb.ru", 54, 1)
        gosent = FB(); chrome_lora(gosent, "TX")
        gosent.draw_rect(34, 12, 60, 28, 1); gosent.text(46, 14, "GO", 3, 1)
        gosent.centered("SENT LORA", 50, 1)
        panic = FB(); chrome_lora(panic, "TX")
        panic.text(19, 12, "PANIC", 3, 1)
        panic.centered("SENT LORA", 48, 1)
        return {k: fb.runs_path() for k, fb in
                {"boot": boot, "go": go, "gosent": gosent, "panic": panic}.items()}

    def rx_frames():
        boot = FB(fill=1); boot.text(10, 24, "soundkorb", 2, 0)
        go = FB(); chrome_lora(go, "RX")
        go.centered("RX OSC OK", 2, 1)
        go.fill_rect(34, 14, 60, 32, 1); go.text(46, 18, "GO", 3, 0)
        go.centered("soundkorb.ru", 54, 1)
        gosent = FB(); chrome_lora(gosent, "RX")
        gosent.draw_rect(34, 12, 60, 28, 1); gosent.text(46, 14, "GO", 3, 1)
        gosent.centered("RX OUT", 50, 1)
        panic = FB(); chrome_lora(panic, "RX")
        panic.text(19, 12, "PANIC", 3, 1)
        panic.centered("RX OUT", 48, 1)
        return {k: fb.runs_path() for k, fb in
                {"boot": boot, "go": go, "gosent": gosent, "panic": panic}.items()}

    tx_paths = tx_frames()
    rx_paths = rx_frames()

    tx_board = board_group(tx_paths, -30, 300, 0.5, show_press=True)
    rx_board = board_group(rx_paths, 560, 300, 0.5, show_press=False)

    qlab = f'<g transform="translate(1060,160) scale(0.62)" font-family="Helvetica, Arial, sans-serif">{qlab_window()}</g>'

    # RX's own screen-change classes are reused from k-go/k-gosent/k-panic but
    # delayed ~1% to visualize "TX sends, then RX reacts a moment later".
    rx_delay_style = '''
@keyframes k-go-rx     { 0%,12% {opacity:0} 14%,33% {opacity:1} 35%,47% {opacity:0} 49%,61% {opacity:1} 63%,87% {opacity:0} 89%,100% {opacity:1} }
@keyframes k-gosent-rx { 0%,34% {opacity:0} 36%,47% {opacity:1} 49%,100% {opacity:0} }
@keyframes k-panic-rx  { 0%,62% {opacity:0} 64%,87% {opacity:1} 89%,100% {opacity:0} }
.rx .scr-go     { animation: k-go-rx     12s step-end infinite; }
.rx .scr-gosent { animation: k-gosent-rx 12s step-end infinite; }
.rx .scr-panic  { animation: k-panic-rx  12s step-end infinite; }
'''

    qlab_style = '''
/* QLab window: standby moves 1 -> 2 on GO, back at loop end (same rules as
   the hero animation's gogo-demo.svg, since this reuses the same mockup) */
@keyframes k-qsel1 { 0%,34% {opacity:1} 35%,97% {opacity:0} 98%,100% {opacity:1} }
@keyframes k-qsel2 { 0%,34% {opacity:0} 35%,97% {opacity:1} 98%,100% {opacity:0} }
.q-sel1, .q-ph1, .q-std1 { animation: k-qsel1 12s step-end infinite; }
.q-sel2, .q-ph2, .q-std2 { opacity:0; animation: k-qsel2 12s step-end infinite; }
.q-rowtx text { fill: currentColor; }
.q-rowtx .q-ic path { fill: currentColor; stroke: currentColor; }
.q-rowtx circle { stroke: currentColor; fill: none; }
.q-rowtx circle + circle { fill: currentColor; stroke: none; }
@keyframes k-rowtx0 { 0%,34% {color:#141414} 35%,97% {color:#e4e4e6} 98%,100% {color:#141414} }
@keyframes k-rowtx1 { 0%,34% {color:#e4e4e6} 35%,97% {color:#141414} 98%,100% {color:#e4e4e6} }
.q-rowtx0 { animation: k-rowtx0 12s step-end infinite; }
.q-rowtx1 { animation: k-rowtx1 12s step-end infinite; }

.q-goflash { opacity:0; animation: k-qgoflash 12s linear infinite; }
@keyframes k-qgoflash { 0%,32% {opacity:0} 33.5%,36% {opacity:.85} 40%,100% {opacity:0} }
.q-panicflash { opacity:0; animation: k-qpanicflash 12s linear infinite; }
@keyframes k-qpanicflash { 0%,60% {opacity:0} 62%,66% {opacity:.85} 71%,100% {opacity:0} }
.q-playtint { opacity:0; animation: k-qplaytint 12s step-end infinite; }
@keyframes k-qplaytint { 0%,34% {opacity:0} 35%,62% {opacity:.16} 63%,100% {opacity:0} }
.q-playbar { animation: k-qplaybar 12s linear infinite; }
@keyframes k-qplaybar { 0%,35% {width:0} 62% {width:640px} 63%,100% {width:0} }
.q-panicrow { opacity:0; animation: k-qpanicrow 12s linear infinite; }
@keyframes k-qpanicrow { 0%,61% {opacity:0} 63%,66% {opacity:.4} 71%,100% {opacity:0} }
'''

    style = STYLE_TIMELINE + rx_delay_style + qlab_style + '''
/* Направление читается движением, а не пульсом на месте:
   GO/PANIC — дуги летят от TX вправо к RX; ACK — от RX влево обратно к TX. */
@keyframes k-wave-go   { 0%,32% {opacity:0; transform:translateX(0)} 33.5% {opacity:.9} 40% {opacity:0; transform:translateX(85px)} 41%,100% {opacity:0; transform:translateX(0)} }
@keyframes k-wave-ack  { 0%,34% {opacity:0; transform:translateX(0)} 35.5% {opacity:.9} 42% {opacity:0; transform:translateX(-85px)} 43%,100% {opacity:0; transform:translateX(0)} }
@keyframes k-wave-panic{ 0%,61% {opacity:0; transform:translateX(0)} 62.5% {opacity:.9} 69% {opacity:0; transform:translateX(85px)} 70%,100% {opacity:0; transform:translateX(0)} }
@keyframes k-wave-ack2 { 0%,63% {opacity:0; transform:translateX(0)} 64.5% {opacity:.9} 71% {opacity:0; transform:translateX(-85px)} 72%,100% {opacity:0; transform:translateX(0)} }
.wave-go    { animation: k-wave-go    12s ease-out infinite; transform-box: view-box; }
.wave-go2   { animation-delay: .2s; }
.wave-panic { animation: k-wave-panic 12s ease-out infinite; transform-box: view-box; }
.wave-panic2{ animation-delay: .2s; }
.wave-ack   { animation: k-wave-ack   12s ease-out infinite; transform-box: view-box; }
.wave-ackB  { animation-delay: .2s; }
.wave-ack2  { animation: k-wave-ack2  12s ease-out infinite; transform-box: view-box; }
.wave-ack2B { animation-delay: .2s; }
'''

    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1700 800" font-family="Menlo, Consolas, monospace">
<style>{style}</style>
<rect width="1700" height="800" rx="24" fill="#101826"/>
<text x="850" y="55" text-anchor="middle" fill="#e8eef7" font-size="27" font-weight="700" font-family="Unbounded, Menlo, sans-serif">GO-GO &#183; LoRa remote + gateway</text>
<text x="850" y="770" text-anchor="middle" fill="#5d6c80" font-size="15" font-family="Unbounded, Menlo, sans-serif">TX in hand &#160;&#8594;&#160; LoRa (auto clean channel) &#160;&#8594;&#160; RX gateway &#160;&#8594;&#160; OSC to QLab (or a BLE keypress)</text>

<text x="230" y="270" text-anchor="middle" fill="#9fb0c8" font-size="16" font-family="Menlo, sans-serif">TX &#183; remote (in hand)</text>
<text x="800" y="270" text-anchor="middle" fill="#9fb0c8" font-size="16" font-family="Menlo, sans-serif">RX &#183; gateway (by the computer)</text>

<!-- GO летит TX -> RX (зелёный, вправо); PANIC — красный туда же; ACK возвращается RX -> TX (синий, влево) -->
<g stroke="#6fd18c" stroke-width="5" fill="none">
  <path class="wave-go" d="M420 465 a55 55 0 0 1 0 90"/>
  <path class="wave-go wave-go2" d="M420 450 a75 75 0 0 1 0 120"/>
</g>
<g stroke="#e5484d" stroke-width="5" fill="none">
  <path class="wave-panic" d="M420 465 a55 55 0 0 1 0 90"/>
  <path class="wave-panic wave-panic2" d="M420 450 a75 75 0 0 1 0 120"/>
</g>
<g stroke="#7fb4ff" stroke-width="4" fill="none">
  <path class="wave-ack" d="M540 465 a55 55 0 0 0 0 90"/>
  <path class="wave-ack wave-ackB" d="M540 450 a75 75 0 0 0 0 120"/>
  <path class="wave-ack2" d="M540 465 a55 55 0 0 0 0 90"/>
  <path class="wave-ack2 wave-ack2B" d="M540 450 a75 75 0 0 0 0 120"/>
</g>

{tx_board}
<g class="rx">{rx_board}</g>
{qlab}
</svg>
'''
    out = f"{REPO}/img/scenario-lora.svg"
    open(out, "w").write(svg)
    print(f"wrote {out}: {len(svg)/1024:.0f} KB")


if __name__ == "__main__":
    build_ble()
    build_lora()
