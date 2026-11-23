# Crowd Counter build

Design: docs/superpowers/specs/2026-11-22-crowd-counter-design.md · PRD: docs/PRD.md
Owner key: lead = session lead, A/B/C/D = team (Opus lead, Sonnet/Haiku workers).

## Foundation
- [x] PRD and design doc in repo (lead)
- [x] Strip macro-clicker firmware; StickS3-only envs; custom partition table (lead)
- [x] Shared contract `lib/counter/src/CounterTypes.h` (lead)

## Team A — counting core (pure, host-tested)
- [x] IdHash + Sha256
- [x] BleFingerprint + ProbeParse
- [x] DeviceTable
- [x] Census (counts, baseline/allowlist capture)
- [x] Estimator (ratio, blend, smoothing, warm-up, peak, threshold, alert)
- [x] Calibration
- [x] NightLogFormat (CSV, events, summary)

## Team B — radio and storage (device)
- [ ] Radio spike on the StickS3: slice timings measured (blocked: serial port permission)
- [x] BleScanner
- [x] WifiSniffer + RadioScheduler
- [x] SettingsStore + RtcState
- [x] NightStore (LittleFS)

## Team C — UI, input, CLI
- [x] Gesture test moved to Unity
- [x] UiController
- [x] Widgets + Ui screens
- [x] Board additions (portrait, brightness, tone, RTC, battery mV)
- [x] CliParse + host tools (export, screenshot)

## Team D — build, web, docs
- [x] `counter:` config section; old sections removed; Python tests
- [x] Packaging against the new partition table; CI and release workflow
- [x] Web flasher copy
- [x] README
- [x] AGENTS.md

## Integration and verification (lead)
- [x] `src/main.cpp`, `src/Night.cpp`, `src/Cli.cpp` wiring
- [x] Independent reviews (core, radio/storage, app behaviour, privacy, traceability) and fixes
- [ ] On-device bench verification (blocked: this account cannot open /dev/ttyACM0;
      `sudo usermod -aG dialout $USER` and log in again, or `sudo setfacl -m u:$USER:rw /dev/ttyACM0`)
  - [ ] Verify chip and flash identity, then flash
  - [ ] Radio measurement: adverts/s, probes/s per channel, queue drops for several slice splits;
        set `counter.radio` in config.yaml from the result
  - [ ] Does WIFI_MODE_NULL deliver probe requests? (fallback: STA without connect)
  - [ ] RTC memory across esp_restart, panic, USB reset and power-off; reset reasons reported
  - [ ] Is the RX8130 RTC present (`status` shows a non-zero epoch after `time <epoch>` and a reboot)?
  - [ ] Boot-to-scanning time, including the first-boot filesystem format
  - [ ] Minute rows, `export`, `summary`, `nights`; float formatting in a log row
  - [ ] Screenshots of every screen via `scripts/screenshot.py`; does opening the port reset the chip?
  - [ ] Stack high-water marks for the NimBLE host, Wi-Fi and radio tasks; queue drops in a busy room
  - [ ] Re-check in the disassembly that the hash working buffers are wiped

## Launch video (lead)
- [x] Square LinkedIn cut with contact card (`brag-output-2026-11-22-175504/`); intermediates in `work/` are gitignored

## Decisions waiting on Alec
- [ ] Flash encryption / disabling USB-JTAG (hard to undo): while powered, salt and ids are in RAM
      together, so physical debug access to a running device could recover addresses.
- [ ] End night currently needs Settings -> END NIGHT -> KEY2 hold, with no second confirmation.

## Pending physical checks (Alec)
- [ ] Readable at 1 m in the dark
- [ ] Which physical button is KEY1 / KEY2
- [ ] Chime loudness
- [ ] Runtime on battery and on the power bank
- [ ] Accuracy against a manual count at a real show
