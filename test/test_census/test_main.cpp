#include <unity.h>

#include <set>
#include <vector>

#include "../support/CounterTestSupport.h"
#include "Census.h"

using namespace cc;
using namespace cctest;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint32_t kNow = 10000;
DeviceEntry g_slots[256];

struct Fixture {
  DeviceTable t{g_slots, 256, 100};
  CensusParams p = censusParams();
  Allowlist allow = emptyAllowlist();
};
}  // namespace

void test_empty_table_is_all_zero() {
  Fixture f;
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  RawCounts z = zeroCounts();
  TEST_ASSERT_EQUAL_MEMORY(&z, &c, sizeof c);
}

void test_window_boundary() {
  Fixture f;
  f.t.observe(wifiObs(1, -50), kNow - 300);  // at the boundary: in
  f.t.observe(wifiObs(2, -50), kNow - 301);  // just outside
  f.t.observe(wifiObs(3, -50), kNow + 5);    // newer than now: in
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(2, c.wifiSeen);
  TEST_ASSERT_EQUAL_UINT16(2, c.wifi);
}

void test_seen_counts_by_signal() {
  Fixture f;
  f.t.observe(makeObs(1, -50, Signal::BLE), kNow);
  f.t.observe(makeObs(2, -90, Signal::BLE), kNow);  // below gate still seen
  f.t.observe(wifiObs(3, -50), kNow);
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(2, c.bleSeen);
  TEST_ASSERT_EQUAL_UINT16(1, c.wifiSeen);
}

void test_below_gate() {
  Fixture f;
  f.t.observe(makeObs(1, -85), kNow);  // at gate: counted
  f.t.observe(makeObs(2, -86), kNow);  // below
  f.t.observe(wifiObs(3, -86), kNow);
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(2, c.belowGate);
  TEST_ASSERT_EQUAL_UINT16(1, c.ble[static_cast<uint8_t>(DeviceClass::OTHER)]);
  TEST_ASSERT_EQUAL_UINT16(0, c.wifi);
}

void test_filter_order_gate_before_allowlist() {
  Fixture f;
  f.t.observe(bleObs(1, -90, AddrKind::FIXED, 0), kNow);
  f.allow.ids[0] = 1;
  f.allow.count = 1;
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(1, c.belowGate);
  TEST_ASSERT_EQUAL_UINT16(0, c.allowlisted);
}

void test_filter_order_allowlist_before_background() {
  Fixture f;
  f.t.observe(bleObs(1, -50, AddrKind::FIXED, marker::BEACON), 0);
  f.t.observe(bleObs(1, -50, AddrKind::FIXED, marker::BEACON), kNow);
  f.allow.ids[0] = 1;
  f.allow.count = 1;
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(1, c.allowlisted);
  TEST_ASSERT_EQUAL_UINT16(0, c.background);
}

void test_allowlist_effect() {
  Fixture f;
  f.t.observe(bleObs(1, -50, AddrKind::FIXED, marker::PHONE), kNow);
  f.t.observe(wifiObs(2, -50, AddrKind::FIXED), kNow);
  f.t.observe(bleObs(3, -50, AddrKind::FIXED, marker::PHONE), kNow);
  f.allow.ids[0] = 1;
  f.allow.ids[1] = 2;
  f.allow.count = 2;
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(2, c.allowlisted);
  TEST_ASSERT_EQUAL_UINT16(1, c.ble[static_cast<uint8_t>(DeviceClass::PHONE)]);
  TEST_ASSERT_EQUAL_UINT16(0, c.wifi);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, c.bleWeighted);
}

void test_allowlist_count_clamped() {
  Fixture f;
  f.t.observe(makeObs(5, -50), kNow);
  f.allow.count = 60000;  // corrupt count must not read past the array
  for (uint16_t i = 0; i < ALLOWLIST_MAX; ++i) f.allow.ids[i] = 1000 + i;
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(1, c.bleSeen);
  TEST_ASSERT_EQUAL_UINT16(0, c.allowlisted);
}

void test_class_bucketing_and_weights() {
  Fixture f;
  f.p = censusParams(300, -85, 5400, distinctWeights());
  const uint8_t marks[CLASS_COUNT] = {marker::PHONE, marker::WEARABLE, marker::AUDIO, marker::TRACKER,
                                      marker::COMPUTER, marker::BEACON, 0};
  for (uint8_t i = 0; i < CLASS_COUNT; ++i)
    f.t.observe(bleObs(10 + i, -50, AddrKind::ROTATING, marks[i]), kNow);
  f.t.observe(bleObs(20, -50, AddrKind::ROTATING, marker::PHONE), kNow);  // second phone
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  for (uint8_t i = 0; i < CLASS_COUNT; ++i)
    TEST_ASSERT_EQUAL_UINT16(i == 0 ? 2 : 1, c.ble[i]);
  // 2*1 + 0.5 + 0.25 + 0 + 0.75 + 0 + 0.125
  TEST_ASSERT_EQUAL_FLOAT(3.625f, c.bleWeighted);
  TEST_ASSERT_EQUAL_FLOAT(3.625f, c.bleWeightedRotating);
}

