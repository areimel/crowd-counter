# Crowd Counter build

Design: docs/superpowers/specs/2026-11-22-crowd-counter-design.md · PRD: docs/PRD.md
Owner key: lead = session lead, A/B/C/D = team (Opus lead, Sonnet/Haiku workers).

## Foundation
- [x] PRD and design doc in repo (lead)
- [x] Strip macro-clicker firmware; StickS3-only envs; custom partition table (lead)
- [x] Shared contract `lib/counter/src/CounterTypes.h` (lead)

## Team A — counting core (pure, host-tested)
- [ ] IdHash + Sha256
- [ ] BleFingerprint + ProbeParse
- [ ] DeviceTable
- [ ] Census (counts, baseline/allowlist capture)
- [ ] Estimator (ratio, blend, smoothing, warm-up, peak, threshold, alert)
- [ ] Calibration
- [ ] NightLogFormat (CSV, events, summary)

## Team B — radio and storage (device)
- [ ] Radio spike on the StickS3: slice timings measured
- [ ] BleScanner
- [ ] WifiSniffer + RadioScheduler
- [ ] SettingsStore + RtcState
- [ ] NightStore (LittleFS)

## Team C — UI, input, CLI
- [ ] Gesture test moved to Unity
- [ ] UiController
- [ ] Widgets + Ui screens
- [ ] Board additions (portrait, brightness, tone, RTC, battery mV)
- [ ] CliParse + host tools (export, screenshot)

## Team D — build, web, docs
- [ ] `counter:` config section; old sections removed; Python tests
- [ ] Packaging against the new partition table; CI and release workflow
- [ ] Web flasher copy
- [ ] README
- [ ] AGENTS.md

## Integration and verification (lead)
- [ ] `src/main.cpp` + `src/Cli.cpp` wiring
- [ ] On-device bench verification (serial transcripts, screenshots)
- [ ] Independent reviews and fixes

## Pending physical checks (Alec)
- [ ] Readable at 1 m in the dark
- [ ] Which physical button is KEY1 / KEY2
- [ ] Chime loudness
- [ ] Runtime on battery and on the power bank
- [ ] Accuracy against a manual count at a real show
