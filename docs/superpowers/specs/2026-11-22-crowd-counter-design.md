# Crowd Counter — design and build plan

## Context

`Crowd Counter — PRD.md` describes a StickS3 device that estimates attendance by
passively counting BLE adverts and Wi-Fi probe requests, so a cover band has its
own number at settle-up. The repo is a copy of an ESP32 template caught halfway
through a rework: the board layer, config generator, packaging, CI and web
flasher are usable; the macro-keyboard code in `include/`, `src/` and `tests/`
is dead (not compiled), and `README.md`/`AGENTS.md` describe an app that no
longer exists. There is no Wi-Fi, BLE-scan, filesystem, hashing or PSRAM code,
and nothing has been built in this copy.

## Decided with Alec (2026-11-22)

- **Scope:** FR-1..FR-12. Export is CSV over USB serial only (no Wi-Fi AP).
- **Allowlist:** setup mode captures a *baseline count* subtracted for the
  night, plus exact matches for fixed-address devices; re-capturable any time.
  A KEY2 hold shows "HOLD K2 AGAIN TO CAPTURE"; a second hold within 5 s
  captures, anything else cancels. After a capture the display shows WARMING UP
  for one smoothing period (60 s) and those samples are excluded from
  peak, time-above and alert.
- **Repo:** StickS3 only. Delete macro code, the HID/buttons/Wi-Fi/OTA config
  sections and the three GPIO boards. Keep config generator, packaging, CI and
  the web flasher, rewritten for Crowd Counter.
- **Threshold alert:** screen flash + chime, on by default, switchable in
  `config.yaml`.
- **Cold power-on:** if the open night has a log file, ask "New night / Resume"
  for 15 s; no press = new night. Starting a new night never deletes the
  previous one; retention keeps the newest 20 night logs and deletes the oldest
  beyond that or when free space drops below 64 KB.
- **Venue types shipped:** Bar / pub, Private event.
- **Dimming:** auto-dim 30 s after the last press; any press wakes.
- **Hardware:** StickS3 on `/dev/ttyACM0` (303a:1001), flashing authorised.
- **Working style:** up to 40 subagents in a mix of Opus (section leads),
  Sonnet (implementation) and Haiku (light work); parallel workstreams; a TODO
  list with per-agent assignments; DRY; files under ~300–400 lines; 15+
  descriptive commits. Nothing is pushed.

## Approaches considered

1. **Build on the template's Arduino/PlatformIO stack (chosen).** NimBLE passive
   scan + `esp_wifi` promiscuous RX, pure logic in host-tested libraries. Reuses
   the board layer, config generator, flasher and CI.
2. **ESP-IDF directly.** Finer coexistence control, but discards everything
   reusable and M5Unified's StickS3 support.
3. **BLE only for v1.** Fallback if the bench spike shows Wi-Fi capture cannot
   be made to work alongside BLE; FR-1 would then be reported as partly unmet.

## Design

### Data flow

```
RadioScheduler task: BLE scan ~2 s ─ stop ─ Wi-Fi dwell ~1 s on ch 1/6/11 ─ repeat
NimBLE cb ──┐ id = SHA-256(salt‖signal‖mac)[0..8] on the callback stack;
Wi-Fi cb ───┴► Observation queue (1024, drops counted) ─► DeviceTable (PSRAM)
                                         │ every 5 s
              Census: window 5 min · RSSI gate · − fixed-address allowlist
                      · − background · per-class counts
                                         │
              Estimator: class weights · − baseline · ÷ ratio · blend
                         · 60 s smoothing · warm-up · held peak · threshold
                 │                │                     │
              UiModel       NightLog (1 row/min)   flash + chime, once per night
```

### What the design review changed (verified against the pinned libraries)

- **The two radios must be time-sliced explicitly.** Espressif rates sniffing
  during a BLE scan as unstable, so a scheduler task alternates them. Slice
  lengths are set from bench measurements (Step 1), not guessed.
- **Baseline and allowlist are disjoint.** Baseline = weighted count of
  *rotating*-address devices per signal; allowlist = ids of *fixed*-address
  devices. A capture finalises once a full window of data exists, so you can
  press and walk away.
- **Background removal is narrow.** Only fixed-address, non-phone BLE devices
  present longer than `background_after_min` (default 90). Anything broader
  would drop patrons' phones on venue Wi-Fi.