void test_marker_priority_via_class() {
  Fixture f;
  f.p = censusParams(300, -85, 5400, distinctWeights());
  f.t.observe(bleObs(1, -50, AddrKind::ROTATING, marker::PHONE | marker::WEARABLE), kNow);
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(1, c.ble[static_cast<uint8_t>(DeviceClass::WEARABLE)]);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, weightOf(f.p.weights, DeviceClass::WEARABLE));
  TEST_ASSERT_EQUAL_FLOAT(0.5f, c.bleWeighted);
}

void test_rotating_subsums() {
  Fixture f;
  f.t.observe(bleObs(1, -50, AddrKind::ROTATING, marker::PHONE), kNow);
  f.t.observe(bleObs(2, -50, AddrKind::FIXED, marker::PHONE), kNow);  // fixed phone: not background
  f.t.observe(wifiObs(3, -50, AddrKind::ROTATING), kNow);
  f.t.observe(wifiObs(4, -50, AddrKind::FIXED), kNow);
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, c.bleWeighted);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, c.bleWeightedRotating);
  TEST_ASSERT_EQUAL_UINT16(2, c.wifi);
  TEST_ASSERT_EQUAL_UINT16(1, c.wifiRotating);
}

void test_background_rule() {
  Fixture f;
  const uint32_t bg = f.p.backgroundAfterS;
  const uint32_t t0 = kNow - bg;  // present exactly backgroundAfterS
  auto seenSince = [&](uint64_t id, uint32_t first, Signal s, AddrKind k, uint8_t mk) {
    f.t.observe(makeObs(id, -50, s, k, mk), first);
    f.t.observe(makeObs(id, -50, s, k, mk), kNow);
  };
  seenSince(1, t0, Signal::BLE, AddrKind::FIXED, marker::BEACON);      // background
  seenSince(2, t0 + 1, Signal::BLE, AddrKind::FIXED, marker::BEACON);  // one second short: counted
  seenSince(3, t0, Signal::BLE, AddrKind::ROTATING, marker::BEACON);   // rotating: counted
  seenSince(4, t0, Signal::BLE, AddrKind::FIXED, marker::PHONE);       // phone: counted
  seenSince(5, t0, Signal::WIFI, AddrKind::FIXED, 0);                  // Wi-Fi: counted
  seenSince(6, t0 - 100, Signal::BLE, AddrKind::FIXED, 0);             // unmarked OTHER: background
  seenSince(7, t0, Signal::BLE, AddrKind::FIXED, marker::PHONE | marker::TRACKER);  // not a phone class
  RawCounts c = takeCensus(f.t, f.allow, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(3, c.background);
  TEST_ASSERT_EQUAL_UINT16(1, c.wifi);
  TEST_ASSERT_EQUAL_UINT16(2, c.ble[static_cast<uint8_t>(DeviceClass::BEACON)]);  // ids 2 and 3
  TEST_ASSERT_EQUAL_UINT16(1, c.ble[static_cast<uint8_t>(DeviceClass::PHONE)]);
  TEST_ASSERT_EQUAL_UINT16(0, c.ble[static_cast<uint8_t>(DeviceClass::OTHER)]);
}

void test_counters_saturate() {
  static std::vector<DeviceEntry> big(131072);
  DeviceTable t(big.data(), 131072, 70000);
  for (uint64_t i = 1; i <= 66000; ++i) t.observe(wifiObs(i, -50), kNow);
  RawCounts c = takeCensus(t, emptyAllowlist(), censusParams(), kNow);
  TEST_ASSERT_EQUAL_UINT16(65535, c.wifi);
  TEST_ASSERT_EQUAL_UINT16(65535, c.wifiSeen);
  TEST_ASSERT_EQUAL_UINT16(65535, c.wifiRotating);
}

void test_capture_contents() {
  Fixture f;
  f.p = censusParams(300, -85, 5400, distinctWeights());
  f.t.observe(bleObs(1, -50, AddrKind::FIXED, marker::WEARABLE), kNow);
  f.t.observe(wifiObs(2, -50, AddrKind::FIXED), kNow);
  f.t.observe(bleObs(3, -50, AddrKind::ROTATING, marker::PHONE), kNow);     // 1
  f.t.observe(bleObs(4, -50, AddrKind::ROTATING, marker::COMPUTER), kNow);  // 0.75
  f.t.observe(wifiObs(5, -50, AddrKind::ROTATING), kNow);
  f.t.observe(wifiObs(6, -50, AddrKind::ROTATING), kNow);
  f.t.observe(bleObs(7, -50, AddrKind::FIXED, 0), kNow - 301);    // out of window
  f.t.observe(bleObs(8, -86, AddrKind::FIXED, 0), kNow);          // below gate
  f.t.observe(bleObs(9, -86, AddrKind::ROTATING, marker::PHONE), kNow);
  f.t.observe(bleObs(10, -50, AddrKind::FIXED, marker::BEACON), 0);  // old fixed: still captured
  f.t.observe(bleObs(10, -50, AddrKind::FIXED, marker::BEACON), kNow);
  Allowlist a;
  Baseline b;
  memset(&a, 0xAB, sizeof a);
  memset(&b, 0xAB, sizeof b);
  captureSetup(f.t, f.p, kNow, a, b);
  std::set<uint64_t> ids(a.ids, a.ids + a.count);
  TEST_ASSERT_EQUAL_UINT16(3, a.count);
  TEST_ASSERT_TRUE(ids == (std::set<uint64_t>{1, 2, 10}));
  TEST_ASSERT_EQUAL_FLOAT(1.75f, b.ble);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, b.wifi);
  TEST_ASSERT_EQUAL_UINT16(3, b.fixedCount);
  TEST_ASSERT_TRUE(b.valid);
}

