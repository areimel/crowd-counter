#pragma once
// Shared fixtures for the counting-core Unity suites. Include with a relative
// path: #include "../support/CounterTestSupport.h"
#include <stdint.h>
#include <string.h>

#include "CounterTypes.h"

namespace cctest {

using namespace cc;

inline Observation makeObs(uint64_t id, int8_t rssi, Signal signal = Signal::BLE,
                           AddrKind kind = AddrKind::ROTATING, uint8_t markers = 0) {
  Observation o;
  memset(&o, 0, sizeof o);
  o.id = id;
  o.rssi = rssi;
  o.signal = signal;
  o.addrKind = kind;
  o.markers = signal == Signal::BLE ? markers : 0;
  return o;
}

inline Observation bleObs(uint64_t id, int8_t rssi, AddrKind kind, uint8_t markers) {
  return makeObs(id, rssi, Signal::BLE, kind, markers);
}

inline Observation wifiObs(uint64_t id, int8_t rssi, AddrKind kind = AddrKind::ROTATING) {
  return makeObs(id, rssi, Signal::WIFI, kind, 0);
}

// All classes weighted `w`.
inline ClassWeights uniformWeights(float w = 1.0f) {
  ClassWeights cw;
  for (uint8_t i = 0; i < CLASS_COUNT; ++i) cw.w[i] = w;
  return cw;
}

// Distinct weights per class so tests can tell which class was counted.
// PHONE 1, WEARABLE 0.5, AUDIO 0.25, TRACKER 0, COMPUTER 0.75, BEACON 0, OTHER 0.125.
inline ClassWeights distinctWeights() {
  ClassWeights cw;
  const float w[CLASS_COUNT] = {1.0f, 0.5f, 0.25f, 0.0f, 0.75f, 0.0f, 0.125f};
  for (uint8_t i = 0; i < CLASS_COUNT; ++i) cw.w[i] = w[i];
  return cw;
}

inline float weightOf(const ClassWeights& cw, DeviceClass c) { return cw.w[static_cast<uint8_t>(c)]; }

// Window 300 s, gate -85 dBm, background after 90 min, uniform weights.
inline CensusParams censusParams(uint32_t windowS = 300, int8_t gate = -85,
                                 uint32_t backgroundAfterS = 5400,
                                 ClassWeights weights = uniformWeights()) {
  CensusParams p;
  p.windowS = windowS;
  p.rssiGate = gate;
  p.backgroundAfterS = backgroundAfterS;
  p.weights = weights;
  return p;
}

// Blend 0.8, recompute 5 s, smoothing 60 s, peak hold 600 s, alert hold 30 s.
inline EstimatorParams estimatorParams(float blend = 0.8f, uint16_t recomputeS = 5,
                                       uint16_t smoothingS = 60, uint16_t peakHoldS = 600,
                                       uint16_t alertHoldS = 30) {
  EstimatorParams p;
  p.blend = blend;
  p.recomputeS = recomputeS;
  p.smoothingS = smoothingS;
  p.peakHoldS = peakHoldS;
  p.alertHoldS = alertHoldS;
  return p;
}

inline Ratios ratios(float ble = 1.0f, float wifi = 1.0f, bool wifiFitted = false) {
  Ratios r;
  r.ble = ble;
  r.wifi = wifi;
  r.wifiFitted = wifiFitted;
  return r;
}

inline Baseline noBaseline() {
  Baseline b;
  memset(&b, 0, sizeof b);
  return b;
}

inline Baseline baseline(float ble, float wifi, uint16_t fixedCount = 0) {
  Baseline b = noBaseline();
  b.ble = ble;
  b.wifi = wifi;
  b.fixedCount = fixedCount;
  b.valid = true;
  return b;
}

inline RawCounts zeroCounts() {
  RawCounts c;
  memset(&c, 0, sizeof c);
  return c;
}

// Counts whose only content is the two signals the estimator reads.
inline RawCounts counts(float bleWeighted, uint16_t wifi) {
  RawCounts c = zeroCounts();
  c.bleWeighted = bleWeighted;
  c.wifi = wifi;
  return c;
}

inline Allowlist emptyAllowlist() {
  Allowlist a;
  memset(&a, 0, sizeof a);
  return a;
}

}  // namespace cctest
