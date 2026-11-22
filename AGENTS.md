# Crowd Counter (StickS3)

## Working agreements

- Target: M5Stack StickS3 (ESP32-S3, 8 MB flash, 8 MB PSRAM). Use the pioarduino/Arduino, M5Unified, M5GFX and NimBLE-Arduino versions pinned in `platformio.ini`. Environments: `sticks3` and `native` only.
- Scope: a small, dependable passive crowd counter covering PRD FR-1..FR-12 (`docs/PRD.md`; design: `docs/superpowers/specs/2026-11-22-crowd-counter-design.md`). Export is CSV over USB serial only.
- Do not add network services, a Wi-Fi access point, OTA, a companion app or a runtime configuration system without a request.
- Privacy invariants are non-negotiable:
  - Receive only. Call only passive-scan and promiscuous-receive APIs (passive NimBLE scan; Wi-Fi in null mode, promiscuous RX). Never connect to, probe, ping or deauth any device. Do not describe NimBLE as "built observer-only": the prebuilt Arduino sdkconfig compiles every BLE role in and the role-disable flags do nothing, so the transmit-capable stack is linked but must never be invoked. `scripts/check_receive_only.py` (run in CI and release) fails on forbidden APIs in `src/` and `lib/` (advertising, client/server, active scan, `esp_wifi_scan_start`/`connect`/`80211_tx`, ESP-NOW, `ble_gap_connect`/`adv_`, Arduino `WiFi`, `WiFiClient`, `HTTPClient`); keep it passing and extend its list rather than bypassing it.
  - A device address is hashed inside the radio callback: id = first 8 bytes of SHA-256(salt || signal || address). The application never keeps a raw address, but the radio drivers necessarily hold the received frame in RAM before the callback runs, so say "never stored by the application", not "never in memory". No application-owned copy may outlive the callback.
  - Never write to flash, print, log or send a raw address or id, including over serial.
  - The salt and the allowlist are held only in RAM (RTC memory plus working copies), never written to flash or printed. They survive a crash or soft reset and are lost at power-off. The salt is regenerated at every cold power-on and every End night, and automatically once a night has run 18 hours.
  - Ids are salted pseudonyms, not anonymous while powered: salt and ids are in RAM together, so someone with physical debug access to a running device could recover addresses by enumeration. Nothing persisted or emitted identifies a device. State this plainly; do not overclaim.
  - No core dumps. `partitions.csv` has no coredump partition; do not add one or enable core dump to flash.
  - Night logs hold aggregate counts only.
  - `config.yaml` never holds secrets: it is published with the firmware.
- `lib/counter/src/CounterTypes.h` is the shared data contract. Every workstream builds against it, so change it only by agreement.
- `config.yaml` is the single source of tunables, validated at build time by `scripts/generate_config.py` and `scripts/config/sections/`. Firmware reads values only through `AppConfig` in the generated header (never committed), mapped into params structs in `src/main.cpp`. Do not hardcode tunables in firmware.
- Module boundaries (see the table below):
  - Pure modules include no `Arduino.h` and no `generated_config.h`. They take params structs, take time as an argument, and are written test-first.
  - `src/main.cpp` is wiring only. `src/Cli.cpp` executes CLI commands.
  - The display renders a plain `UiModel` and holds no input logic.
  - `NightLogFormat.h` is the only CSV encoder.
  - Host tools live in `scripts/`.
  - `web/` is the independent Astro flasher. It reads the packaged `index.json` rather than duplicating config.
- Preserve these design behaviours:
  - Warm-up samples are excluded from peak, minutes-above-X and the alert.
  - The alert fires once per night.
  - Baseline (rotating-address weighted count) and allowlist (fixed-address ids) are disjoint, so nothing is subtracted twice.
  - Background removal stays narrow: fixed-address, non-phone BLE devices only.
  - Night state survives a crash reset but not power-off.
  - A cold boot shows "New night / Resume" only when the open night has a log file; no press within 15 s means a new night.
  - Retention: the newest 20 night logs are kept; the oldest are deleted beyond that or when free space drops below 64 KB. Starting a new night never deletes the previous one.
  - Setup capture needs confirmation: a KEY2 hold shows "HOLD K2 AGAIN TO CAPTURE"; a second hold within 5 s captures, anything else cancels. Samples for 60 s after a capture are warm-up.
  - Manual count during warm-up is rejected. Confirmation beeps and the error tone follow `alert.chime`.
  - Chime volume is capped to avoid a brownout.
- Code rules:
  - Reuse before writing: `Every`, `Log`, `Gesture`, `Theme`, the config helpers (`fields`, `integer`, `boolean`, `cstr`), existing web components and CSS tokens.
  - When something appears three times, extract one shared helper. Keep look-alike code with different reasons to change separate.
  - Keep files under ~300 lines; split by responsibility when they grow.
- Battery and accuracy honesty: never claim battery runtime, accuracy or hardware behaviour as verified unless observed or confirmed by the user. Report physical checks as pending.

Module map (from the design doc):

