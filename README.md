# Crowd Counter

Crowd Counter is a pocket device, built on an M5Stack StickS3, that estimates how many people are in a room. It passively counts the phones and wearables it can hear over Bluetooth Low Energy (BLE) and Wi-Fi probe requests, and it shows one live number and whether the crowd has cleared a threshold X. It was built for a cover band that gets a bonus when attendance tops X, so the band has its own number at settle-up.

**The number is an estimate. It is not a certified or legally binding count.** Use it as a cross-check on the venue's door tally and a starting point for a conversation, not as a headcount.

Status: firmware and host tools are in progress. Nothing in this README has been measured on hardware yet. Accuracy, battery runtime and button identity are still pending (see [Known limitations and pending checks](#known-limitations-and-pending-checks)).

Design documents: [docs/PRD.md](docs/PRD.md) (product requirements) and [docs/superpowers/specs/2026-11-22-crowd-counter-design.md](docs/superpowers/specs/2026-11-22-crowd-counter-design.md) (design and build plan).

## Limits

- **BLE only.** The ESP32-S3 has no Bluetooth Classic, so classic-only devices (some older headphones) are invisible.
- **2.4 GHz only.** Wi-Fi probe requests on 5 GHz are missed.
- **One shared radio.** BLE and Wi-Fi capture are time-sliced, so each sees the room for only part of the time.
- **Occupancy, not door counting.** It estimates how many people are present. It does not count people in and out.
- **Signal depends on the crowd.** Phones in airplane mode, older crowds with fewer devices, and packed rooms (bodies absorb 2.4 GHz) all lower the signal.
- **Outside signals.** Phones on the street or in the next bar can leak in unless the RSSI gate filters them.
- **Temperature.** Rated 0 to 40 C. Do not leave it on an amplifier.

## Hardware

| Item | Spec |
| --- | --- |
| SoC | ESP32-S3-PICO-1-N8R8 (dual-core, 240 MHz) |
| Memory | 8 MB flash, 8 MB PSRAM |
| Radios | 2.4 GHz Wi-Fi (receive-only capture) and Bluetooth LE (observer only) |
| Display | 1.14 inch ST7789, 135 x 240, portrait |
| Buttons | KEY1 (front), KEY2 (side) |
| Audio | ES8311 codec with speaker (alert chime) |
| Battery | 250 mAh internal cell |
| Connector | USB-C (power and serial export) |

Nothing else is needed to run the device. A USB-C power bank is recommended for a full night (see [Battery](#battery)).

## Quick start

### Build and flash from source

The toolchain needs Python 3.12. pioarduino rejects newer Python versions such as 3.15, so create the virtual environment with `python3.12` explicitly.

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/pio run -e sticks3
```

Plug in the StickS3 and identify its serial port before uploading:

```sh
.venv/bin/pio device list
```

The StickS3 enumerates with USB ID `303a:1001`. Port names such as `/dev/ttyACM0` are examples only; use the port that `pio device list` reports for this board, and check the USB ID before you flash. Close any serial monitor first.

```sh
.venv/bin/pio run -e sticks3 -t upload
.venv/bin/pio device monitor
```

To wipe the device (this also deletes settings and all night logs):

```sh
.venv/bin/pio run -e sticks3 -t erase
```

**Windows.** Use your Python 3.12 install to create `.venv`, then replace `.venv/bin/` with `.venv\Scripts\` in the commands above (for example `.venv\Scripts\pio run -e sticks3`).

**Local settings.** Upload and monitor ports are local choices. Put them in `platformio_override.ini`, which is not committed.

### Flash from the browser

The browser flasher is an Astro app in [web/](web/). It serves the firmware package that `scripts/package_firmware.py` writes into `web/public/firmware`.

```sh
.venv/bin/python scripts/package_firmware.py --output web/public/firmware
cd web
npm ci
npm run dev
```

Open the local page in Chrome, Edge or Opera on desktop. The flasher uses Web Serial.

Flashing writes the bootloader, partition table, boot app and firmware. Settings and night logs are kept unless you choose to erase the device.

## Using it at a show

1. **Ask the venue first.** Get their permission and tell them the device is running. See [Privacy and legal](#privacy-and-legal).
2. **Power on at load-in.** The device boots to scanning. The boot-to-scanning target is under 10 seconds; this is not yet measured.
3. **Set X.** Open settings and set the threshold (default 120).
4. **Run setup at soundcheck.** With the band's and staff's phones nearby, long-press KEY2 to start setup mode (see below).
5. **Place it centrally** on the stage or merch table (the PRD's placement guidance).
6. **Glance at the number** between songs. Use KEY1 short presses to change screens.
7. **At the end,** export the night log over USB (see [Exporting logs](#exporting-logs)).

### Buttons

| Action | KEY1 (front) | KEY2 (side) |
| --- | --- | --- |
| Short press | Cycle screens: count, detail, summary | Dim the screen, or wake it |
| Long press | Enter settings | Start setup mode (baseline and allowlist capture) |
| In settings, short press | Next item | Increase the value by one step |
| In settings, double-click | (no action) | Decrease the value by one step |
| In settings, long press | Leave settings | On "manual count": save the count. On "end night": end the night. Otherwise no action |

Gesture timing: debounce 25 ms, double-click window 250 ms, long press 650 ms (defaults from `config.yaml`).

While the screen is dimmed, the first press of either button only wakes it and is otherwise ignored. An alert also wakes the screen.

### Screens

The screen is 135 x 240 pixels in portrait, on a dark background.

- **Count (main screen).** The estimate in the largest font that fits, with "est." beside it. A status bar coloured by the threshold: grey below 80% of X, amber from 80% to 99% of X, green at X or above. A line reading `X=120 pk151` (threshold and peak). An "ABOVE X" line when the crowd is at or above the threshold. Battery icon and a scanning pulse so you can see the device is alive.
- **Detail (for the builder, not the band).** BLE count, Wi-Fi count, ratio in use, background count, baseline, RSSI gate.
- **Summary (night).** Peak, minutes at or above X, start time, current time and elapsed time, X, venue type.

The count screen shows "warming up" instead of a steady reading for a while after boot, after a resume, and after a new setup capture. Warm-up samples are excluded from the peak, from minutes above X, and from the alert.

### Settings

Settings are reached with a KEY1 long press. KEY1 short presses move between items; KEY2 changes the value.

| Item | What it does |
| --- | --- |
| Threshold X | Sets X. Default 120, step 10, maximum 990. Saved to flash (NVS) and kept across reboots. |
| Venue type | Chooses the devices-per-person ratio. Shipped types: BAR / PUB and PRIVATE EVENT. |
| RSSI gate | Signal floor for counting a device. Default -85 dBm. Adjustable on the device. |
| Manual count | Enter a headcount for calibration (see [Calibration](#calibration-workflow)). It starts at the current estimate, rounded. |
| End night | Ends the current night. |

### Setup mode and baseline

Setup mode starts with a KEY2 long press. Run it at soundcheck with the band's and staff's phones nearby. It captures:

- a **baseline count** of rotating-address devices, which is subtracted for the rest of the night;
- the **hashed ids of fixed-address devices**, kept as an allowlist and removed from the count.

The capture finalises once a full 5-minute window of data exists, so you can press it and walk away. You can re-capture at any time with another KEY2 long press. The allowlist and the per-night salt live only in RTC memory, so they are lost when the device is powered off.

### Cold power-on and the night prompt

If the device is powered on cold and a night is still open, it asks "New night / Resume" for 15 seconds:

- KEY1 starts a new night.
- KEY2 resumes the open night.
- No press within 15 seconds starts a new night.

Old nights are always kept (the last 20 night files are retained). After a cold resume only the aggregate baseline survives. The allowlist and salt are gone, so the screen suggests re-running setup. Re-run setup after any power loss.

A reboot or crash that does not cut power keeps the night state, because it lives in RTC memory.

### Auto-dim

The screen dims to a low level 30 seconds after the last press. Any press wakes it. Dim and normal levels are set in `config.yaml`.

### Threshold alert

When the smoothed estimate holds at or above X for 30 seconds, the device flashes the screen and plays a chime. This fires once per night. Both are on by default and can be switched off in `config.yaml` (`counter.alert.flash`, `counter.alert.chime`). The chime volume is capped to avoid a brownout.

## How counting works

The device never counts people directly. It counts radio-visible devices, removes noise, and divides by a devices-per-person ratio.

1. **Scan.** The radio alternates between BLE scanning (about 2 s) and Wi-Fi probe capture (about 1 s dwell) on channels 1, 6 and 11. Each address is hashed with a per-night random salt the moment it is received. Raw addresses are never stored.
2. **Window.** Every 5 seconds the device counts unique devices seen in the last 5 minutes.
3. **Clean up.** It drops signals below the RSSI gate, drops allowlisted devices, and drops background devices (fixed-address, non-phone BLE devices seen for more than 90 minutes).
4. **Classify coarsely.** Devices are grouped as phone, wearable, audio, tracker, computer, beacon or other. The classes are types, never people. Some Samsung and Apple markers are ambiguous and fall into "other".
5. **Estimate.** Each class has a weight (phone 1.0, wearable, audio and computer 0.25, tracker and beacon 0, other 0.5). The device blends BLE and Wi-Fi (default 80% BLE), subtracts the setup baseline, and divides by the venue's ratio.
6. **Smooth and hold.** The displayed value is smoothed over 60 seconds. The peak is the highest smoothed value held for at least 10 minutes, so a single spike does not count.

```
estimate = (weighted unique devices in window - background - baseline) / devices-per-person ratio
```

The starting ratios are guesses (BAR / PUB 1.45, PRIVATE EVENT 1.30 for BLE). Calibration replaces them. Wi-Fi has its own ratio. Until it is fitted it gets zero weight in the blend.

The MAC-address randomisation problem and the reasons for each step are in the [PRD](docs/PRD.md) and the [design document](docs/superpowers/specs/2026-11-22-crowd-counter-design.md).

## Configuration (`config.yaml`)

[config.yaml](config.yaml) is the source of truth for the counter's settings. The build validates it with [scripts/generate_config.py](scripts/generate_config.py). An invalid value fails the build and the error names the key path, for example `counter.threshold.step`.

The validated values are compiled into a generated header at `.pio/build/<env>/generated/generated_config.h` (namespace `AppConfig`). That header is generated and never committed.

`config.yaml` is embedded in the firmware and in the published release index (`index.json`) that the web flasher serves. **Never put secrets in it.** It has none today, and that must stay true.

| Section | Key | Default | Allowed | Meaning |
| --- | --- | --- | --- | --- |
| `project` | `name` | Crowd Counter | 1-32 printable ASCII, required | Project name |
| `project` | `tagline` | | up to 120 chars | Short tagline |
| `project` | `description` | | up to 500 chars | Longer description |
| `display` | `brightness` | 96 | 1-255 | Normal screen brightness |
| `display` | `dim_brightness` | 12 | 1-255, must be below `brightness` | Brightness while dimmed |
| `display` | `dim_after_s` | 30 | 5-600 | Seconds after the last press before dimming |
| `gestures` | `debounce_ms` | 25 | 1-100, below `double_click_ms` | Button debounce |
| `gestures` | `double_click_ms` | 250 | 50-1000, below `long_press_ms` | Double-click window |
| `gestures` | `long_press_ms` | 650 | 100-10000 | Long-press threshold |
| `counter` | `window_s` | 300 | 60-900 | Sliding window for unique devices |
| `counter` | `rssi_gate_dbm` | -85 | -95 to -50 | Signal floor for counting (adjustable on the device) |
| `counter` | `recompute_s` | 5 | 1-30 | How often the estimate is recomputed |
| `counter` | `smoothing_s` | 60 | `recompute_s` to 300, multiple of `recompute_s` | Smoothing window for the displayed value |
| `counter` | `peak_hold_min` | 10 | 1-30 | Minutes a value must hold to count as the peak |
| `counter` | `alert_hold_s` | 30 | 0-300 | Seconds at or above X before the alert fires |
| `counter` | `table_capacity` | 5000 | 500-6000 | Device table size; oldest entries are evicted |
| `counter` | `background_after_min` | 90 | 10-600 | Minutes a fixed-address non-phone BLE device must be present to count as background |
| `counter` | `blend_ble_pct` | 80 | 0-100 | BLE share of the BLE/Wi-Fi blend |
| `counter` | `threshold.default` | 120 | multiple of `step`, within `step` to `max` | Starting threshold X |
| `counter` | `threshold.step` | 10 | 1-50 | Change per button press |
| `counter` | `threshold.max` | 990 | `step` to 9999 | Maximum X |
| `counter` | `class_weights_pct.phone` | 100 | 0-100, required | Weight of a phone |
| `counter` | `class_weights_pct.wearable` | 25 | 0-100, required | Weight of a wearable |
| `counter` | `class_weights_pct.audio` | 25 | 0-100, required | Weight of an audio device |
| `counter` | `class_weights_pct.tracker` | 0 | 0-100, required | Weight of a tracker |
| `counter` | `class_weights_pct.computer` | 25 | 0-100, required | Weight of a computer |
| `counter` | `class_weights_pct.beacon` | 0 | 0-100, required | Weight of a beacon |
| `counter` | `class_weights_pct.other` | 50 | 0-100, required | Weight of an unclassified device |
| `counter` | `radio.ble_slice_ms` | 2000 | 200-10000 | BLE scan slice length (to be set from bench measurements) |
| `counter` | `radio.wifi_dwell_ms` | 1000 | 200-10000 | Wi-Fi dwell per channel (to be set from bench measurements) |
| `counter` | `radio.channels` | [1, 6, 11] | 1-3 distinct channels, each 1-13 | Wi-Fi channels to capture |
| `counter` | `venues` | BAR / PUB 1.45 / 1.0; PRIVATE EVENT 1.30 / 1.0 | 1-8 entries | Each entry: `name` (1-14 printable ASCII, unique), `ratio_ble` (0.3-5.0), `ratio_wifi` (0.3-5.0) |
| `counter` | `alert.flash` | true | true/false | Flash the screen when the alert fires |
| `counter` | `alert.chime` | true | true/false | Play the chime when the alert fires |
| `counter` | `alert.volume` | 96 | 0-160 | Chime volume |
| (top level) | `schema` | none | optional path | Path to a `.py` module that adds project-specific sections |

Adding a venue type: add an entry under `counter.venues` (up to eight), rebuild, and flash. Its starting ratios are guesses until you calibrate it.

## Calibration workflow

Calibration turns the guessed ratios into ones fitted to this venue type.

1. **Count at a full-crowd show.** Calibrate at shows that are full, not empty. Packed rooms change the signal.
2. **Do a manual headcount** at a known time.
3. **Enter it on the device.** KEY1 long press to open settings, KEY1 short presses to reach "manual count" (it starts at the current estimate, rounded), KEY2 presses to set the true count, then KEY2 long press to save.
4. **Repeat** at a few shows. Each save stores a pair of (raw BLE signal, raw Wi-Fi signal, true count) for the current venue type. The device keeps a ring of 16 pairs per venue type.
5. **Fit.** The ratio is fitted as `sum(raw x true) / sum(true^2)`, clamped to 0.3 to 5, and saved to flash (NVS).

Each save is also logged as an event row in the night log, so the method change can be traced later.

Old logs can be used to tune later. Each minute row records per-class counts, both raw signals, battery millivolts and the warm-up flag. Method changes during a night appear as event rows (see [CSV format](#csv-format)).

Calibration is not yet measured. The MVP target is an estimate within plus or minus 20% of a manual count at three real shows after calibration. That target has not been met or tested yet.

## Exporting logs

Each night is written to one file on the device's LittleFS filesystem, with one row per minute. The device keeps the last 20 nights.

### Serial commands

The serial CLI runs at 115200 baud over USB CDC.

| Command | Purpose |
| --- | --- |
| `help` | List the commands |
| `status` | Show device status |
| `nights` | List stored nights |
| `export [n]` | Send a night's CSV. Without `n`, the device uses its default night |
| `summary [n]` | Send the night summary (peak, minutes above X, start and end times) |
| `time <epoch>` | Set the clock, in Unix seconds. Use this if the board's RTC does not answer |
| `shot` | Send the screen contents as a raw frame, for `screenshot.py` |

Rows show minutes since night start. They also show the wall-clock epoch when the clock is known; the epoch is 0 when it is not.

### Host tools

Run these with the repo venv's Python. Each one needs `--port`.

**Export a night's CSV**

```sh
.venv/bin/python scripts/export_csv.py --port /dev/ttyACM0
.venv/bin/python scripts/export_csv.py --port /dev/ttyACM0 --night 3 --out night-3.csv
```

Options: `N` or `--night N` selects a night (omit for the device default), `--out FILE` sets the output path (default `night-<id>.csv`), `--timeout SECONDS` (default 30), `--baud` (default 115200).

**Save the screen as a PNG**

```sh
.venv/bin/python scripts/screenshot.py --port /dev/ttyACM0 --out screen.png
```

Options: `--out FILE` (default `shot-YYYYMMDD-HHMMSS.png`), `--timeout SECONDS` (default 20), `--baud`.

**`scripts/serial_link.py`** is the shared serial helper that the two tools import. It has no command-line interface of its own.

Use the port names reported by `pio device list` on your machine; `/dev/ttyACM0` is an example.

### CSV format

The export is the night's file sent verbatim, so it includes comment lines starting with `#`. Most spreadsheet tools will need those lines skipped or handled.

- `# crowd-counter night=<id> firmware=<version>`: the title line.
- `# method venue="..." ratio_ble=... ratio_wifi=... wifi_fitted=... blend=... window_s=... rssi_gate=... w_<class>=... baseline_ble=... baseline_wifi=... baseline_fixed=... baseline_valid=...`: the method in use. It records the venue, ratios, blend, window, RSSI gate, class weights and baseline, so the number can be explained.
- The column header row (below), then one row per minute.
- Event lines, also starting with `#`:
  - `# event minute=N method ...`: the method changed mid-night.
  - `# event minute=N resume`: the night was resumed.
  - `# event minute=N manual_count truth=N ble_signal=... wifi_signal=...`: a manual count was saved.

The column header is defined in [lib/counter/src/NightLogFormat.h](lib/counter/src/NightLogFormat.h). The 21 columns are:

| Column | Meaning |
| --- | --- |
| `minute` | Minutes since night start |
| `epoch` | Unix time, or 0 if the clock is unknown |
| `ble_phone`, `ble_wearable`, `ble_audio`, `ble_tracker`, `ble_computer`, `ble_beacon`, `ble_other` | BLE devices per class |
| `wifi` | Wi-Fi devices |
| `wifi_rotating` | Wi-Fi devices with rotating addresses |
| `background` | Background devices removed |
| `allowlisted` | Allowlisted devices removed |
| `ble_signal`, `wifi_signal` | Raw signal values |
| `estimate` | Instant estimate |
| `smoothed` | Smoothed estimate shown on the device |
| `peak` | Held peak so far |
| `threshold` | X at that minute |
| `battery_mv` | Battery voltage in millivolts |
| `flags` | Bit flags, including warm-up (bit values are defined in `lib/counter/src/CounterTypes.h`) |

The night summary holds the peak, minutes at or above X (not counting warm-up), and start and end times.

## Privacy and legal

- **Passive only.** The device never connects to, pings or deauthenticates anyone's device. BLE is built as an observer, and Wi-Fi starts in a receive-only mode, so the device does not transmit.
- **Hashed addresses.** Every address is hashed as it is received: the id is the first 8 bytes of SHA-256 over a per-night salt, the signal and the address. Raw addresses are never stored, written to flash, printed or sent. Serial output never prints ids or addresses.
- **Salt in RTC memory.** The per-night salt and the allowlist live only in RTC memory. They survive a reset and are gone at power-off. A new salt is used each night.
- **Aggregate logs only.** Night logs hold counts and signal levels, never identifiers.
- **No core dumps.** The custom partition table has no coredump partition, so a crash cannot write a radio-task stack (which could hold an address) to flash.
- **Ask first.** Get the venue's permission before using the device, and be upfront that it is running.

This is not legal advice. Passive Wi-Fi and BLE sniffing for aggregate counts is common in retail analytics, but the rules differ by state and country. Check before relying on it in a contract.

## Tests

```sh
.venv/bin/pio test -e native
```

Host Unity tests cover the pure libraries in `lib/counter`, `lib/input`, `lib/app` and `test/`.

```sh
.venv/bin/pio run -e sticks3
.venv/bin/python -m unittest discover -s scripts/tests -v
```

Run the Python tests after a firmware build. The packaging tests use PlatformIO's esptool.

```sh
cd web
npm ci
npm run check
npm test
npm run build
```

### CI and release

- [.github/workflows/ci.yml](.github/workflows/ci.yml) builds the `sticks3` firmware, runs the Python tests, runs `pio test -e native`, packages the firmware, and runs the web check, test and build.
- [.github/workflows/release.yml](.github/workflows/release.yml) is a manual dispatch. It takes an existing `vX.Y.Z` tag and publishes a GitHub Release titled `Crowd Counter <version>` and the Pages site.

Passing CI shows that the code builds and the tests pass. It does not show that Bluetooth or Wi-Fi capture, button identity, display output, the chime, accuracy or battery life work on hardware.

## Repo layout

The module map as designed. Some modules are still being written.

| Path | Holds |
| --- | --- |
| `lib/counter/src/CounterTypes.h` | Shared data contract (observations, device entries, params, snapshots, night log records, UI model) |
| `lib/counter/src/IdHash.h`, `Sha256.h` | Salted, truncated SHA-256 id hashing |
| `lib/counter/src/BleFingerprint.h` | BLE advert bytes to coarse class marker bits |
| `lib/counter/src/ProbeParse.h` | Probe request to source address and randomised flag |
| `lib/counter/src/DeviceTable.h` | Fixed-size device table with expiry and oldest-out eviction |
| `lib/counter/src/Census.h` | Window sweep to raw class counts; baseline capture |
| `lib/counter/src/Estimator.h` | Ratio, blend, smoothing, warm-up, peak, threshold, alert latch |
| `lib/counter/src/Calibration.h` | Calibration pair ring and fit |
| `lib/counter/src/NightLogFormat.h` | The only CSV header, row and event encoder |
| `lib/input/src/Gesture.h` | Button gesture recogniser |
| `lib/input/src/UiController.h` | Screen, settings, setup and manual-count state machine |
| `lib/app/src/CliParse.h` | Serial line to command |
| `lib/radio/src` | BLE scanner, Wi-Fi sniffer, radio slicing scheduler (device side) |
| `lib/store/src` | Settings (NVS), RTC night state, night files (LittleFS) (device side) |
| `lib/display/src` | Theme, widgets and screen renderer |
| `lib/board/src` | Board support: portrait, brightness, tone, RTC, battery |
| `src/` | Serial CLI execution and `main.cpp` wiring only |
| `scripts/` | Config generator, packaging, CI helpers, host tools |
| `web/` | Astro browser flasher |
| `boards/boards.yaml` | Board definitions (StickS3 only) |
| `platformio.ini`, `partitions.csv` | Build environments (`sticks3`, `native`) and the custom partition table |

## Known limitations and pending checks

None of the following has been verified. They are open items, not results.

- Readability at 1 m in a dark room.
- Which physical button is KEY1 and which is KEY2.
- Chime loudness.
- Boot-to-scanning time (target under 10 seconds).
- Radio slice lengths, from the bench spike.
- Whether Wi-Fi capture works alongside BLE. If it does not, the fallback is BLE only.
- Accuracy against a real manual count (target within plus or minus 20% at three shows).
- Runtime on the internal battery and on a power bank.

### Battery

The StickS3 has a 250 mAh internal cell. The design estimate is 1.5 to 2.5 hours with both radios receiving. That is an estimate, not a measurement. The 5-hour target for a full gig assumes a USB-C power bank. Battery voltage is logged every minute, so runtime will be measured rather than claimed.

### Other limitations

- Host test suites for the pure modules are in the repo, but this README does not record their results. The device-side radio, storage and UI modules have not been checked on hardware.