- **No core dumps to flash.** The stock partition table writes crash stacks to
  flash, which could contain an address in flight. A custom table removes that
  partition and gives the space to the log filesystem.
- **Warm-up.** After boot, resume or re-capture the window is still filling;
  those samples are flagged, shown as warming up, and excluded from peak,
  minutes-above-X and the alert.
- **Night state survives a crash.** Salt, night id, minute index, peak state,
  alert latch, allowlist and baselines live in RTC memory (magic + CRC): kept
  across a reset, gone at power-off. After a cold resume only the aggregate
  baseline survives and the screen hints to re-run setup.
- **Clock.** M5Unified probes an RX8130 RTC on this board; use it if it answers,
  plus a serial `time <epoch>` command. Otherwise rows carry minutes since
  night start.
- **Tunable later from old logs.** The table stores marker bits and address
  kind, not a final class; each minute row logs per-class counts, both raw
  signals, battery mV and warm-up, and method changes mid-night are logged as
  event rows.
- **Battery.** 250 mAh cell; estimated 1.5–2.5 h with both radios receiving.
  5 h needs the power bank. Battery mV is logged every minute so runtime is
  measured, never claimed. Chime volume is capped to avoid a brownout.
- **Dropped from the PRD:** fingerprinting by advert interval (slicing makes it
  unreliable).

### Other calls I made (correct any of these)

- Ids are 64-bit; serial output never prints ids or addresses. The firmware
  only ever calls passive-scan and promiscuous-receive APIs (passive NimBLE
  scan; Wi-Fi in null mode). The prebuilt Arduino sdkconfig compiles all BLE
  roles in and the role-disable flags have no effect, so the transmit-capable
  stack is linked but never invoked; `scripts/check_receive_only.py` in CI
  forbids those APIs in `src/` and `lib/`.
- Privacy wording: raw addresses are never stored *by the application*. The
  radio drivers hold a received frame in RAM before our callback hashes it; no
  application-owned copy outlives the callback and nothing reaches flash,
  serial or a log. The salt is held only in RAM (RTC memory plus working
  copies), never written to flash or printed, and is regenerated at every cold
  power-on, every End night and once a night has run 18 hours. Ids are salted
  pseudonyms: while powered, salt and ids are in RAM together, so physical
  debug access to a running device could recover addresses by enumeration.
  Nothing persisted or emitted identifies a device.
- Alert: when it fires the device switches to the count screen, wakes the
  display, flashes and chimes. Confirmation beeps (setup captured, manual
  count saved) and the error tone (manual count rejected during warm-up)
  follow `alert.chime`.
- Count screen: the status bar always shows ABOVE n / BELOW n / WARMING UP; the
  small line reads like `X=120 PK151`; a log write failure shows LOG ERR.
  Summary: peak, minutes at or above X, start time (UTC, only when the clock
  is known), elapsed, X and venue; the end time is available from the serial
  `summary` command. Settings return to the count screen after 60 s idle.
- `export` / `summary` with no argument use the current night once it has a log
  (after its first minute); before that: `ERR current night has no log yet`.
- Device class is coarse (phone, wearable, audio, tracker, computer, other)
  with weights in `config.yaml`. Samsung and several Apple markers are
  ambiguous; those fall to "other".
- Blend default 0.8 BLE / 0.2 Wi-Fi; an unfitted Wi-Fi ratio gets zero weight.
- Calibration: (raw BLE, raw Wi-Fi, true count) pairs, ring of 16 per venue
  type, ratio = Σ(raw·true)/Σ(true²), clamped 0.3–5, persisted in NVS.
- Alert fires once per night when the smoothed estimate holds ≥ X for 30 s.
- Edit fields: KEY2 single = +step, KEY2 double-click = −step.
- Settings items: threshold X, venue type, RSSI gate, manual count, end night.
- Serial CLI: `help status nights export [n] summary time <epoch> shot`.
  `shot` dumps the screen so the host makes a PNG; this is how the UI is
  checked in-session.
- Night files on LittleFS, one per night, newest 20 kept (oldest deleted beyond
  that or when free space drops below 64 KB), written open-append-close once
  a minute.

### Screens (portrait 135×240, dark)

