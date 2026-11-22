#include <unity.h>
#include <math.h>

#include "../support/CounterTestSupport.h"
#include "Estimator.h"

using namespace cc;
using namespace cctest;

void setUp() {}
void tearDown() {}

static EstimateSnapshot step(Estimator& e, float ble, uint16_t thr = 0, bool warm = false,
                             const Ratios& r = ratios(), const Baseline& b = noBaseline()) {
  return e.update(counts(ble, 0), b, r, thr, warm);
}

void test_signal_without_baseline() {
  Estimator e(estimatorParams());
  EstimateSnapshot s = e.update(counts(10, 4), noBaseline(), ratios(), 0, false);
  TEST_ASSERT_EQUAL_FLOAT(10, s.bleSignal);
  TEST_ASSERT_EQUAL_FLOAT(4, s.wifiSignal);
}

void test_signal_with_baseline_and_clamp() {
  Estimator e(estimatorParams());
  EstimateSnapshot s = e.update(counts(10, 4), baseline(3, 6), ratios(), 0, false);
  TEST_ASSERT_EQUAL_FLOAT(7, s.bleSignal);
  TEST_ASSERT_EQUAL_FLOAT(0, s.wifiSignal);
}

void test_nan_counts_are_zero() {
  Estimator e(estimatorParams());
  RawCounts c = counts(NAN, 0);
  c.bleWeighted = NAN;
  EstimateSnapshot s = e.update(c, baseline(NAN, 0), ratios(), 0, false);
  TEST_ASSERT_EQUAL_FLOAT(0, s.bleSignal);
  TEST_ASSERT_EQUAL_UINT16(0, s.instant);
}

void test_ble_only_when_wifi_not_fitted() {
  Estimator e(estimatorParams());
  EstimateSnapshot s = e.update(counts(20, 100), noBaseline(), ratios(2, 1, false), 0, false);
  TEST_ASSERT_EQUAL_UINT16(10, s.instant);
}

void test_blend_when_fitted() {
  Estimator e(estimatorParams(0.5f));
  EstimateSnapshot s = e.update(counts(20, 30), noBaseline(), ratios(2, 3, true), 0, false);
  TEST_ASSERT_EQUAL_UINT16(10, s.instant);  // .5*10 + .5*10
  Estimator f(estimatorParams(0.8f));
  s = f.update(counts(20, 30), noBaseline(), ratios(2, 10, true), 0, false);
  TEST_ASSERT_EQUAL_UINT16(9, s.instant);  // .8*10 + .2*3 = 8.6
}

void test_blend_clamped() {
  Estimator hi(estimatorParams(5.0f));
  TEST_ASSERT_EQUAL_UINT16(10, hi.update(counts(10, 100), noBaseline(), ratios(1, 1, true), 0, false).instant);
  Estimator lo(estimatorParams(-1.0f));
  TEST_ASSERT_EQUAL_UINT16(100, lo.update(counts(10, 100), noBaseline(), ratios(1, 1, true), 0, false).instant);
  Estimator nn(estimatorParams(NAN));
  TEST_ASSERT_EQUAL_UINT16(10, nn.update(counts(10, 100), noBaseline(), ratios(1, 1, true), 0, false).instant);
}

void test_ratio_guards() {
  const float bad[] = {0.0f, -2.0f, NAN};
  for (float v : bad) {
    Estimator e(estimatorParams());
    TEST_ASSERT_EQUAL_UINT16(10, e.update(counts(10, 0), noBaseline(), ratios(v, 1, false), 0, false).instant);
    Estimator f(estimatorParams(0.0f));
    TEST_ASSERT_EQUAL_UINT16(10, f.update(counts(0, 10), noBaseline(), ratios(1, v, true), 0, false).instant);
  }
}

