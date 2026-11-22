#pragma once
#include <stdint.h>

#include "CounterTypes.h"

// Night state in RTC no-init memory: survives a crash or soft reset, gone at
// power-off. Holds the per-night salt and allowlist; this is the ONLY place the
// salt lives. save() is called from the main loop only (single writer).
namespace cc {
namespace rtc {

// False on cold boot, power-on or corruption (out untouched).
bool load(NightState& out);
void save(const NightState& s);
// Wipes the whole record, salt included, and invalidates it.
void clear();
// Call only after the radio is started: esp_fill_random is a true RNG only
// once Wi-Fi/BT is running (RF entropy).
void freshSalt(uint8_t out[16]);
int resetReason();              // raw esp_reset_reason()
const char* resetReasonName();  // "POWERON", "SW", "PANIC", ... "UNKNOWN"

}  // namespace rtc
}  // namespace cc