| Path | Responsibility |
| --- | --- |
| `lib/counter/src/CounterTypes.h` | Shared contract: observations, devices, census, estimator, night log, settings, radio, UI |
| `lib/counter/src/IdHash.h`, `Sha256.h` | Salted device id hash |
| `lib/counter/src/BleFingerprint.h` | Advert bytes to marker bits |
| `lib/counter/src/ProbeParse.h` | Probe request to source address and randomized flag |
| `lib/counter/src/DeviceTable.h` | Fixed-capacity device table over caller storage; expiry and oldest-out |
| `lib/counter/src/Census.h` | Table sweep to raw counts; baseline capture |
| `lib/counter/src/Estimator.h` | Ratio, blend, smoothing, warm-up, held peak, threshold, alert latch |
| `lib/counter/src/Calibration.h` | Calibration pair ring and fit |
| `lib/counter/src/NightLogFormat.h` | The only CSV header, row and event encoder; summary from rows |
| `lib/input/src/Gesture.h` | Button gesture recognition |
| `lib/input/src/UiController.h` | Screen, settings, setup and manual-count state machine |
| `lib/app/src/CliParse.h` | Serial line to command |
| `lib/radio/src/BleScanner.{h,cpp}` | NimBLE passive scan into a queue |
| `lib/radio/src/WifiSniffer.{h,cpp}` | Null-mode promiscuous probe capture into a queue |
| `lib/radio/src/RadioScheduler.{h,cpp}` | Time-slices BLE and Wi-Fi, channel hopping, stats |
| `lib/store/src/SettingsStore.{h,cpp}` | NVS settings |
| `lib/store/src/RtcState.{h,cpp}` | RTC-memory night state and salt lifecycle |
| `lib/store/src/NightStore.{h,cpp}` | LittleFS night files, retention, iteration |
| `lib/display/src/{Theme.h,Widgets.h,Ui.h,Ui.cpp}` | Sprite renderer, theme and shared widgets |
| `lib/board/src/*` | Portrait, brightness, tone, RTC, battery mV |
| `lib/core/src/Every.h`, `Log.h` | Shared helpers: reuse, do not duplicate |
| `src/Cli.cpp`, `src/main.cpp` | CLI execution; wiring only |
| `scripts/` | Host tools, config generator and packaging |

## Build and validation

- Create a Python 3.12 venv (pioarduino rejects newer Pythons such as 3.15) and install requirements: `python3.12 -m venv .venv && .venv/bin/python -m pip install -r requirements.txt`. On Windows use `.venv\Scripts\pio` and `.venv\Scripts\python`.
- Firmware: `.venv/bin/pio run -e sticks3`.
- Host C++ tests: `.venv/bin/pio test -e native` (Unity tests of pure libraries in `lib/counter`, `lib/input`, `lib/app` and `test/`).
- Receive-only guard: `python scripts/check_receive_only.py` (stdlib only; also run in CI).
- Python tests: `.venv/bin/python -m unittest discover -s scripts/tests -v`. Run them after a firmware build, because packaging tests use PlatformIO's esptool.
- Package: `.venv/bin/python scripts/package_firmware.py --output web/public/firmware`.
- Web: in `web/`, run `npm ci && npm run check && npm test && npm run build`.
- Use a private `PLATFORMIO_BUILD_DIR` for each agent when several build at once.
- A successful compile does not prove radio capture, display rendering, button identity, chime, accuracy or battery runtime.
- Keep the README's config reference, button table, CSV description and pending-checks list aligned with `config.yaml`, `scripts/config/` and the firmware.

## Flashing

- Re-identify the intended USB device first with `.venv/bin/pio device list`. The StickS3 is USB ID 303a:1001. Port names such as `/dev/ttyACM0` are examples, not stable identifiers.
- Verify chip and flash identity (ESP32-S3, 8 MB) before writing.
- Flash only within the user's authorization. Upload with `.venv/bin/pio run -e sticks3 -t upload`.
- Close serial monitors before uploading.
- Flashing keeps settings and night logs. Erasing (`-t erase`) deletes night logs and calibration.

## Commits and workflow

- Keep a TODO list: `TODO.md` tracks tasks with owner and status, updated at each commit.
- Keep parallel workstreams on distinct files. Coordinate shared interfaces, starting with `CounterTypes.h`.
- Make focused local commits with descriptive titles and bodies.
- Preserve user changes. Never push unless asked.

<!-- CODEGRAPH_START -->
## CodeGraph

In repositories indexed by CodeGraph (a `.codegraph/` directory exists at the repo root), reach for it BEFORE grep/find or reading files when you need to understand or locate code:

- **MCP tool** (when available): `codegraph_explore` answers most code questions in one call — the relevant symbols' verbatim source plus the call paths between them, including dynamic-dispatch hops grep can't follow. Name a file or symbol in the query to read its current line-numbered source. If it's listed but deferred, load it by name via tool search.
- **Shell** (always works): `codegraph explore "<symbol names or question>"` prints the same output.

If there is no `.codegraph/` directory, skip CodeGraph entirely — indexing is the user's decision.
<!-- CODEGRAPH_END -->
