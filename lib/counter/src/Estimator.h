#pragma once
// Pure counting core: signal -> people estimate, smoothing, held peak,
// threshold state and the once-per-night alert. No Arduino, no heap.
#include <math.h>
#include <stdint.h>

#include "CounterTypes.h"

namespace cc {

class Estimator {
 public:
  static constexpr uint16_t SMOOTH_MAX = 300;   // ring slots (updates)
  static constexpr uint16_t PEAK_MAX = 1800;    // ring slots (updates)

  explicit Estimator(const EstimatorParams& p) : p_(p) {
    if (p_.recomputeS == 0) p_.recomputeS = 1;
    smoothN_ = clampSlots(p_.smoothingS / p_.recomputeS, SMOOTH_MAX);
    peakK_ = clampSlots(p_.peakHoldS / p_.recomputeS, PEAK_MAX);
    reset();
  }

  EstimateSnapshot update(const RawCounts& c, const Baseline& b, const Ratios& r,
                          uint16_t threshold, bool warmingUp) {
    EstimateSnapshot s;
    s.bleSignal = signal(c.bleWeighted, b.valid ? b.ble : 0.0f);
    s.wifiSignal = signal(static_cast<float>(c.wifi), b.valid ? b.wifi : 0.0f);

    const float rb = ratio(r.ble);
    float inst = s.bleSignal / rb;
    if (r.wifiFitted) {
      float w = p_.blend;
      if (!isfinite(w)) w = 1.0f;
      w = w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w);
      inst = w * inst + (1.0f - w) * s.wifiSignal / ratio(r.wifi);
    }
    if (!isfinite(inst)) inst = 0.0f;
    s.instant = toU16(inst);

    smooth_[smoothHead_] = inst;
    smoothHead_ = static_cast<uint16_t>((smoothHead_ + 1) % smoothN_);
    if (smoothCount_ < smoothN_) ++smoothCount_;
    float sum = 0.0f;
    for (uint16_t i = 0; i < smoothCount_; ++i) sum += smooth_[i];  // order-free
    s.smoothed = toU16(sum / smoothCount_);

    const bool above = threshold > 0 && s.smoothed >= threshold;
    if (warmingUp) {
      peakCount_ = peakHead_ = 0;
      run_ = 0;
    } else {
      peakRing_[peakHead_] = s.smoothed;
      peakHead_ = static_cast<uint16_t>((peakHead_ + 1) % peakK_);
      if (peakCount_ < peakK_) ++peakCount_;
      if (peakCount_ == peakK_) {
        uint16_t lo = peakRing_[0];
        for (uint16_t i = 1; i < peakK_; ++i) lo = peakRing_[i] < lo ? peakRing_[i] : lo;
        if (lo > st_.peak) st_.peak = lo;
      }
      if (above) {
        st_.secondsAbove += p_.recomputeS;
        if (run_ < 0xFFFFFFFFu - p_.recomputeS) run_ += p_.recomputeS;
      } else {
        run_ = 0;
      }
    }

    s.alertNow = false;
    if (!warmingUp && above && !st_.alertLatched && run_ >= p_.alertHoldS) {
      st_.alertLatched = true;
      s.alertNow = true;
    }

    if (threshold == 0) s.state = ThresholdState::BELOW;
    else if (above) s.state = ThresholdState::ABOVE;
    else if (static_cast<uint32_t>(s.smoothed) * 5u >= static_cast<uint32_t>(threshold) * 4u)
      s.state = ThresholdState::NEAR;
    else s.state = ThresholdState::BELOW;

    s.peak = st_.peak;
    const uint32_t min = st_.secondsAbove / 60u;
    s.minutesAbove = min > 65535u ? 65535u : static_cast<uint16_t>(min);
    s.warmingUp = warmingUp;
    return s;
  }

  EstimatorState state() const { return st_; }

  // After a crash reset: keep peak, time above and the latch; drop the rings.
  void restore(const EstimatorState& s) {
    clearRings();
    st_ = s;
  }

  // New night: everything zero.
  void reset() {
    clearRings();
    st_.peak = 0;
    st_.secondsAbove = 0;
    st_.alertLatched = false;
  }

 private:
  static uint16_t clampSlots(uint32_t n, uint16_t cap) {
    return n < 1 ? 1 : (n > cap ? cap : static_cast<uint16_t>(n));
  }
  static float signal(float count, float base) {
    if (!isfinite(count)) count = 0.0f;
    if (!isfinite(base)) base = 0.0f;
    const float v = count - base;
    return v > 0.0f ? v : 0.0f;
  }
  static float ratio(float r) { return (isfinite(r) && r > 0.0f) ? r : 1.0f; }
  static uint16_t toU16(float v) {
    if (!(v > 0.0f)) return 0;
    v += 0.5f;
    return v >= 65535.0f ? 65535 : static_cast<uint16_t>(v);
  }
  void clearRings() {
    smoothHead_ = smoothCount_ = peakHead_ = peakCount_ = 0;
    run_ = 0;
  }

  EstimatorParams p_;
  EstimatorState st_{};
  uint16_t smoothN_ = 1, peakK_ = 1;
  uint16_t smoothHead_ = 0, smoothCount_ = 0, peakHead_ = 0, peakCount_ = 0;
  uint32_t run_ = 0;
  float smooth_[SMOOTH_MAX] = {};
  uint16_t peakRing_[PEAK_MAX] = {};
};

}  // namespace cc