```
┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│ ▮▮▮▯   ●    │  │ DETAIL      │  │ NIGHT       │
│             │  │ BLE   187   │  │ PEAK   151  │
│    142      │  │ WIFI   96   │  │ ≥X   74 min │
│        est. │  │ RATIO 1.45  │  │ START 19:02 │
│█████████████│  │ BKGND  12   │  │ NOW   3:12  │
│ ABOVE 120   │  │ BASE    9   │  │ X     120   │
│ X=120 pk151 │  │ GATE  -85   │  │ BAR / PUB   │
└─────────────┘  └─────────────┘  └─────────────┘
 count            detail           summary
```
Status bar: grey < 80% of X, amber 80–99%, green ≥ X. Detail, summary and
settings are all drawn by one label/value row helper.

### Modules

Pure logic (host-tested with Unity; no `Arduino.h`, no `generated_config.h`;
params passed in as structs, time passed in as arguments):

| Path | Holds |
| --- | --- |
| `lib/counter/src/CounterTypes.h` | **Shared contract**: `Observation`, `DeviceEntry`, marker/address-kind bits, `RawCounts`, params structs, `EstimateSnapshot`, `MinuteRecord`, `NightSummary`, `CalPair`, `VenueProfile`, `Settings`, `NightState`, `RadioStats`, `UiModel`, `Screen`, `UiAction` |
| `lib/counter/src/IdHash.h` (+`Sha256.h`) | portable salted hash |
| `lib/counter/src/BleFingerprint.h` | advert bytes → marker bits |
| `lib/counter/src/ProbeParse.h` | probe request → source address, randomized flag |
| `lib/counter/src/DeviceTable.h` | 8192-slot table over caller storage, expiry + oldest-out at 5000 |
| `lib/counter/src/Census.h` | sweep → `RawCounts`; baseline capture |
| `lib/counter/src/Estimator.h` | ratio, blend, smoothing, warm-up, peak, threshold, alert latch |
| `lib/counter/src/Calibration.h` | pair ring + fit |
| `lib/counter/src/NightLogFormat.h` | the only CSV header/row/event encoder; summary from rows |
| `lib/input/src/Gesture.h` | moved from `include/` unchanged |
| `lib/input/src/UiController.h` | screen/settings/setup/manual-count state machine |
| `lib/app/src/CliParse.h` | serial line → command |

Device-side:

| Path | Holds |
| --- | --- |
| `lib/radio/src/BleScanner.{h,cpp}` | NimBLE passive scan → queue |
| `lib/radio/src/WifiSniffer.{h,cpp}` | null-mode promiscuous probe capture → queue |
| `lib/radio/src/RadioScheduler.{h,cpp}` | slicing task, channel hop, stats |
| `lib/store/src/SettingsStore.{h,cpp}` | NVS |
| `lib/store/src/RtcState.{h,cpp}` | RTC-memory night state, salt lifecycle |
| `lib/store/src/NightStore.{h,cpp}` | LittleFS night files, retention, iteration |
| `lib/display/src/{Theme.h,Widgets.h,Ui.h,Ui.cpp}` | sprite renderer; `Theme.h` moved; shared row/header/battery/big-number widgets |
| `lib/board/src/*` | portrait, brightness, `tone()`, RTC, battery mV |
| `src/Cli.cpp`, `src/main.cpp` | CLI execution; wiring only |
| `scripts/screenshot.py`, `scripts/export_csv.py` | host tools |

Build side: `scripts/config/sections/counter.py` (window, gate, smoothing, peak
hold, table size, threshold default/step, class weights, blend, channels, slice
lengths, background, venue types, alert, dim level/timeout); remove `ble`,
`wifi`, `ota`, `buttons`, `keys`; `boards.yaml` and `platformio.ini` reduced to `sticks3` + `native`, with
`partitions.csv`; packaging (no Improv wait,
`firmware_images.py` checked against the new table), CI, release title,
`web/src/content/site.ts`, `README.md`, `AGENTS.md`, `TODO.md` rewritten.

### Code rules for every agent

- Reuse before writing: `Every`, `Log`, `Gesture`, `Theme`, the config helpers
  (`fields`, `integer`, `boolean`, `cstr`), the existing web components.