void test_capture_empty_table_is_valid_and_zeroed() {
  Fixture f;
  Allowlist a;
  Baseline b;
  memset(&a, 0xAB, sizeof a);
  memset(&b, 0xAB, sizeof b);
  captureSetup(f.t, f.p, kNow, a, b);
  TEST_ASSERT_EQUAL_UINT16(0, a.count);
  TEST_ASSERT_EQUAL_UINT64(0, a.ids[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, b.ble);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, b.wifi);
  TEST_ASSERT_EQUAL_UINT16(0, b.fixedCount);
  TEST_ASSERT_TRUE(b.valid);
}

void test_capture_cap_at_allowlist_max() {
  static std::vector<DeviceEntry> big(512);
  DeviceTable t(big.data(), 512, 300);
  std::set<uint64_t> fixedIds;
  for (uint64_t i = 1; i <= 150; ++i) {
    t.observe(wifiObs(i, -50, AddrKind::FIXED), kNow);
    fixedIds.insert(i);
  }
  Allowlist a;
  Baseline b;
  captureSetup(t, censusParams(), kNow, a, b);
  TEST_ASSERT_EQUAL_UINT16(ALLOWLIST_MAX, a.count);
  TEST_ASSERT_EQUAL_UINT16(ALLOWLIST_MAX, b.fixedCount);
  std::set<uint64_t> stored(a.ids, a.ids + a.count);
  TEST_ASSERT_EQUAL_UINT32(ALLOWLIST_MAX, stored.size());  // no duplicates
  for (uint64_t id : stored) TEST_ASSERT_EQUAL_UINT32(1, fixedIds.count(id));
}

void test_capture_then_census_round_trip() {
  Fixture f;
  f.p = censusParams(300, -85, 5400, distinctWeights());
  f.t.observe(bleObs(1, -50, AddrKind::FIXED, marker::BEACON), 0);
  f.t.observe(bleObs(1, -50, AddrKind::FIXED, marker::BEACON), kNow);
  f.t.observe(wifiObs(2, -50, AddrKind::FIXED), kNow);
  f.t.observe(bleObs(3, -50, AddrKind::ROTATING, marker::PHONE), kNow);
  f.t.observe(bleObs(4, -60, AddrKind::ROTATING, marker::WEARABLE), kNow);
  f.t.observe(wifiObs(5, -50, AddrKind::ROTATING), kNow);
  Allowlist a;
  Baseline b;
  captureSetup(f.t, f.p, kNow, a, b);
  RawCounts c = takeCensus(f.t, a, f.p, kNow);
  TEST_ASSERT_EQUAL_UINT16(b.fixedCount, c.allowlisted);
  TEST_ASSERT_EQUAL_FLOAT(b.ble, c.bleWeightedRotating);
  TEST_ASSERT_EQUAL_FLOAT(b.wifi, static_cast<float>(c.wifiRotating));
  TEST_ASSERT_EQUAL_UINT16(0, c.wifi - c.wifiRotating);
  TEST_ASSERT_EQUAL_UINT16(0, c.background);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_table_is_all_zero);
  RUN_TEST(test_window_boundary);
  RUN_TEST(test_seen_counts_by_signal);
  RUN_TEST(test_below_gate);
  RUN_TEST(test_filter_order_gate_before_allowlist);
  RUN_TEST(test_filter_order_allowlist_before_background);
  RUN_TEST(test_allowlist_effect);
  RUN_TEST(test_allowlist_count_clamped);
  RUN_TEST(test_class_bucketing_and_weights);
  RUN_TEST(test_marker_priority_via_class);
  RUN_TEST(test_rotating_subsums);
  RUN_TEST(test_background_rule);
  RUN_TEST(test_counters_saturate);
  RUN_TEST(test_capture_contents);
  RUN_TEST(test_capture_empty_table_is_valid_and_zeroed);
  RUN_TEST(test_capture_cap_at_allowlist_max);
  RUN_TEST(test_capture_then_census_round_trip);
  return UNITY_END();
}
