#!/usr/bin/env python3
"""GO-GO scenario animation: WiFi Setup on the board + the onboarding page on
a phone, ending in the web panel. Documents the v16.15 onboarding redesign
(docs/SCENARIOS.md scenario E) and doubles as promo/video-instruction
material — same pixel-exact-OLED approach as generate_demo.py /
generate_scenario_demos.py, sharing assets via docs/demo_lib.py.

Usage: python3 docs/generate_setup_demo.py  (run from the repo root)
Needs the same cached assets as the other generators (img/heltec.png,
docs/.demo_cache/*font). No Mac photo needed here — the phone body is drawn
in pure SVG to match this scene's own web-panel palette (see web_html.h /
web_html_wifi.h: bg #0b0e12, card #12161c, accent #5cb2a2).
"""

from demo_lib import REPO, FB, layer, b64_file, cached_brand_font

b64_board = b64_file(f"{REPO}/img/heltec.png")
b64_font = b64_file(cached_brand_font())

DUR = "12s"

# ============================================================================
# OLED: the board's own WIFI SETUP screen (drawWebSetup, AP branch) —
# see ui_screens.cpp drawWebSetup(): title, "Join WiFi:", AP name, pass, click hint.
# ============================================================================
def chrome_battery(fb):
    fb.draw_rect(2, 2, 10, 5); fb.fill_rect(12, 3, 2, 3); fb.fill_rect(3, 3, 6, 3)

setup = FB()
chrome_battery(setup)
setup.centered("WIFI SETUP", 0, 1)
setup.centered("Join WiFi:", 14, 1)
setup.centered("GO-GO-3A7F42", 25, 1)
setup.centered("pass: password123", 36, 1)
setup.centered("click: exit", 56, 1)

paths = {"setup": setup.runs_path()}

# ============================================================================
# Palette matching the real web panel (web_html.h / web_html_wifi.h)
# ============================================================================
P = {
    "bg": "#0b0e12", "card": "#12161c", "line": "rgba(255,255,255,.12)",
    "tx": "#f4f2ef", "mut": "#88929c", "acc": "#5cb2a2", "ok": "#8ec9a8",
    "sel": "#d4b896", "red": "#e0564f",
}

NETWORKS = [("Studio-Main", 3, True), ("guest-wifi", 2, False), ("Backstage-5G", 1, True)]


def wifi_icon(x, y, level, cls=""):
    bars = []
    for i in range(4):
        h = 3 + i * 3
        on = i < level
        bars.append(f'<rect x="{x + i * 6}" y="{y + 12 - h}" width="4" height="{h}" rx="1" '
                    f'fill="{P["acc"] if on else "rgba(255,255,255,.15)"}"/>')
    return f'<g class="{cls}">' + "".join(bars) + '</g>'


def phone_frame(inner_svg):
    """iPhone-style body: rounded rect, side buttons, notch, screen clip."""
    return f'''
<g transform="translate(0,0)">
  <rect x="0" y="0" width="300" height="610" rx="46" fill="#1b1f26" stroke="#333844" stroke-width="2"/>
  <rect x="8" y="8" width="284" height="594" rx="38" fill="{P["bg"]}"/>
  <rect x="105" y="18" width="90" height="26" rx="13" fill="#05070a"/>
  <clipPath id="screenClip"><rect x="8" y="8" width="284" height="594" rx="38"/></clipPath>
  <g clip-path="url(#screenClip)">
    {inner_svg}
  </g>
  <rect x="120" y="596" width="60" height="5" rx="2.5" fill="#4a5058"/>
</g>'''


def status_bar():
    return f'''<text x="26" y="66" fill="{P["tx"]}" font-size="13" font-family="-apple-system,Helvetica,sans-serif">9:41</text>
<text x="274" y="66" text-anchor="end" fill="{P["tx"]}" font-size="13" font-family="-apple-system,Helvetica,sans-serif">&#128267; 100%</text>'''


def card(x, y, w, h, extra=""):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="14" fill="{P["card"]}" stroke="{P["line"]}" {extra}/>'


