#include <math.h>
#include <string.h>
#include <unity.h>

#include "../support/CounterTestSupport.h"
#include "Calibration.h"

using namespace cc;
using cctest::ratios;

void setUp() {}
void tearDown() {}

static CalProfile empty() {
  CalProfile p;
  memset(&p, 0, sizeof p);
  return p;
}

static CalPair pr(float ble, float wifi, uint16_t truth) { return CalPair{ble, wifi, truth}; }

static const Ratios DEF = ratios(2.0f, 3.0f, false);

void test_empty_gives_defaults() {
  Ratios r = fitRatios(empty(), DEF);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, r.ble);
  TEST_ASSERT_EQUAL_FLOAT(3.0f, r.wifi);
  TEST_ASSERT_FALSE(r.wifiFitted);
}

void test_empty_keeps_fitted_default() {
  Ratios r = fitRatios(empty(), ratios(2.0f, 3.0f, true));
  TEST_ASSERT_TRUE(r.wifiFitted);
}

void test_one_pair_ble_is_raw_over_truth() {
  CalProfile p = empty();
  addPair(p, pr(30.0f, 0.0f, 20));
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, r.ble);
  TEST_ASSERT_EQUAL_FLOAT(3.0f, r.wifi);
  TEST_ASSERT_FALSE(r.wifiFitted);
}

void test_exact_proportional_fit() {
  CalProfile p = empty();
  addPair(p, pr(20, 15, 10));
  addPair(p, pr(40, 30, 20));
  addPair(p, pr(80, 60, 40));
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, r.ble);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, r.wifi);
  TEST_ASSERT_TRUE(r.wifiFitted);
}

void test_least_squares_noisy() {
  CalProfile p = empty();
  addPair(p, pr(12, 0, 10));
  addPair(p, pr(18, 0, 20));
  addPair(p, pr(36, 0, 30));
  // (120 + 360 + 1080) / (100 + 400 + 900) = 1560 / 1400
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.1142857f, r.ble);
}

void test_truth_zero_ignored() {
  CalProfile p = empty();
  addPair(p, pr(99, 99, 0));
  addPair(p, pr(20, 10, 10));
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, r.ble);
  addPair(p, pr(5, 5, 0));
  r = fitRatios(p, DEF);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, r.ble);
}

void test_only_truth_zero_gives_defaults() {
  CalProfile p = empty();
  addPair(p, pr(10, 10, 0));
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, r.ble);
}

void test_clamp_low_and_high() {
  CalProfile p = empty();
  addPair(p, pr(1, 1, 10));
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_EQUAL_FLOAT(RATIO_MIN, r.ble);
  CalProfile q = empty();
  addPair(q, pr(100, 100, 10));
  r = fitRatios(q, DEF);
  TEST_ASSERT_EQUAL_FLOAT(RATIO_MAX, r.ble);
}

void test_wifi_clamped() {
  CalProfile p = empty();
  for (int i = 0; i < 3; ++i) addPair(p, pr(10, 1000, 10));
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_EQUAL_FLOAT(RATIO_MAX, r.wifi);
  TEST_ASSERT_TRUE(r.wifiFitted);
}

void test_wifi_needs_three_pairs() {
  CalProfile p = empty();
  addPair(p, pr(20, 15, 10));
  addPair(p, pr(40, 30, 20));
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_FALSE(r.wifiFitted);
  TEST_ASSERT_EQUAL_FLOAT(3.0f, r.wifi);
  addPair(p, pr(80, 60, 40));
  r = fitRatios(p, DEF);
  TEST_ASSERT_TRUE(r.wifiFitted);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, r.wifi);
}

void test_wifi_zero_pairs_do_not_qualify() {
  CalProfile p = empty();
  addPair(p, pr(20, 15, 10));
  addPair(p, pr(40, 30, 20));
  addPair(p, pr(80, 0, 40));
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_FALSE(r.wifiFitted);
  TEST_ASSERT_EQUAL_FLOAT(3.0f, r.wifi);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, r.ble);
}

