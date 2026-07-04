# Demo video — shooting script

A short (2:30–3:30) demo video for the README/promotion posts. Goal: show
the actual device working in each mode, not just slides — the SVG
animations (`img/gogo-demo.svg`, `img/scenario-ble.svg`,
`img/scenario-lora.svg`, `img/scenario-setup.svg`) already cover the "how
it works" explanation
graphically; this video's job is to prove it's real hardware that really
works, in under 3 minutes.

**You'll need:** one GO-GO board minimum (two if you want to film the LoRa
segment — highly recommended, it's the most distinctive feature), a laptop
running QLab with a simple 2-cue test workspace, a phone for the BLE/OSC
setup shots, and a way to capture both a screen recording (QLab reacting)
and a physical shot of the board (a second camera or phone on a small
tripod, tight on the OLED).

## Shot list

**1. Cold open — the problem (0:00–0:15)**
Wide or medium shot: a console or booth mid-show, hands busy on faders.
Voiceover/caption: *"During a show, your hands are on the console — not
the laptop."* No need to stage an actual performance; a believable booth
setup with the laptop just out of comfortable reach makes the point.

**2. The object itself (0:15–0:25)**
Close-up, board in hand, thumb on the button. Caption: *"One button. Two
commands."* Click it once on camera (doesn't need to be connected yet) —
this is a product shot, not a demo yet.

**3. Mode 1 — OSC/WiFi into QLab (0:25–1:00)**
- Screen recording: QLab workspace with 2-3 cues loaded, OSC input enabled
  (Workspace Settings → Network), OSC target already configured on the
  board (skip the WiFi-setup portal in this cut — it's a one-time step,
  `docs/osc_setup.md` covers it in writing).
- Split-screen or cut between: board OLED close-up showing `OSC OK`, then
  click → `GO` / `SENT OSC` on the OLED, cut to QLab's cue list advancing
  in the same beat.
- Hold for PANIC → OLED shows `PANIC` / `SENT OSC` → QLab's workspace stops.
- Caption: *"Straight into QLab over WiFi — no app on the computer."*

**4. Mode 2 — BLE keyboard (1:00–1:25)**
- Quick cut: board OLED `BLE OK`, click → `GO` → cut to any app (even just
  a text editor) showing a space character or cursor move, to prove it's
  a literal keystroke, not magic. If you have a slide deck open (Keynote/
  PowerPoint), advancing a slide on click reads very clearly on camera.
- Caption: *"Zero setup — works with anything that takes a keypress."*
- Optional half-sentence: *"It's a real Bluetooth keyboard, so whatever's in
  focus gets the keystroke — keep your show software up front."*

**5. Mode 3 — LoRa pair (1:25–2:05)** *(skip this whole segment if you only
have one board — it's the hardest to fake convincingly, don't half-do it)*
- Wide shot: two boards, visibly apart (across a room, or better, from one
  room to another / across a parking lot if you want to sell the range
  story honestly — don't claim a distance on camera you haven't actually
  measured that day).
- Remote (TX) OLED close-up: `TX OK xxxxxx`.
- Click on the remote → cut to gateway (RX) OLED: `GO` / `RX OUT` → cut to
  QLab reacting (same as segment 3's QLab shot, different camera angle to
  signal "this is the same reaction, different transport").
- Caption: *"No WiFi needed — a dedicated radio link between the two."*

**6. Web panel (2:05–2:25)**
- Screen recording: phone or laptop browser open to the board's IP, show
  the live status page, the spectrum view moving, and the GO/PANIC buttons
  in the panel itself (tap one, cut back to the OLED reacting).
- Caption: *"Everything's also controllable from a browser — status,
  settings, live signal, firmware updates."*

**7. Outro / CTA (2:25–2:45)**
- Board on a table, maybe next to a music stand or belt clip to show scale.
- Caption: *"Open source, MIT licensed. Flash it yourself, or help build
  it."* Show the repo URL as on-screen text (don't rely on saying it aloud
  only — captions get read, audio doesn't always get heard).
- End card: repo URL + "docs/ISSUES.md" mention for anyone who wants to
  contribute.

## Notes on honesty

- Don't stage a "successful" LoRa range shot beyond what you've actually
  tested — if you haven't measured range at a real venue, don't imply a
  specific distance in the video. "No WiFi needed" is true and enough.
- The BLE segment's on-camera proof should be an actual keystroke landing
  somewhere visible (slide advance, cursor move, space character appearing)
  — a demo that just shows the OLED without the receiving end reacting
  doesn't prove anything to a skeptical viewer.
- If a take fails (missed click, wrong cue), just recut — better than
  implying a feature works when it glitched during filming.

## Captions vs. voiceover

Either works; captions are safer for a first cut (no need to write and
record a clean voiceover track, and it's how most people will watch this on
Reddit/Twitter anyway with sound off). If you do want narration, keep each
line to what's already in the captions above — don't add unverified claims
in the ad-lib.

## Export

- Landscape 16:9 for YouTube/README embed; a square or vertical crop of the
  same footage works for the Twitter thread and can be a second, shorter
  cut (30-45s, segments 2-3-5 only) rather than the full video.
- Once uploaded, drop the link into: `README.md`'s "Demo" section (if you
  add one), `promotion/reddit_techtheatre.md`, `promotion/reddit_esp32.md`,
  `promotion/twitter_thread.md`, and `promotion/habr_article.md` — they all
  currently have a `[DEMO VIDEO LINK]` placeholder.