# --- Screen 1: scanning ------------------------------------------------------
scan_screen = f'''
{status_bar()}
<text x="26" y="110" fill="{P["tx"]}" font-size="26" font-family="'Unbounded',sans-serif" font-weight="600">GO<tspan fill="{P["acc"]}">-</tspan>GO</text>
<text x="26" y="132" fill="{P["mut"]}" font-size="13">&#1055;&#1086;&#1076;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;&#1080;&#1077; &#1082; WiFi</text>
{card(24, 156, 252, 210)}
<circle cx="48" cy="196" r="11" fill="none" stroke="{P["mut"]}" stroke-width="3" stroke-dasharray="26 60">
  <animateTransform attributeName="transform" type="rotate" from="0 48 196" to="360 48 196" dur="1s" repeatCount="indefinite"/>
</circle>
<text x="70" y="201" fill="{P["mut"]}" font-size="15">&#1048;&#1097;&#1091; &#1089;&#1077;&#1090;&#1080;&#1093;&#8230;</text>
'''

# --- Screen 2: network list ---------------------------------------------------
def net_rows():
    rows = []
    y0 = 156
    for i, (name, level, locked) in enumerate(NETWORKS):
        y = y0 + i * 58
        sel = ' class="net-hi"' if i == 0 else ''
        rows.append(f'<g{sel}>')
        rows.append(f'<rect x="24" y="{y}" width="252" height="58" fill="transparent"/>')
        if i > 0:
            rows.append(f'<line x1="24" y1="{y}" x2="276" y2="{y}" stroke="{P["line"]}"/>')
        rows.append(wifi_icon(38, y + 21, level))
        rows.append(f'<text x="78" y="{y+35}" fill="{P["tx"]}" font-size="17">{name}</text>')
        if locked:
            rows.append(f'<text x="252" y="{y+35}" fill="{P["mut"]}" font-size="15">&#128274;</text>')
        rows.append('</g>')
    return "".join(rows)

list_screen = f'''
{status_bar()}
<text x="26" y="110" fill="{P["tx"]}" font-size="26" font-family="'Unbounded',sans-serif" font-weight="600">GO<tspan fill="{P["acc"]}">-</tspan>GO</text>
<text x="26" y="132" fill="{P["mut"]}" font-size="13">&#1055;&#1086;&#1076;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;&#1080;&#1077; &#1082; WiFi</text>
{card(24, 156, 252, 3 * 58)}
{net_rows()}
<rect class="tap-ring" x="24" y="156" width="252" height="58" rx="10" fill="none" stroke="{P["acc"]}" stroke-width="3" opacity="0"/>
'''

# --- Screen 3: password entry --------------------------------------------------
pass_screen = f'''
{status_bar()}
<text x="26" y="110" fill="{P["tx"]}" font-size="26" font-family="'Unbounded',sans-serif" font-weight="600">GO<tspan fill="{P["acc"]}">-</tspan>GO</text>
{card(24, 140, 252, 190)}
<text x="42" y="172" fill="{P["tx"]}" font-size="17" font-weight="600">Studio-Main</text>
<rect x="42" y="192" width="216" height="42" rx="10" fill="#05070a" stroke="{P["line"]}"/>
<g class="pw-dots">
  <circle cx="60" cy="213" r="4" fill="{P["tx"]}"/><circle cx="74" cy="213" r="4" fill="{P["tx"]}"/>
  <circle cx="88" cy="213" r="4" fill="{P["tx"]}"/><circle cx="102" cy="213" r="4" fill="{P["tx"]}"/>
  <circle cx="116" cy="213" r="4" fill="{P["tx"]}"/><circle cx="130" cy="213" r="4" fill="{P["tx"]}"/>
</g>
<rect class="connect-btn" x="42" y="252" width="216" height="46" rx="10" fill="{P["acc"]}"/>
<text x="150" y="281" text-anchor="middle" fill="#0b0e12" font-size="15" font-weight="600" font-family="'Unbounded',sans-serif">&#1055;&#1086;&#1076;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</text>
'''

# --- Screen 4: connecting -> success -------------------------------------------
result_screen = f'''
{status_bar()}
<text x="26" y="110" fill="{P["tx"]}" font-size="26" font-family="'Unbounded',sans-serif" font-weight="600">GO<tspan fill="{P["acc"]}">-</tspan>GO</text>
{card(24, 140, 252, 150)}
<g class="connecting-msg">
  <circle cx="46" cy="180" r="10" fill="none" stroke="{P["mut"]}" stroke-width="3" stroke-dasharray="24 55">
    <animateTransform attributeName="transform" type="rotate" from="0 46 180" to="360 46 180" dur="1s" repeatCount="indefinite"/>
  </circle>
  <text x="66" y="185" fill="{P["sel"]}" font-size="14">&#1055;&#1086;&#1076;&#1082;&#1083;&#1102;&#1095;&#1072;&#1102;&#1089;&#1100;&#8230;</text>
</g>
<g class="ok-msg">
  <circle cx="46" cy="180" r="11" fill="{P["ok"]}"/>
  <path d="M40 180l4 4 8-8" stroke="#0b0e12" stroke-width="2.5" fill="none" stroke-linecap="round" stroke-linejoin="round"/>
  <text x="66" y="185" fill="{P["ok"]}" font-size="14" font-weight="600">&#1055;&#1086;&#1076;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;&#1086;!</text>
  <text x="42" y="215" fill="{P["tx"]}" font-size="15">http://gogo.local</text>
  <text x="42" y="238" fill="{P["mut"]}" font-size="12">&#1055;&#1077;&#1088;&#1077;&#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1072;&#8230;</text>
</g>
'''

