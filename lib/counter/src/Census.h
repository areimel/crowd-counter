#pragma once
#include <stdint.h>

#include "BleFingerprint.h"
#include "CounterTypes.h"
#include "DeviceTable.h"

namespace cc {
namespace census {

inline bool inWindow(const DeviceEntry& e, const CensusParams& p, uint32_t nowS) {
  return e.lastSeenS >= nowS || (nowS - e.lastSeenS) <= p.windowS;
}

inline bool atGate(const DeviceEntry& e, const CensusParams& p) { return e.rssi >= p.rssiGate; }

inline void bump(uint16_t& c) {
  if (c < 0xFFFF) ++c;
}

inline bool onAllowlist(const Allowlist& a, uint64_t id) {
  const uint16_t n = a.count < ALLOWLIST_MAX ? a.count : ALLOWLIST_MAX;
  for (uint16_t i = 0; i < n; ++i)
    if (a.ids[i] == id) return true;
  return false;
}

// Fixed-address, non-phone BLE present for backgroundAfterS or longer: venue kit.
inline bool isBackground(const DeviceEntry& e, const CensusParams& p, uint32_t nowS) {
  return e.signal == Signal::BLE && e.addrKind == AddrKind::FIXED &&
         classOf(e.markers) != DeviceClass::PHONE && (nowS > e.firstSeenS ? nowS - e.firstSeenS : 0) >= p.backgroundAfterS;
}

inline float weightFor(const DeviceEntry& e, const CensusParams& p) {
  return p.weights.w[static_cast<uint8_t>(classOf(e.markers))];
}

}  // namespace census

// One sweep of the table into counts. Filters are mutually exclusive and applied
// in order: below gate, allowlisted, background.
inline RawCounts takeCensus(const DeviceTable& t, const Allowlist& allow, const CensusParams& p,
                            uint32_t nowS) {
  RawCounts c = {};
  t.forEach([&](const DeviceEntry& e) {
    if (!census::inWindow(e, p, nowS)) return;
    const bool ble = e.signal == Signal::BLE;
    const bool rotating = e.addrKind == AddrKind::ROTATING;
    census::bump(ble ? c.bleSeen : c.wifiSeen);
    if (!census::atGate(e, p)) return census::bump(c.belowGate);
    if (census::onAllowlist(allow, e.id)) return census::bump(c.allowlisted);
    if (census::isBackground(e, p, nowS)) return census::bump(c.background);
    if (ble) {
      census::bump(c.ble[static_cast<uint8_t>(classOf(e.markers))]);
      const float w = census::weightFor(e, p);
      c.bleWeighted += w;
      if (rotating) c.bleWeightedRotating += w;
    } else {
      census::bump(c.wifi);
      if (rotating) census::bump(c.wifiRotating);
    }
  });
  return c;
}

// Setup capture: fixed-address devices become the allowlist, rotating ones the
// baseline. Background and allowlist filters do not apply here.
inline void captureSetup(const DeviceTable& t, const CensusParams& p, uint32_t nowS,
                         Allowlist& outAllow, Baseline& outBase) {
  outAllow = {};
  outBase = {};
  t.forEach([&](const DeviceEntry& e) {
    if (!census::inWindow(e, p, nowS) || !census::atGate(e, p)) return;
    if (e.addrKind == AddrKind::FIXED) {
      // Beyond ALLOWLIST_MAX the extras are not stored and not in fixedCount, so
      // they keep being counted as people at census time.
      if (outAllow.count < ALLOWLIST_MAX) outAllow.ids[outAllow.count++] = e.id;
    } else if (e.signal == Signal::BLE) {
      outBase.ble += census::weightFor(e, p);
    } else {
      outBase.wifi += 1.0f;
    }
  });
  outBase.fixedCount = outAllow.count;
  outBase.valid = true;
}

}  // namespace cc
