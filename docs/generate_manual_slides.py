#!/usr/bin/env python3
"""GO-GO manual slides: one big, zoomed, pixel-exact OLED render per key
screen, used by docs/PRESENTATION.md instead of tiny inline screenshots.

Reuses the same FB renderer as generate_demo.py / generate_scenario_demos.py
(docs/demo_lib.py) so every slide matches the real firmware draw functions
(ui_screens.cpp) pixel-for-pixel - these are not illustrations, they're the
actual framebuffer content rendered by the same code path.

Usage: python3 docs/generate_manual_slides.py  (run from the repo root)
"""

from demo_lib import REPO, FB, layer, b64_file, cached_brand_font

OUT_DIR = f"{REPO}/img/manual"
import os
os.makedirs(OUT_DIR, exist_ok=True)

SCALE = 6  # 128x64 -> 768x384, big enough to read every pixel clearly


def power_icon(fb, x, y):
    fb.draw_rect(x, y, 10, 5)
    fb.fill_rect(x + 10, y + 1, 2, 3)
    fb.fill_rect(x + 1, y + 1, 6, 3)


def wifi_bars(fb, x, y, bars):
    for i in range(4):
        h = 2 + i * 2
        c = 1 if i < bars else 0
        if c:
            fb.fill_rect(x + i * 4, y + 8 - h, 3, h)


def lora_dot(fb, x, y):
    fb.draw_circle(x + 4, y + 4, 4)
    fb.set(x + 4, y + 4)


# ============================================================================
# Screens (faithful to ui_screens.cpp; see docs/generate_manual_slides.py
# header for what this reuses)
# ============================================================================
screens = {}

boot = FB(fill=1)
boot.text(10, 24, "soundkorb", 2, 0)
screens["boot"] = boot

mode_select = FB()
mode_select.centered("SELECT MODE", 0, 1)
mode_select.fill_rect(0, 13, 128, 18, 1)
mode_select.centered("OSC / WIFI", 17, 1, c=0)
mode_select.centered("QLab over WiFi", 34, 1)
mode_select.centered("clean GO screen", 45, 1)
mode_select.centered("click next hold ok", 56, 1)
screens["mode_select"] = mode_select

go = FB()
power_icon(go, 2, 2)
wifi_bars(go, 106, 0, 4)
go.centered("OSC OK", 2, 1)
go.fill_rect(34, 14, 60, 32, 1)
go.text(46, 18, "GO", 3, 0)
go.centered("soundkorb.ru", 54, 1)
screens["go"] = go

go_sent = FB()
power_icon(go_sent, 2, 2)
wifi_bars(go_sent, 106, 0, 4)
go_sent.draw_rect(34, 12, 60, 28, 1)
go_sent.text(46, 14, "GO", 3, 1)
go_sent.centered("SENT OSC", 50, 1)
screens["go_sent"] = go_sent

panic_sent = FB()
power_icon(panic_sent, 2, 2)
wifi_bars(panic_sent, 106, 0, 4)
panic_sent.text(19, 12, "PANIC", 3, 1)
panic_sent.centered("SENT OSC", 48, 1)
screens["panic_sent"] = panic_sent

no_wifi = FB()
power_icon(no_wifi, 2, 2)
wifi_bars(no_wifi, 106, 0, 0)
no_wifi.centered("NO WIFI", 14, 2)
no_wifi.centered("click: retry setup", 40, 1)
no_wifi.centered("hold: menu", 52, 1)
screens["no_wifi"] = no_wifi

wifi_setup = FB()
wifi_setup.centered("WIFI SETUP", 0, 1)
wifi_setup.centered("Join WiFi:", 16, 1)
wifi_setup.centered("GO-GO-A1B2C3", 28, 1)
wifi_setup.centered("pass: password123", 40, 1)
wifi_setup.centered("192.168.4.1", 56, 1)
screens["wifi_setup"] = wifi_setup

menu = FB()
menu.centered("MENU", 0, 2)
items = ["Mode", "WiFi Setup", "Web Setup", "Status"]
y = 16
for i, label in enumerate(items):
    if i == 1:
        menu.fill_rect(0, y - 1, 128, 10, 1)
        menu.text(4, y, label, 1, 0)
    else:
        menu.text(4, y, label, 1, 1)
    y += 10
menu.centered("click next hold ok", 56, 1)
screens["menu"] = menu

lora_search = FB()
power_icon(lora_search, 2, 2)
lora_dot(lora_search, 106, 0)
lora_search.centered("SEARCH RX", 10, 2)
lora_search.centered("scan 868.10 MHz", 32, 1)
lora_search.centered("Pair if needed", 45, 1)
lora_search.centered("hold: menu", 55, 1)
screens["lora_search"] = lora_search