void test_wifi_truth_zero_does_not_qualify() {
  CalProfile p = empty();
  addPair(p, pr(20, 15, 10));
  addPair(p, pr(40, 30, 20));
  addPair(p, pr(80, 60, 0));
  TEST_ASSERT_FALSE(fitRatios(p, DEF).wifiFitted);
}

void test_ring_wraps_after_sixteen() {
  CalProfile p = empty();
  addPair(p, pr(10, 10, 10));  // ratio 1, oldest
  for (int i = 0; i < 16; ++i) addPair(p, pr(40, 30, 20));  // ratio 2 / 1.5
  TEST_ASSERT_EQUAL_UINT8(CAL_PAIRS_MAX, p.count);
  TEST_ASSERT_EQUAL_UINT8(1, p.next);
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, r.ble);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, r.wifi);
}

void test_ring_counts_and_next_advance() {
  CalProfile p = empty();
  for (int i = 0; i < 5; ++i) addPair(p, pr(1, 1, 1));
  TEST_ASSERT_EQUAL_UINT8(5, p.count);
  TEST_ASSERT_EQUAL_UINT8(5, p.next);
  for (int i = 0; i < 11; ++i) addPair(p, pr(1, 1, 1));
  TEST_ASSERT_EQUAL_UINT8(16, p.count);
  TEST_ASSERT_EQUAL_UINT8(0, p.next);
}

void test_corrupt_count_recovered() {
  CalProfile p = empty();
  p.count = 200;
  p.next = 3;
  addPair(p, pr(20, 10, 10));
  TEST_ASSERT_EQUAL_UINT8(1, p.count);
  TEST_ASSERT_EQUAL_UINT8(1, p.next);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, p.pairs[0].ble);
}

void test_corrupt_next_recovered() {
  CalProfile p = empty();
  p.count = 4;
  p.next = 99;
  addPair(p, pr(20, 10, 10));
  TEST_ASSERT_EQUAL_UINT8(1, p.count);
  TEST_ASSERT_EQUAL_UINT8(1, p.next);
}

void test_fit_tolerates_corrupt_count() {
  CalProfile p = empty();
  for (int i = 0; i < 16; ++i) p.pairs[i] = pr(20, 15, 10);
  p.count = 255;
  Ratios r = fitRatios(p, DEF);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, r.ble);
}

void test_bad_pairs_rejected() {
  CalProfile p = empty();
  addPair(p, pr(NAN, 1, 10));
  addPair(p, pr(1, NAN, 10));
  addPair(p, pr(INFINITY, 1, 10));
  addPair(p, pr(1, INFINITY, 10));
  addPair(p, pr(-1, 1, 10));
  addPair(p, pr(1, -1, 10));
  TEST_ASSERT_EQUAL_UINT8(0, p.count);
  TEST_ASSERT_EQUAL_UINT8(0, p.next);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_gives_defaults);
  RUN_TEST(test_empty_keeps_fitted_default);
  RUN_TEST(test_one_pair_ble_is_raw_over_truth);
  RUN_TEST(test_exact_proportional_fit);
  RUN_TEST(test_least_squares_noisy);
  RUN_TEST(test_truth_zero_ignored);
  RUN_TEST(test_only_truth_zero_gives_defaults);
  RUN_TEST(test_clamp_low_and_high);
  RUN_TEST(test_wifi_clamped);
  RUN_TEST(test_wifi_needs_three_pairs);
  RUN_TEST(test_wifi_zero_pairs_do_not_qualify);
  RUN_TEST(test_wifi_truth_zero_does_not_qualify);
  RUN_TEST(test_ring_wraps_after_sixteen);
  RUN_TEST(test_ring_counts_and_next_advance);
  RUN_TEST(test_corrupt_count_recovered);
  RUN_TEST(test_corrupt_next_recovered);
  RUN_TEST(test_fit_tolerates_corrupt_count);
  RUN_TEST(test_bad_pairs_rejected);
  return UNITY_END();
}
