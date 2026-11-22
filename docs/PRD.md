# Crowd Counter — PRD

Nov 22, 2026 · @Alec

## Overview

Crowd Counter is a pocket-sized device that estimates how many people are at a show by passively counting the phones and wearables in the room. It runs on an M5Stack StickS3 and shows one live number, plus whether the crowd has cleared the payout threshold.

**Problem.** A local cover band gets a bonus when attendance tops a set threshold (X people). Today the count comes from the venue's door tally, a bartender's guess, or nobody at all. The band has no independent number to bring to the settle-up at the end of the night.

**Solution.** A $21 device clipped to the stage or the merch table that:

- Scans Bluetooth Low Energy (BLE) advertisements and Wi-Fi probe requests from nearby devices
- Collapses them into an estimated headcount, using a correction factor tuned against real manual counts
- Logs the count over the night, so the band can show the peak and how long it held

**Positioning.** The number is an evidence-backed *estimate*, not a certified count. It works best as a cross-check on the venue's tally and a starting point for a conversation.

## Goals and success metrics

The MVP succeeds if it lands within ±20% of a manual count at three real shows.

**Goals**

1. Give a live attendance estimate, readable at a glance on the device
2. Flag clearly when the estimate crosses the band's threshold X
3. Keep a per-night log (peak, time above X, count curve) the band can show afterward
4. Run a full gig (about 5 hours) on battery or a small USB power bank

**Non-goals (v1)**

- A legally binding or certified headcount
- Identifying, tracking or storing data about individual attendees
- Door-entry counting (people in vs. out). This is an occupancy estimate.
- A companion phone app (later phase)

**Success metrics**

| Metric | Target |
| --- | --- |
| Error vs. manual count, after calibration | within ±20% at 3 shows |
| Threshold call (above or below X) | correct at 9 of 10 shows |
| Battery runtime, screen dimmed | 5 h or more |
| Setup time at the venue | under 2 minutes |
| Raw device identifiers stored | zero |

## Users and use cases

The main user is a band member with a few seconds to spare between songs, not a developer.

| User | Need |
| --- | --- |
| Band member or manager | Glance at the number mid-set; bring the log to settle-up |
| Alec (builder) | Calibrate, debug, update firmware, tune the estimator |
| Venue owner (indirect) | A number they can trust, or at least argue with in good faith |

**User stories**

- As a band member, I power on the device at load-in, set X, and set it on the merch table.
- As a band member, I glance at it between songs and see "142 est. · above 120" in green.
- As a band manager, I open the night's log at the end and see the peak and how long the crowd stayed above X.
- As the builder, I enter a manual headcount during a show so the device can learn a better correction factor for that venue type.
- As the builder, I mark the band's and staff's own phones so they don't count toward the total.

## Hardware