void test_rounding_half_up_and_clamp() {
  Estimator e(estimatorParams(0.8f, 5, 5));
  TEST_ASSERT_EQUAL_UINT16(3, step(e, 2.5f).instant);
  Estimator f(estimatorParams(0.8f, 5, 5));
  TEST_ASSERT_EQUAL_UINT16(2, step(f, 2.49f).instant);
  Estimator g(estimatorParams(0.8f, 5, 5));
  EstimateSnapshot s = step(g, 1e9f);
  TEST_ASSERT_EQUAL_UINT16(65535, s.instant);
  TEST_ASSERT_EQUAL_UINT16(65535, s.smoothed);
}

void test_smoothing_step_and_partial_fill() {
  Estimator e(estimatorParams(0.8f, 5, 20));  // n = 4
  TEST_ASSERT_EQUAL_UINT16(0, step(e, 0).smoothed);
  TEST_ASSERT_EQUAL_UINT16(20, step(e, 40).smoothed);  // mean(0,40)
  TEST_ASSERT_EQUAL_UINT16(27, step(e, 40).smoothed);  // 26.67
  TEST_ASSERT_EQUAL_UINT16(30, step(e, 40).smoothed);
  TEST_ASSERT_EQUAL_UINT16(40, step(e, 40).smoothed);  // 0 dropped out
  TEST_ASSERT_EQUAL_UINT16(40, step(e, 40).smoothed);
}

void test_smoothing_includes_warmup() {
  Estimator e(estimatorParams(0.8f, 5, 10));
  step(e, 10, 0, true);
  TEST_ASSERT_EQUAL_UINT16(20, step(e, 30, 0, true).smoothed);
}

void test_peak_requires_full_span() {
  Estimator e(estimatorParams(0.8f, 5, 5, 15));  // k = 3
  TEST_ASSERT_EQUAL_UINT16(0, step(e, 50).peak);
  TEST_ASSERT_EQUAL_UINT16(0, step(e, 50).peak);
  TEST_ASSERT_EQUAL_UINT16(50, step(e, 50).peak);
}

void test_peak_spike_ignored_plateau_counts() {
  Estimator e(estimatorParams(0.8f, 5, 5, 15));
  step(e, 10); step(e, 10);
  TEST_ASSERT_EQUAL_UINT16(10, step(e, 10).peak);
  TEST_ASSERT_EQUAL_UINT16(10, step(e, 90).peak);  // span 10,10,90 -> min 10
  TEST_ASSERT_EQUAL_UINT16(10, step(e, 10).peak);  // 10,90,10
  step(e, 60); step(e, 70);
  TEST_ASSERT_EQUAL_UINT16(60, step(e, 80).peak);  // min(60,70,80)
  TEST_ASSERT_EQUAL_UINT16(60, step(e, 5).peak);   // peak never falls
}

void test_warmup_excluded_from_peak_and_restarts_span() {
  Estimator e(estimatorParams(0.8f, 5, 5, 15));
  step(e, 10); step(e, 10);
  step(e, 10);
  TEST_ASSERT_EQUAL_UINT16(10, e.state().peak);
  EstimateSnapshot s = step(e, 99, 0, true);
  TEST_ASSERT_EQUAL_UINT16(10, s.peak);
  TEST_ASSERT_TRUE(s.warmingUp);
  TEST_ASSERT_EQUAL_UINT16(10, step(e, 99).peak);  // span restarted: 1 of 3
  TEST_ASSERT_EQUAL_UINT16(10, step(e, 99).peak);
  TEST_ASSERT_EQUAL_UINT16(99, step(e, 99).peak);
}

void test_threshold_states() {
  Estimator e(estimatorParams(0.8f, 5, 5));
  TEST_ASSERT_EQUAL(ThresholdState::BELOW, step(e, 79, 100).state);
  TEST_ASSERT_EQUAL(ThresholdState::NEAR, step(e, 80, 100).state);
  TEST_ASSERT_EQUAL(ThresholdState::NEAR, step(e, 99, 100).state);
  TEST_ASSERT_EQUAL(ThresholdState::ABOVE, step(e, 100, 100).state);
  TEST_ASSERT_EQUAL(ThresholdState::NEAR, step(e, 100, 101).state);
  TEST_ASSERT_EQUAL(ThresholdState::ABOVE, step(e, 100, 100, true).state);  // during warm-up
}