# --- Screen 5: the actual panel, Status tab ------------------------------------
panel_screen = f'''
{status_bar()}
<rect x="0" y="86" width="300" height="44" fill="{P["card"]}"/>
<text x="26" y="112" fill="{P["tx"]}" font-size="15" font-weight="700">GO-GO-3A7F42</text>
<text x="274" y="112" text-anchor="end" fill="{P["ok"]}" font-size="12">v16.15</text>
<g font-family="'Unbounded',sans-serif" font-size="11">
  <rect x="16" y="140" width="66" height="28" rx="8" fill="none" stroke="{P["acc"]}"/>
  <text x="49" y="158" text-anchor="middle" fill="{P["tx"]}">Status</text>
  <text x="100" y="158" fill="{P["mut"]}">Settings</text>
  <text x="182" y="158" fill="{P["mut"]}">Spectrum</text>
  <text x="252" y="158" fill="{P["mut"]}">QLab</text>
</g>
<g class="panel-btns">
  <rect x="24" y="182" width="118" height="64" rx="12" fill="{P["acc"]}"/>
  <text x="83" y="222" text-anchor="middle" fill="#0b0e12" font-size="22" font-weight="700" font-family="'Unbounded',sans-serif">GO</text>
  <rect x="158" y="182" width="118" height="64" rx="12" fill="{P["red"]}"/>
  <text x="217" y="222" text-anchor="middle" fill="#fff" font-size="15" font-weight="700" font-family="'Unbounded',sans-serif">PANIC</text>
</g>
{card(24, 262, 252, 150)}
<text x="40" y="288" fill="{P["mut"]}" font-size="10" letter-spacing="1">MODE</text>
<text x="40" y="306" fill="{P["tx"]}" font-size="14">OSC / WiFi</text>
<text x="170" y="288" fill="{P["mut"]}" font-size="10" letter-spacing="1">WIFI</text>
<text x="170" y="306" fill="{P["ok"]}" font-size="14">192.168.1.42</text>
<text x="40" y="336" fill="{P["mut"]}" font-size="10" letter-spacing="1">OSC TARGET</text>
<text x="40" y="354" fill="{P["tx"]}" font-size="14">192.168.1.10:53000</text>
<text x="40" y="384" fill="{P["mut"]}" font-size="10" letter-spacing="1">BATTERY</text>
<text x="40" y="382" fill="{P["tx"]}" font-size="14" dy="18">87%</text>
'''

svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 900 700" font-family="-apple-system,Helvetica,sans-serif">
<style>
@font-face {{
  font-family: 'Unbounded';
  src: url(data:font/woff2;base64,{b64_font}) format('woff2');
  font-weight: 200 900;
}}
@keyframes k-setup {{ 0%,68% {{opacity:1}} 70%,100% {{opacity:0}} }}
@keyframes k-scan   {{ 0%,1% {{opacity:0}} 2%,15% {{opacity:1}} 17%,100% {{opacity:0}} }}
@keyframes k-list   {{ 0%,16% {{opacity:0}} 18%,30% {{opacity:1}} 32%,100% {{opacity:0}} }}
@keyframes k-pass   {{ 0%,31% {{opacity:0}} 33%,49% {{opacity:1}} 51%,100% {{opacity:0}} }}
@keyframes k-result {{ 0%,50% {{opacity:0}} 52%,69% {{opacity:1}} 71%,100% {{opacity:0}} }}
@keyframes k-panel  {{ 0%,70% {{opacity:0}} 72%,100% {{opacity:1}} }}
.scr-setup {{ animation: k-setup {DUR} step-end infinite; }}
.sc-scan   {{ animation: k-scan {DUR} step-end infinite; }}
.sc-list   {{ animation: k-list {DUR} step-end infinite; }}
.sc-pass   {{ animation: k-pass {DUR} step-end infinite; }}
.sc-result {{ animation: k-result {DUR} step-end infinite; }}
.sc-panel  {{ animation: k-panel {DUR} step-end infinite; }}