The StickS3 covers every MVP need on one board: it has both radios we sniff with, a screen, buttons, and enough PSRAM to hold a large device table. ([M5Stack docs](https://docs.m5stack.com/en/core/StickS3), [CNX Software](https://www.cnx-software.com/2026/01/23/m5stack-sticks3-esp32-s3-mini-iot-dev-kit-features-1-14-inch-color-display-speaker-and-mic-for-voice-interaction/))

| Component | Spec | Use here |
| --- | --- | --- |
| SoC | ESP32-S3-PICO-1-N8R8, dual-core LX7 @ 240 MHz | One core scans, one runs UI and estimator |
| Memory | 8 MB flash, 8 MB PSRAM | Device table, hashed IDs, night logs |
| Radios | 2.4 GHz Wi-Fi + Bluetooth LE | BLE adverts + Wi-Fi promiscuous mode |
| Display | 1.14" ST7789, 135×240 | The live count |
| Input | 2 buttons (KEY1, KEY2) | Set X, mode, calibrate |
| IMU | BMI270 | Optional: auto-rotate the screen |
| Audio | ES8311 codec, mic + speaker | Optional: chime at threshold; mic-based crowd-noise signal later |
| Power | M5PM1 PMIC, battery charging | Runtime, battery gauge |
| Expansion | Grove (HY2.0-4P), Hat2 16-pin | Later: external antenna, sensors |
| Size, weight | 48 × 24 × 15 mm, 20 g | Clips anywhere |

**Constraints**

- The ESP32-S3 has **BLE only, no Bluetooth Classic**. Classic-only gear (older headphones) is invisible. Nearly all modern phones advertise over BLE, so this is acceptable.
- 2.4 GHz only. Probe requests on 5 GHz are missed.
- One radio front-end shared by Wi-Fi and BLE, so the firmware has to time-slice scans (see Counting).
- Rated 0–40 °C. A packed, hot club is fine, but don't leave it on an amp.
- Toolchain: PlatformIO + Arduino with M5Unified and M5PM1, or ESP-IDF directly.

## How counting works

We never count people directly. We count *radio-visible devices*, clean them up, and divide by a calibrated devices-per-person ratio.

&#91;embedded content: counting pipeline · 5 stages + calibration loop\]

Each stage throws away noise or identity; only the last one turns devices into people, and calibration is what makes that step honest.

### Signals

| Signal | What it sees | Strength | Weakness |
| --- | --- | --- | --- |
| BLE advertisements | Phones, watches, earbuds, trackers | Continuous; most phones advertise often | Several devices per person; addresses rotate |
| Wi-Fi probe requests | Phones scanning for networks | One mostly per phone, so closer to 1:1 | Bursty; idle or locked phones probe rarely |

Both signals are blended. BLE gives a steady base, and Wi-Fi probes help confirm the phone count.

### The hard part: MAC randomization

Modern iOS and Android rotate their BLE and Wi-Fi probe addresses, often every few minutes. Counting unique addresses over a whole night would inflate the number badly. Mitigations:

- **Short sliding window.** Count unique addresses seen in the last N minutes (start with 5), not since power-on. Rotation inside the window adds some inflation, which calibration absorbs.
- **Fingerprinting by payload, not identity.** Group adverts by manufacturer data, service UUIDs and advert interval to separate "one phone + one watch" from "two phones". Stays coarse on purpose: device type, never a person.
- **RSSI gate.** Drop signals weaker than a threshold (start near −85 dBm, tuned per venue) to exclude the street, the parking lot and the next bar.
- **Allowlist.** Mark the band's and staff's devices during setup and subtract them.
- **Ignore fixed infrastructure.** Anything present for the whole night with no rotation (POS terminals, the venue's speakers, TVs) is treated as background and dropped.

### Estimation model

`estimate = (weighted unique devices in window − background) ÷ devices-per-person ratio`

- The starting ratio is a guess (around 1.3–1.6 BLE devices per person). Real data replaces it.
- **Calibration:** at a few shows, someone does a manual count and enters it on the device. The device stores (raw signal, true count) pairs and fits the ratio per venue type.
- The display shows the estimate smoothed over about 60 seconds, so it doesn't jitter.
- Peak is the highest smoothed value held for at least 10 minutes. A single spike doesn't count.

## Functional requirements

MVP is a standalone device. Everything else waits until the estimator is proven at real shows.

**MVP (P0)**

- [ ] FR-1: Continuous BLE passive scan, time-sliced with Wi-Fi promiscuous capture on 2.4 GHz channels 1, 6 and 11
- [ ] FR-2: Hash every address with a per-night random salt (SHA-256, truncated) before it touches memory; discard raw addresses
- [ ] FR-3: Sliding-window unique-device count with RSSI gate, background removal and allowlist
- [ ] FR-4: Estimate = calibrated ratio applied to the windowed count, smoothed
- [ ] FR-5: Threshold X set on-device; persists across reboots
- [ ] FR-6: Live display of estimate, threshold status and battery
- [ ] FR-7: Night log to flash every minute: timestamp, raw counts per signal, estimate (no identifiers)
- [ ] FR-8: "Setup mode" that captures all devices currently nearby as the allowlist (band at soundcheck)

**Next (P1)**

- [ ] FR-9: Manual-count entry for calibration; per-venue-type ratio profiles
- [ ] FR-10: End-of-night summary: peak, minutes above X, start and end times
- [ ] FR-11: Export the log over USB serial as CSV, or over a Wi-Fi access point the device hosts
- [ ] FR-12: Chime or screen flash when the estimate first crosses X

**Later (P2)**

- [ ] FR-13: Web dashboard over the device's own access point, showing the curve live
- [ ] FR-14: Two or more devices in a large room, merging counts
- [ ] FR-15: Shareable night report (PNG or PDF) for the venue
- [ ] FR-16: Mic-based crowd-noise level as a second, independent signal

## UI and UX

The 135×240 screen shows one big number. Everything else is secondary and readable from about 1 m in a dark room.

**Main screen (portrait)**

- Estimate in the largest font that fits, with "est." beside it
- Status bar coloured by threshold: grey (below 80% of X), amber (80–99%), green (X or more)
- Small line: "X = 120 · peak 151"
- Battery icon and a scanning pulse, so you can tell it's alive

**Buttons**

| Action | KEY1 (front) | KEY2 (side) |
| --- | --- | --- |
| Short press | Cycle screens: count → detail → summary | Dim or wake screen |
| Long press | Enter settings | Start setup mode (allowlist capture) |
| In settings | Next item | Adjust value (X in steps of 10) |

**Detail screen:** raw BLE count, raw Wi-Fi count, ratio in use, background count, RSSI gate. For the builder, not the band.

**Design notes**

- Dark background, high-contrast digits; avoid white screens that glow on stage
- No step should need more than two button presses during a show
- Optionally pair with the ARDA visual style for boot and summary screens

## Non-functional requirements

Privacy is the requirement most likely to sink the project socially, so it is designed in from the start.

**Privacy and legal**

- Passive receive only. The device never connects to, pings or deauths anyone's device.
- Raw MAC addresses never written to flash or sent anywhere; salted hashes rotate each night and are wiped at power-off.
- Logs hold only aggregate counts.
- Ask the venue's permission before using it, and be upfront that it's running. Passive Wi-Fi and BLE sniffing for aggregate counts is widely used in retail analytics, but rules differ by state and country. Neither the band nor I am a lawyer; check before relying on it in a contract.

**Accuracy and trust**

- Always label the number "est." and keep the raw counts visible on the detail screen.
- The log shows its method (ratio, window, RSSI gate), so anyone can see how the number was made.

**Performance and power**

- Screen refresh at 1 Hz; estimate recomputed every 5 s
- 5+ hours runtime with the screen dimmed between glances; USB-C power bank as backup
- Boots to scanning in under 10 s

**Reliability**

- Survives a reboot mid-show without losing the night log or settings
- Device table capped in PSRAM (e.g. 5,000 entries, oldest evicted)

## Risks, open questions and milestones

The biggest risk is accuracy: it will take a few real shows of calibration before the number is worth showing anyone.

**Risks**

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Address rotation inflates counts | High | Short window, payload grouping, calibration |
| Phones in airplane mode, or few devices in an older crowd | Medium | Per-venue-type ratios; Wi-Fi + BLE blend |
| Signals from outside the room (street, next bar) | Medium | RSSI gate tuned at soundcheck; place device centrally |
| Venue distrusts or disputes the number | High | Position as a cross-check; transparent method; get buy-in first |
| Privacy objection from venue or patrons | Medium | Hashing, no storage, disclosure |
| Bodies absorb 2.4 GHz in a packed room, lowering RSSI | Medium | Calibrate at full-crowd shows, not empty ones |

**Open questions**

- [ ] What is X for the band's usual venues, and does the venue count capacity including staff and the band?
- [ ] Does the venue already do a door count (clicker, wristbands, ticket scans) that this should complement?
- [ ] Typical room sizes: does one device cover the room, or will bigger venues need two?
- [ ] Who will do the manual counts for calibration?

**Milestones**

1. **Bench prototype:** BLE + Wi-Fi scanning with raw counts on screen, at home
2. **Estimator v0:** windowing, hashing, RSSI gate, allowlist, logging
3. **Field test 1:** a show with a manual count every 30 min; first real ratio
4. **MVP:** threshold UI, night summary, CSV export; two more calibrated shows
5. **v1.1:** on-device web dashboard and shareable report
