#pragma once
#include <stdint.h>

#include "CounterTypes.h"

// Night state in RTC no-init memory: survives a crash or soft reset, gone at
// power-off. Holds the per-night salt and allowlist; this is the ONLY place the
// salt lives. save() is called from the main loop only (single writer).
namespace cc {
namespace rtc {

// True only after a restart that keeps power (SW, panic, watchdogs, CPU lockup,
// USB, JTAG) with a valid record. Any other reset reason (power-on, external,
// brownout, deep sleep, glitch, unknown) wipes the record and returns false, as
// does corruption (out untouched).
bool load(NightState& out);
void save(const NightState& s);
// Wipes the whole record, salt included, and invalidates it.
void clear();
// Call only after the radio is started: esp_fill_random is a true RNG only
// once Wi-Fi/BT is running (RF entropy).
void freshSalt(uint8_t out[16]);
const char* resetReasonName();  // "POWERON", "SW", "PANIC", ... "UNKNOWN"

}  // namespace rtc
}  // namespace cc