@keyframes k-tap {{ 0%,20% {{opacity:0; transform:scale(1)}} 21% {{opacity:.9; transform:scale(1)}} 25% {{opacity:0; transform:scale(1.08)}} 26%,100% {{opacity:0}} }}
.tap-ring {{ animation: k-tap {DUR} ease-out infinite; transform-box: fill-box; transform-origin: center; }}
.net-hi rect {{ fill: rgba(92,178,162,.10); }}

@keyframes k-dots {{ 0%,33% {{opacity:0}} 35% {{opacity:1}} 48%,100% {{opacity:1}} }}
.pw-dots {{ animation: k-dots {DUR} step-end infinite; }}
.pw-dots circle {{ opacity: 0; animation: k-dotpop 0.3s ease-out forwards; }}
.pw-dots circle:nth-child(1){{ animation-delay: 0.5s }} .pw-dots circle:nth-child(2){{ animation-delay: 0.65s }}
.pw-dots circle:nth-child(3){{ animation-delay: 0.8s }} .pw-dots circle:nth-child(4){{ animation-delay: 0.95s }}
.pw-dots circle:nth-child(5){{ animation-delay: 1.1s }} .pw-dots circle:nth-child(6){{ animation-delay: 1.25s }}
@keyframes k-dotpop {{ to {{opacity:1}} }}

@keyframes k-btnpress {{ 0%,44% {{transform:scale(1)}} 45% {{transform:scale(.96)}} 47%,100% {{transform:scale(1)}} }}
.connect-btn {{ animation: k-btnpress {DUR} linear infinite; transform-box: fill-box; transform-origin: center; }}

@keyframes k-connecting {{ 0%,51% {{opacity:1}} 60%,100% {{opacity:0}} }}
@keyframes k-ok {{ 0%,59% {{opacity:0}} 61%,100% {{opacity:1}} }}
.connecting-msg {{ animation: k-connecting {DUR} step-end infinite; }}
.ok-msg {{ opacity:0; animation: k-ok {DUR} step-end infinite; }}

@keyframes k-btnbounce {{ 0%,79% {{opacity:0; transform:scale(.7)}} 80% {{opacity:1; transform:scale(1.05)}} 84% {{transform:scale(1)}} 100% {{opacity:1; transform:scale(1)}} }}
.panel-btns {{ animation: k-btnbounce {DUR} ease-out infinite; transform-box: fill-box; transform-origin: center; }}
</style>

<rect width="900" height="700" rx="24" fill="#101826"/>
<text x="450" y="52" text-anchor="middle" fill="#e8eef7" font-size="28" font-weight="700" font-family="'Unbounded',sans-serif">WiFi Setup &#8594; Web Panel</text>
<text x="450" y="672" text-anchor="middle" fill="#5d6c80" font-size="15">&#1057;&#1082;&#1072;&#1085;&#1080;&#1088;&#1091;&#1077;&#1084; &#1089;&#1077;&#1090;&#1100; &#8594; &#1074;&#1074;&#1086;&#1076;&#1080;&#1084; &#1087;&#1072;&#1088;&#1086;&#1083;&#1100; &#8594; &#1087;&#1072;&#1085;&#1077;&#1083;&#1100; &#1086;&#1090;&#1082;&#1088;&#1099;&#1074;&#1072;&#1077;&#1090;&#1089;&#1103; &#1089;&#1072;&#1084;&#1072; (captive portal)</text>

<g transform="translate(70,150) scale(0.62)">
  <image href="data:image/png;base64,{b64_board}" width="800" height="800"/>
  <g transform="translate(341,275) scale(2.20,2.31)" class="scr-setup">
    {layer(paths, "setup")}
  </g>
</g>

<g transform="translate(430,60)">
  {phone_frame(f'<g class="sc-scan">{scan_screen}</g><g class="sc-list">{list_screen}</g>'
               f'<g class="sc-pass">{pass_screen}</g><g class="sc-result">{result_screen}</g>'
               f'<g class="sc-panel">{panel_screen}</g>')}
</g>
</svg>
'''

out = f"{REPO}/img/scenario-setup.svg"
open(out, "w").write(svg)
print(f"wrote {out}: {len(svg)/1024:.0f} KB")
