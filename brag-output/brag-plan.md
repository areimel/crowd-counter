# Brag plan — Crowd Counter

## Context
You ran `/brag-slim` with no input from inside the crowd-counter repo, so this is a **Project** brag. You get a 20 s landscape video (1920×1080, 30 fps), a poster and share copy in `brag-output/`, which doesn't exist yet. Every intermediate file goes in `brag-output/work/`. I don't modify anything in the repo outside `brag-output/`.

## What I found
- **What it is:** a pocket StickS3 that estimates how many people are in a room by passively listening for phones and wearables (BLE adverts plus Wi-Fi probe requests). It shows one big number and whether the crowd has passed X.
- **Who it's for:** a cover band that gets a bonus when attendance tops X, so it brings its own number to the settle-up.
- **Hook / funniest truth:** "The band gets paid if 120 people show up. Who's counting?"
- **What sets it apart:** it only receives. Addresses are salted and hashed on receipt, and the logs hold counts only. It is honest that the number is an estimate and not a certified count.
- **Identity, using real tokens:** `web/src/styles/tokens.css` and `lib/display/src/Theme.h` give bg #141310, panel #1c1b17, ink #efebe0, dim #a39e90, line #39372f, accent #ff5b1a, and status grey #4a4840 / amber #e09a10 / green #16a34a. The type is mono (`ui-monospace`; DejaVu Sans Mono or Noto Sans Mono locally) with uppercase wide tracking (0.16em).
- **Real UI to show:** the count screen from `lib/display/src/Ui.cpp` `drawCount` and `Widgets.cpp`. I port that layout to an HTML canvas at its native 135×240, then scale it up with pixelated rendering. It keeps the same y-constants (BIG_Y 26, EST_Y 126, STATUS_Y 176, SMALL_Y 214), pulse dots, battery, status bar text ("BELOW 120" / "ABOVE 120"), "X=120 PK…", the green alert flash with dark digits, a 7-segment big number and a 6×8 bitmap label font. The CSV header lines come from `NightLogFormat.h` (`# crowd-counter night=…`, `minute,epoch,ble_phone,…,estimate,smoothed`).
- **Copy reused:** the tagline "A pocket attendance estimate for the settle-up" and the feature lines "One number at a glance", "Receive only" and "An estimate, not a count".
- **Honesty:** I won't claim accuracy or battery runtime. The counts on screen are illustrative demo values, and the CSV rows use placeholder numbers. No personal data or repo URL goes on screen.

## Storyboard: 120 BPM (beat 0.5 s), every cut on a beat, 20.0 s total
| # | Time | Scene |
|---|---|---|
| 1 Hook | 0.0–4.0 | Words land on 16th-note plucks: "The band gets paid" / "if **120** people show up." Bell hit at 2.5: "Who's counting?" |
| 2 Reveal | 4.0–7.0 | Device slides up showing WARMING UP; "Crowd Counter" + tagline. |
| 3 Listen | 7.0–10.5 | Rings and phone/watch glyphs; count 34 → 118, grey → amber "BELOW 120". |
| 4 Payoff | 10.5–13.0 | Riser 10.0, chime 10.5: green flash "ABOVE 120"; "Green means pay up." |
| 5 Log | 13.0–16.5 | Terminal types `export`; BEGIN, header, minute rows, alert event, END. |
| 6 Outro | 16.5–20.0 | Device left, "Your own number at the settle-up." + honesty line. |

(Re-timed from the first draft so every line meets ~0.3 s/word.)

Transitions stagger (old content out, then new content in) or dip through the bg, with no muddy crossfades. Every line I want read stays on screen at about 0.3 s per word or longer.

**Poster / frame 0:** t = 11.4 s (green flash, caption settled).

## Build
1. `brag-output/brag-plan.md`: the plan above, written out.
2. `work/index.html`: one self-contained page. `window.renderAt(t)` draws every frame as a pure function of t (DOM transforms plus the device canvas). It inlines tokens.css and waits for `document.fonts.ready`.
3. `work/capture.mjs`: playwright-core drives the system `/usr/bin/google-chrome` headless. It renders 600 frames to PNG, all in one RGB format. This needs a one-off `npx -y playwright-core` fetch into the npx cache. It leaves the system and the project untouched, and I'll report it.
4. `work/music.py`: stdlib Python (no numpy) synthesizes a 120 BPM A-minor track: soft kick, hats, sub bass, a pluck motif, a gentle chime at 10.0 s in key, and quiet ticks under the cuts. Everything sits on the same beat grid from frame 0 with a simple mix and limiter, written out as WAV.
5. ffmpeg: frames + WAV → `brag.mp4` (H.264 yuv420p, AAC). The poster goes to `brag.jpg` and replaces frame 0 with no added frame.
6. `share-copy.txt`: 1–3 specific sentences.

## Verification
- Before the full render, take stills of every scene and of each transition at times taken from the timeline (3.0, 6.0, 10.0, 13.0, 16.5 ± 2 frames). I check them for overflow, collisions and contrast, and fix what I find.
- After the render, `ffprobe` should show 600 frames, 20.0 s and an audio stream. I extract frames at 1.5 s, 8 s, 11 s, 15 s and 19 s and confirm each matches its scene.
- I listen-check the audio by inspecting peak and RMS levels with ffmpeg `volumedetect` to make sure nothing clips.