void test_threshold_zero_inert() {
  Estimator e(estimatorParams(0.8f, 5, 5, 5, 0));
  for (int i = 0; i < 5; ++i) {
    EstimateSnapshot s = step(e, 500, 0);
    TEST_ASSERT_EQUAL(ThresholdState::BELOW, s.state);
    TEST_ASSERT_FALSE(s.alertNow);
  }
  TEST_ASSERT_EQUAL_UINT32(0, e.state().secondsAbove);
  TEST_ASSERT_FALSE(e.state().alertLatched);
}

void test_seconds_and_minutes_above() {
  Estimator e(estimatorParams(0.8f, 5, 5));
  EstimateSnapshot s;
  for (int i = 0; i < 12; ++i) s = step(e, 100, 100);
  TEST_ASSERT_EQUAL_UINT32(60, e.state().secondsAbove);
  TEST_ASSERT_EQUAL_UINT16(1, s.minutesAbove);
  step(e, 100, 100, true);  // warm-up: not counted
  TEST_ASSERT_EQUAL_UINT32(60, e.state().secondsAbove);
  step(e, 0, 100);
  TEST_ASSERT_EQUAL_UINT32(60, e.state().secondsAbove);
}

void test_alert_fires_once_on_sixth() {
  Estimator e(estimatorParams(0.8f, 5, 5, 600, 30));
  for (int i = 1; i <= 5; ++i) TEST_ASSERT_FALSE(step(e, 100, 100).alertNow);
  TEST_ASSERT_TRUE(step(e, 100, 100).alertNow);
  TEST_ASSERT_TRUE(e.state().alertLatched);
  TEST_ASSERT_FALSE(step(e, 100, 100).alertNow);
  step(e, 0, 100);
  for (int i = 0; i < 8; ++i) TEST_ASSERT_FALSE(step(e, 100, 100).alertNow);
}

void test_alert_hold_zero_fires_first() {
  Estimator e(estimatorParams(0.8f, 5, 5, 600, 0));
  TEST_ASSERT_TRUE(step(e, 100, 100).alertNow);
}

void test_alert_run_resets_on_drop_and_warmup() {
  Estimator e(estimatorParams(0.8f, 5, 5, 600, 30));
  for (int i = 0; i < 5; ++i) step(e, 100, 100);
  step(e, 0, 100);  // drop
  for (int i = 0; i < 5; ++i) TEST_ASSERT_FALSE(step(e, 100, 100).alertNow);
  TEST_ASSERT_FALSE(step(e, 100, 100, true).alertNow);  // warm-up resets
  for (int i = 0; i < 5; ++i) TEST_ASSERT_FALSE(step(e, 100, 100).alertNow);
  TEST_ASSERT_TRUE(step(e, 100, 100).alertNow);
}

void test_restore_keeps_latch_and_peak_clears_rings() {
  Estimator e(estimatorParams(0.8f, 5, 5, 15, 0));
  step(e, 40, 10); step(e, 40, 10);
  step(e, 40, 10);
  EstimatorState st = e.state();
  TEST_ASSERT_EQUAL_UINT16(40, st.peak);
  TEST_ASSERT_TRUE(st.alertLatched);
  Estimator f(estimatorParams(0.8f, 5, 5, 15, 0));
  f.restore(st);
  EstimateSnapshot s = step(f, 40, 10);
  TEST_ASSERT_FALSE(s.alertNow);
  TEST_ASSERT_EQUAL_UINT16(40, s.peak);
  TEST_ASSERT_EQUAL_UINT32(st.secondsAbove + 5, f.state().secondsAbove);
  // hold span cleared: a high value needs 3 fresh samples
  step(f, 90, 10);
  TEST_ASSERT_EQUAL_UINT16(40, step(f, 90, 10).peak);
  TEST_ASSERT_EQUAL_UINT16(90, step(f, 90, 10).peak);
}

