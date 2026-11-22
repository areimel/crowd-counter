#pragma once
// Calibration: ring of (signal, manual count) pairs and the per-venue ratio fit.
// Pure logic: no Arduino, no heap.
#include <math.h>
#include <stdint.h>

#include "CounterTypes.h"

namespace cc {

constexpr float RATIO_MIN = 0.3f;
constexpr float RATIO_MAX = 5.0f;
constexpr uint8_t WIFI_FIT_MIN_PAIRS = 3;

// Append to the ring, overwriting the oldest. Non-finite or negative signals are
// rejected. A corrupt blob (count/next out of range) is treated as empty first.
inline void addPair(CalProfile& p, const CalPair& pair) {
  if (!isfinite(pair.ble) || !isfinite(pair.wifi) || pair.ble < 0.0f || pair.wifi < 0.0f) return;
  if (p.next >= CAL_PAIRS_MAX || p.count > CAL_PAIRS_MAX) {
    p.count = 0;
    p.next = 0;
  }
  p.pairs[p.next] = pair;
  p.next = static_cast<uint8_t>((p.next + 1) % CAL_PAIRS_MAX);
  if (p.count < CAL_PAIRS_MAX) ++p.count;
}

namespace detail {
inline float clampRatio(double num, double den, float fallback) {
  if (!(den > 0.0)) return fallback;
  const double r = num / den;
  if (!isfinite(r)) return fallback;
  if (r < RATIO_MIN) return RATIO_MIN;
  if (r > RATIO_MAX) return RATIO_MAX;
  return static_cast<float>(r);
}
}  // namespace detail

// Least squares through the origin, BLE and Wi-Fi independently.
inline Ratios fitRatios(const CalProfile& p, const Ratios& defaults) {
  const uint8_t n = p.count < CAL_PAIRS_MAX ? p.count : CAL_PAIRS_MAX;
  double bleXY = 0, bleYY = 0, wifiXY = 0, wifiYY = 0;
  uint8_t bleN = 0, wifiN = 0;
  for (uint8_t i = 0; i < n; ++i) {
    const CalPair& c = p.pairs[i];
    if (c.truth == 0) continue;
    const double t = c.truth;
    bleXY += static_cast<double>(c.ble) * t;
    bleYY += t * t;
    ++bleN;
    // A count taken while Wi-Fi heard nothing is still evidence (of a low
    // ratio), so every pair enters the Wi-Fi fit.
    wifiXY += static_cast<double>(c.wifi) * t;
    wifiYY += t * t;
    if (c.wifi > 0.0f) ++wifiN;
  }
  Ratios out = defaults;
  if (bleN >= 1) out.ble = detail::clampRatio(bleXY, bleYY, defaults.ble);
  // Wi-Fi is trusted once there are enough counts and it heard something.
  if (bleN >= WIFI_FIT_MIN_PAIRS && wifiN >= 1) {
    const float w = detail::clampRatio(wifiXY, wifiYY, defaults.wifi);
    out.wifi = w;
    out.wifiFitted = true;
  }
  return out;
}

}  // namespace cc