lora_link_ok = FB()
power_icon(lora_link_ok, 2, 2)
lora_dot(lora_link_ok, 106, 0)
lora_link_ok.centered("LINK OK", 10, 2)
lora_link_ok.centered("RX 4F21A0", 32, 1)
lora_link_ok.centered("RSSI -61 dBm", 45, 1)
lora_link_ok.centered("GO / 3x MENU", 55, 1)
screens["lora_link_ok"] = lora_link_ok

paths = {k: fb.runs_path() for k, fb in screens.items()}
b64_font = b64_file(cached_brand_font())

SLIDES = [
    ("01_boot", "boot", "Загрузка",
     "Заставка «soundkorb» — примерно секунда, потом плата сама предлагает "
     "выбрать режим (если он ещё не настроен) или сразу переходит на "
     "рабочий экран сохранённого режима."),
    ("02_mode_select", "mode_select", "Выбор режима",
     "Клик — следующий вариант (OSC/WiFi → BLE → LoRa TX → LoRa RX), "
     "удержание — выбрать. Полоса внизу показывает, сколько ещё держать."),
    ("03_go", "go", "Рабочий экран — GO",
     "Один клик отправляет команду GO (звук/кью). Вверху — статус связи "
     "текущего режима: тут «OSC OK», WiFi подключён и цель настроена."),
    ("04_go_sent", "go_sent", "После GO — подтверждение",
     "Экран показывает, что команда реально ушла, и через ~0.7 с "
     "возвращается на рабочий экран. Если этого окна не видно — "
     "разрядка не в логике экрана, а где-то в связи (см. раздел "
     "«диагностика» в docs/MANUAL.md)."),
    ("05_panic_sent", "panic_sent", "PANIC",
     "Удержание кнопки 1.4 с с рабочего экрана — стоп всему. Полоса "
     "прогресса на рабочем экране показывает время до срабатывания."),
    ("06_no_wifi", "no_wifi", "Нет сети",
     "Плата так и говорит: нет WiFi. Клик — короткая тихая попытка "
     "переподключиться (без порталов и долгих пауз), удержание — в меню."),
    ("07_wifi_setup", "wifi_setup", "WiFi Setup (WiFiManager)",
     "Меню → WiFi Setup поднимает точку доступа GO-GO-XXXXXX и открывает "
     "настоящий портал WiFiManager (перекрашенный под бренд): список "
     "сетей → пароль → цель OSC (IP/порт QLab) на той же странице."),
    ("08_menu", "menu", "Меню (3 клика)",
     "Три быстрых клика с любого рабочего экрана открывают меню: режим, "
     "подключение, панель, телеметрия, регион/частота и т.д."),
    ("09_lora_search", "lora_search", "LoRa — поиск",
     "Пульт ищет гейтвей по сетке частот региона (авто-режим). Найденную "
     "станцию видно по RSSI, удержание — привязка."),
    ("10_lora_link_ok", "lora_link_ok", "LoRa — связь есть",
     "После привязки — рабочий экран LoRa: партнёр, RSSI и то же самое "
     "GO/PANIC на одну кнопку."),
]

CARD_W, CARD_H = 128 * SCALE, 64 * SCALE
PAD = 40


def slide_svg(name, screen_key, title, caption):
    W = CARD_W + PAD * 2
    H = CARD_H + PAD * 2 + 190
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="-apple-system, 'Segoe UI', Roboto, sans-serif">
<style>
@font-face {{
  font-family: 'Unbounded';
  src: url(data:font/woff2;base64,{b64_font}) format('woff2');
  font-weight: 200 900;
}}
</style>
<rect width="{W}" height="{H}" rx="28" fill="#0b0e12"/>
<text x="{PAD}" y="66" fill="#f4f2ef" font-size="34" font-weight="600" font-family="Unbounded, sans-serif">{title}</text>
<g transform="translate({PAD},{100})">
  <rect width="{CARD_W}" height="{CARD_H}" rx="14" fill="#04283a"/>
  <rect x="10" y="10" width="{CARD_W-20}" height="{CARD_H-20}" rx="8" fill="#020c12"/>
  <g transform="translate(10,10) scale({SCALE})">
    {layer(paths, screen_key)}
  </g>
</g>
<foreignObject x="{PAD}" y="{100+CARD_H+30}" width="{CARD_W}" height="150">
  <div xmlns="http://www.w3.org/1999/xhtml" style="color:#b7c0c9;font-size:22px;line-height:1.5;font-family:-apple-system,'Segoe UI',Roboto,sans-serif">{caption}</div>
</foreignObject>
</svg>
'''
    return svg


for fname, screen_key, title, caption in SLIDES:
    svg = slide_svg(fname, screen_key, title, caption)
    out = f"{OUT_DIR}/{fname}.svg"
    open(out, "w").write(svg)
    print(f"wrote {out}: {len(svg)/1024:.0f} KB")
