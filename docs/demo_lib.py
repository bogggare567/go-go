"""Shared rendering pipeline for GO-GO's demo SVGs.

Extracted from the original generate_demo.py (hero animation) so the same
pixel-exact OLED renderer, board/Mac photo embedding, and QLab-mockup window
can be reused across scenario animations (docs/generate_scenario_demos.py)
without copy-pasting the whole thing.

Nothing here changes what generate_demo.py used to draw — same font, same
FB class, same QLab window markup — this is a pure extraction.
"""

import re, os, base64, subprocess, urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.path.join(REPO, "docs", ".demo_cache")
os.makedirs(TMP, exist_ok=True)

W, H = 128, 64
OLED = "#e6f3ff"


def load_font():
    font_path = os.path.join(TMP, "glcdfont.c")
    if not os.path.exists(font_path):
        urllib.request.urlretrieve(
            "https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/glcdfont.c",
            font_path)
    src = open(font_path).read()
    return [int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", src)]


FONT = load_font()


def cached_macbook():
    """Downscaled macbook.png, generated with macOS `sips` on first run.
    If the cache already exists (checked into docs/.demo_cache or produced by
    a previous run), reuse it — lets this pipeline run on Linux too."""
    mac_small = os.path.join(TMP, "macbook_s.png")
    if not os.path.exists(mac_small):
        subprocess.run(["sips", "-Z", "1400", f"{REPO}/img/macbook.png", "--out", mac_small],
                       check=True, capture_output=True)
    return mac_small


def cached_brand_font():
    # Unbounded (same as soundkorb.ru), OFL license, latin subset.
    brand_font = os.path.join(TMP, "unbounded-latin.woff2")
    if not os.path.exists(brand_font):
        urllib.request.urlretrieve(
            "https://fonts.gstatic.com/s/unbounded/v12/Yq6W-LOTXCb04q32xlpwu8ZfvRIkSQ.woff2",
            brand_font)
    return brand_font


def b64_file(path):
    return base64.b64encode(open(path, "rb").read()).decode()


# ============================ OLED framebuffer ==============================
class FB:
    def __init__(self, fill=0):
        self.px = [[fill] * W for _ in range(H)]

    def set(self, x, y, c=1):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = c

    def fill_rect(self, x, y, w, h, c=1):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.set(xx, yy, c)

    def draw_rect(self, x, y, w, h, c=1):
        for xx in range(x, x + w):
            self.set(xx, y, c); self.set(xx, y + h - 1, c)
        for yy in range(y, y + h):
            self.set(x, yy, c); self.set(x + w - 1, yy, c)

    def draw_line(self, x0, y0, x1, y1, c=1):
        # Simple Bresenham — enough for the thin glyph strokes used here.
        dx, dy = abs(x1 - x0), abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy
        x, y = x0, y0
        while True:
            self.set(x, y, c)
            if x == x1 and y == y1:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy; x += sx
            if e2 < dx:
                err += dx; y += sy

    def draw_circle(self, cx, cy, r, c=1):
        x, y, d = r, 0, 1 - r
        while x >= y:
            for (px, py) in [(x, y), (y, x), (-y, x), (-x, y), (-x, -y), (-y, -x), (y, -x), (x, -y)]:
                self.set(cx + px, cy + py, c)
            y += 1
            if d < 0:
                d += 2 * y + 1
            else:
                x -= 1
                d += 2 * (y - x) + 1

    def draw_char(self, x, y, ch, size, c):
        code = ord(ch)
        for i in range(5):
            line = FONT[code * 5 + i]
            for j in range(8):
                if line & 1:
                    if size == 1:
                        self.set(x + i, y + j, c)
                    else:
                        self.fill_rect(x + i * size, y + j * size, size, size, c)
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
                    while x < W and self.px[y][x]:
                        x += 1
                    parts.append(f"M{x0} {y}h{x - x0}v1h-{x - x0}z")
                else:
                    x += 1
        return "".join(parts)


def layer(paths, name):
    """SVG <g> for one OLED frame's pixels — visibility is toggled by CSS
    animation classes the caller defines (scr-<name>)."""
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
    s.append(f'<path d="M0 10a10 10 0 0 1 10-10h{Wq-20}a10 10 0 0 1 10 10v16H0z" fill="{Q["title"]}"/>')
    s.append('<circle cx="18" cy="13" r="6" fill="#ff5f57"/><circle cx="38" cy="13" r="6" fill="#febc2e"/><circle cx="58" cy="13" r="6" fill="#28c840"/>')
    s.append(f'<text x="{Wq/2}" y="18" text-anchor="middle" fill="{Q["titletxt"]}" font-size="13" font-family="Helvetica, Arial, sans-serif">My Show.qlab5 &#8212; Cue Lists</text>')

    s.append(f'<rect x="14" y="38" width="92" height="74" rx="8" fill="{Q["gofill"]}" stroke="{Q["gostroke"]}" stroke-width="3"/>')
    s.append(f'<rect class="q-goflash" x="14" y="38" width="92" height="74" rx="8" fill="{Q["green"]}"/>')
    s.append(f'<rect class="q-panicflash" x="14" y="38" width="92" height="74" rx="8" fill="{Q["red"]}"/>')
    s.append('<text x="60" y="90" text-anchor="middle" fill="#fff" font-size="42" font-weight="600" font-family="Helvetica, Arial, sans-serif">GO</text>')

    s.append(f'<rect x="120" y="38" width="646" height="30" rx="6" fill="{Q["field"]}"/>')
    s.append(f'<text class="q-std1" x="134" y="59" fill="{Q["fieldtxt"]}" font-size="16" font-family="Helvetica, Arial, sans-serif">1 &#183; Intro music</text>')
    s.append(f'<text class="q-std2" x="134" y="59" fill="{Q["fieldtxt"]}" font-size="16" font-family="Helvetica, Arial, sans-serif">2 &#183; Thunder SFX</text>')
    s.append(f'<rect x="120" y="76" width="646" height="34" rx="6" fill="{Q["field"]}"/>')
    s.append(f'<text x="134" y="98" fill="{Q["muted"]}" font-size="15" font-family="Helvetica, Arial, sans-serif">Notes</text>')

    hy = 124
    s.append(f'<rect x="0" y="{hy}" width="{Wq}" height="22" fill="{Q["hdr"]}"/>')
    cols = [(64, "Number", "middle"), (110, "Name", "start"), (470, "Target", "middle"),
            (545, "Pre-Wait", "middle"), (630, "Duration", "middle"), (715, "Post-Wait", "middle")]
    for cx, label, anc in cols:
        s.append(f'<text x="{cx}" y="{hy+16}" text-anchor="{anc}" fill="{Q["hdrtxt"]}" font-size="12.5" font-family="Helvetica, Arial, sans-serif">{label}</text>')

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

    s.append(f'<rect x="0" y="460" width="{Wq}" height="30" fill="{Q["win"]}"/>')
    s.append('<rect x="12" y="466" width="88" height="19" rx="9" fill="#1c1c1e"/>')
    s.append('<rect x="14" y="467.5" width="42" height="16" rx="8" fill="#5a5a5f"/>')
    s.append('<text x="35" y="480" text-anchor="middle" fill="#fff" font-size="11" font-family="Helvetica, Arial, sans-serif">Edit</text>')
    s.append('<text x="78" y="480" text-anchor="middle" fill="#9a9a9e" font-size="11" font-family="Helvetica, Arial, sans-serif">Show</text>')
    s.append(f'<text x="{Wq/2}" y="480" text-anchor="middle" fill="#9a9a9e" font-size="12" font-family="Helvetica, Arial, sans-serif">6 cues in 1 cue list</text>')
    s.append(f'<circle cx="{Wq-24}" cy="475" r="7" fill="none" stroke="#9a9a9e" stroke-width="1.6"/>')
    s.append(f'<circle cx="{Wq-24}" cy="475" r="2.5" fill="#9a9a9e"/>')
    return "\n  ".join(s)