- Anything that appears three times becomes one shared helper (UI rows, CSV
  fields, NVS get/put, test fixtures). Similar-looking code with different
  reasons to change stays separate.
- Files stay under ~300 lines; split by responsibility when they grow.
- Configurable values come only from `config.yaml` via `AppConfig`, mapped into
  params structs in `main.cpp`.
- Pure modules are written test-first.

## Execution

`TODO.md` is rewritten as the tracked task list (one line per task, with owner
and status) and updated at each commit. Agents work on disjoint files, use
their own `PLATFORMIO_BUILD_DIR`, and do not commit; I review, integrate and
commit. Section leads orchestrate their own team where nested spawning is
available; otherwise they return a brief per task and I dispatch it.

| Team | Lead (Opus) | Implementers (Sonnet) | Light work (Haiku) |
| --- | --- | --- | --- |
| A Counting core | 1 | IdHash · BleFingerprint+ProbeParse · DeviceTable · Census · Estimator · Calibration · NightLogFormat (7) | test fixtures (1) |
| B Radio + storage | 1 | BleScanner · WifiSniffer+RadioScheduler · SettingsStore+RtcState · NightStore (4) | — |
| C UI + input + CLI | 1 | UiController · Ui+Widgets · board additions · Cli + host tools (4) | file moves (1) |
| D Build, web, docs | 1 | config section + Python tests · packaging + partitions + CI (2) | web copy · README · AGENTS/TODO (3) |
| Review | — | correctness ×3 · privacy audit · PRD traceability (5) | — |

About 33 agents, leaving headroom for fix-up work.

**Step 0 — foundation (me, serial):** `.venv` + requirements; design doc at
`docs/superpowers/specs/2026-11-22-crowd-counter-design.md`; strip the template
and get `pio run -e sticks3`, `pio test -e native` and Python tests green;
write `CounterTypes.h` and the `counter:` section.

**Step 1 — radio spike on the device (Team B lead + me), in parallel with
Step 2's pure work:** throwaway sketch running the slicing scheduler, printing
adverts/s, probes/s per channel and queue drops. Verify chip and flash identity
before the first flash. Pick slice lengths; if Wi-Fi capture cannot be made to
work, fall back to approach 3 and tell Alec before continuing.

**Step 2 — parallel build:** Teams A, C, D start immediately after Step 0;
Team B's radio modules start once the spike has settled the scheduler shape.

**Step 3 — integration (me):** `src/main.cpp`, full build and all test suites.

**Step 4 — bench verification on the device:** flash; over serial confirm
boot-to-scanning time, live counts, minute rows, settings and night state
across `esp_restart`, crash and power cycle, baseline capture, `export`;
pull `shot` PNGs of every screen and look at them.

**Step 5 — review and fixes:** independent reviewers, a simplify pass for
duplication and oversized files, fix confirmed findings, re-verify.

### Commit checkpoints

1. Add PRD and Crowd Counter design doc
2. Strip macro-clicker code; StickS3-only envs; builds and tests green
3. Add shared counter contract and `counter:` config section
4. Custom partition table without core dump; packaging updated
5. Counting core: id hash, fingerprint, probe parser
6. Counting core: device table and census
7. Counting core: estimator, smoothing, peak, threshold
8. Calibration and night-log format
9. Radio: BLE scanner, Wi-Fi sniffer, slicing scheduler (with spike numbers)
10. Storage: settings, RTC night state, night files
11. Input: gestures moved, UI controller
12. Display: widgets and screens; board additions
13. Serial CLI and host export/screenshot tools
14. Wire `main.cpp`; first on-device run
15. Web flasher copy, CI and release workflow
16. README, AGENTS.md, TODO.md
17. Review fixes and bench-verification results

## Verification

- `.venv/bin/pio run -e sticks3` — firmware builds; `pio pkg list` shows one
  M5GFX.
- `.venv/bin/pio test -e native` — all pure-logic suites.
- `.venv/bin/python -m unittest discover -s scripts/tests -v`.
- `web/`: `npm ci && npm run check && npm test && npm run build`.
- On device (Step 4), with serial transcripts and screenshots as evidence.
- **Pending for Alec** (not observable from here): readability at 1 m in the
  dark, which physical button is KEY1, chime loudness, runtime on battery and
  on your power bank, and accuracy against a real manual count (PRD
  milestones 3–4).