void test_reset_clears() {
  Estimator e(estimatorParams(0.8f, 5, 5, 5, 0));
  step(e, 80, 10);
  e.reset();
  EstimatorState st = e.state();
  TEST_ASSERT_EQUAL_UINT16(0, st.peak);
  TEST_ASSERT_EQUAL_UINT32(0, st.secondsAbove);
  TEST_ASSERT_FALSE(st.alertLatched);
  EstimateSnapshot s = step(e, 20, 10);
  TEST_ASSERT_TRUE(s.alertNow);
  TEST_ASSERT_EQUAL_UINT16(20, s.smoothed);  // smoothing history cleared
}

void test_state_round_trip() {
  Estimator e(estimatorParams());
  EstimatorState in;
  in.peak = 123; in.secondsAbove = 4567; in.alertLatched = true;
  e.restore(in);
  EstimatorState out = e.state();
  TEST_ASSERT_EQUAL_UINT16(123, out.peak);
  TEST_ASSERT_EQUAL_UINT32(4567, out.secondsAbove);
  TEST_ASSERT_TRUE(out.alertLatched);
}

void test_minutes_above_saturates() {
  Estimator e(estimatorParams());
  EstimatorState in;
  in.peak = 0; in.secondsAbove = 60u * 70000u; in.alertLatched = true;
  e.restore(in);
  TEST_ASSERT_EQUAL_UINT16(65535, step(e, 100, 10).minutesAbove);
}

void test_params_clamped() {
  Estimator z(estimatorParams(0.8f, 0, 3, 3, 0));  // recomputeS 0 -> 1
  step(z, 10, 5);
  TEST_ASSERT_EQUAL_UINT32(1, z.state().secondsAbove);
  Estimator big(estimatorParams(0.8f, 1, 1000, 5000, 30));  // n 300, k 1800
  EstimateSnapshot s;
  for (int i = 0; i < 400; ++i) s = step(big, i < 100 ? 0.0f : 30.0f);
  // last 300 updates: indices 100..399 all 30
  TEST_ASSERT_EQUAL_UINT16(30, s.smoothed);
  for (int i = 0; i < 2000; ++i) s = step(big, 30);
  TEST_ASSERT_EQUAL_UINT16(30, s.peak);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_signal_without_baseline);
  RUN_TEST(test_signal_with_baseline_and_clamp);
  RUN_TEST(test_nan_counts_are_zero);
  RUN_TEST(test_ble_only_when_wifi_not_fitted);
  RUN_TEST(test_blend_when_fitted);
  RUN_TEST(test_blend_clamped);
  RUN_TEST(test_ratio_guards);
  RUN_TEST(test_rounding_half_up_and_clamp);
  RUN_TEST(test_smoothing_step_and_partial_fill);
  RUN_TEST(test_smoothing_includes_warmup);
  RUN_TEST(test_peak_requires_full_span);
  RUN_TEST(test_peak_spike_ignored_plateau_counts);
  RUN_TEST(test_warmup_excluded_from_peak_and_restarts_span);
  RUN_TEST(test_threshold_states);
  RUN_TEST(test_threshold_zero_inert);
  RUN_TEST(test_seconds_and_minutes_above);
  RUN_TEST(test_alert_fires_once_on_sixth);
  RUN_TEST(test_alert_hold_zero_fires_first);
  RUN_TEST(test_alert_run_resets_on_drop_and_warmup);
  RUN_TEST(test_restore_keeps_latch_and_peak_clears_rings);
  RUN_TEST(test_reset_clears);
  RUN_TEST(test_state_round_trip);
  RUN_TEST(test_minutes_above_saturates);
  RUN_TEST(test_params_clamped);
  return UNITY_END();
}
