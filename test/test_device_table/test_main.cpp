#include <unity.h>

#include <map>
#include <set>
#include <vector>

#include "../support/CounterTestSupport.h"
#include "DeviceTable.h"

using namespace cc;
using namespace cctest;

void setUp() {}
void tearDown() {}

namespace {
DeviceEntry g_slots[16];

const DeviceEntry* lookup(const DeviceTable& t, uint64_t id) {
  const DeviceEntry* found = nullptr;
  uint32_t seen = 0;
  t.forEach([&](const DeviceEntry& e) {
    if (e.id == id) {
      found = &e;
      ++seen;
    }
  });
  TEST_ASSERT_TRUE(seen <= 1);
  return found;
}

// n distinct ids whose home slot in a table of `slots` slots is `home`.
std::vector<uint64_t> collidingIds(uint32_t slots, uint32_t home, size_t n) {
  std::vector<uint64_t> ids;
  for (uint64_t id = 1; ids.size() < n; ++id)
    if ((static_cast<uint32_t>(mixId(id)) & (slots - 1)) == home) ids.push_back(id);
  return ids;
}
}  // namespace

void test_insert_new_entry() {
  DeviceTable t(g_slots, 16, 8);
  t.observe(makeObs(7, -60, Signal::WIFI, AddrKind::FIXED), 100);
  TEST_ASSERT_EQUAL_UINT32(1, t.size());
  const DeviceEntry* e = lookup(t, 7);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_UINT32(100, e->firstSeenS);
  TEST_ASSERT_EQUAL_UINT32(100, e->lastSeenS);
  TEST_ASSERT_EQUAL_INT8(-60, e->rssi);
  TEST_ASSERT_TRUE(e->signal == Signal::WIFI);
  TEST_ASSERT_TRUE(e->addrKind == AddrKind::FIXED);
}

void test_refresh_semantics() {
  DeviceTable t(g_slots, 16, 8);
  t.observe(bleObs(7, -60, AddrKind::ROTATING, marker::PHONE), 100);
  t.observe(makeObs(7, -50, Signal::WIFI, AddrKind::FIXED, 0), 160);
  TEST_ASSERT_EQUAL_UINT32(1, t.size());
  const DeviceEntry* e = lookup(t, 7);
  TEST_ASSERT_EQUAL_UINT32(100, e->firstSeenS);
  TEST_ASSERT_EQUAL_UINT32(160, e->lastSeenS);
  TEST_ASSERT_TRUE(e->signal == Signal::BLE);          // unchanged
  TEST_ASSERT_TRUE(e->addrKind == AddrKind::ROTATING);  // unchanged
  TEST_ASSERT_EQUAL_UINT8(marker::PHONE, e->markers);
  t.observe(bleObs(7, -50, AddrKind::ROTATING, marker::AUDIO), 170);
  TEST_ASSERT_EQUAL_UINT8(marker::PHONE | marker::AUDIO, lookup(t, 7)->markers);
}

void test_rssi_rule() {
  DeviceTable t(g_slots, 16, 8);
  t.observe(makeObs(1, -80), 0);
  t.observe(makeObs(1, -70), 1);  // fast attack: new is stronger
  TEST_ASSERT_EQUAL_INT8(-70, lookup(t, 1)->rssi);
  t.observe(makeObs(1, -90), 2);  // (3*-70 + -90)/4 = -75
  TEST_ASSERT_EQUAL_INT8(-75, lookup(t, 1)->rssi);
  t.observe(makeObs(1, -80), 3);  // (-225 + -80)/4 = -76.25 -> -77 (floors)
  TEST_ASSERT_EQUAL_INT8(-77, lookup(t, 1)->rssi);
  for (int i = 0; i < 20; ++i) t.observe(makeObs(1, -80), 4);  // decay reaches the real level
  TEST_ASSERT_EQUAL_INT8(-80, lookup(t, 1)->rssi);
  t.observe(makeObs(2, -100), 0);
  t.observe(makeObs(2, -99), 0);  // new stronger than smoothed
  TEST_ASSERT_EQUAL_INT8(-99, lookup(t, 2)->rssi);
  t.observe(makeObs(3, 10), 0);
  t.observe(makeObs(3, -127), 0);  // (30 - 127)/4 = -24.25 -> -25
  TEST_ASSERT_EQUAL_INT8(-25, lookup(t, 3)->rssi);
}

void test_id_zero_ignored() {
  DeviceTable t(g_slots, 16, 8);
  t.observe(makeObs(0, -50), 1);
  TEST_ASSERT_EQUAL_UINT32(0, t.size());
}

void test_clear() {
  DeviceTable t(g_slots, 16, 4);
  for (uint64_t i = 1; i <= 9; ++i) t.observe(makeObs(i, -50), 1);
  TEST_ASSERT_TRUE(t.evictions() > 0);
  t.clear();
  TEST_ASSERT_EQUAL_UINT32(0, t.size());
  TEST_ASSERT_EQUAL_UINT32(0, t.evictions());
  int n = 0;
  t.forEach([&](const DeviceEntry&) { ++n; });
  TEST_ASSERT_EQUAL_INT(0, n);
  t.observe(makeObs(3, -50), 5);
  TEST_ASSERT_EQUAL_UINT32(1, t.size());
}

void test_degenerate_tables_are_safe() {
  DeviceTable n(nullptr, 16, 4);
  n.observe(makeObs(1, -50), 1);
  TEST_ASSERT_EQUAL_UINT32(0, n.size());
  TEST_ASSERT_EQUAL_UINT32(0, n.expire(100, 1));
  n.clear();
  int visits = 0;
  n.forEach([&](const DeviceEntry&) { ++visits; });
  TEST_ASSERT_EQUAL_INT(0, visits);
  DeviceTable z(g_slots, 0, 4);
  z.observe(makeObs(1, -50), 1);
  TEST_ASSERT_EQUAL_UINT32(0, z.size());
  TEST_ASSERT_EQUAL_UINT32(0, z.expire(100, 1));
  DeviceTable one(g_slots, 1, 4);  // must not spin forever
  for (uint64_t i = 1; i <= 3; ++i) one.observe(makeObs(i, -50), 1);
  TEST_ASSERT_EQUAL_UINT32(0, one.size());
  TEST_ASSERT_EQUAL_UINT32(0, one.expire(100, 1));
  one.forEach([&](const DeviceEntry&) { ++visits; });
  TEST_ASSERT_EQUAL_INT(0, visits);
}

void test_clear_zeroes_whole_slots() {
  DeviceTable t(g_slots, 16, 8);
  for (uint64_t i = 1; i <= 6; ++i) t.observe(makeObs(i, -50), 7);
  t.clear();
  for (const DeviceEntry& e : g_slots) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(&e);
    for (size_t k = 0; k < sizeof e; ++k) TEST_ASSERT_EQUAL_UINT8(0, b[k]);
  }
}

void test_constructor_clamps() {
  DeviceTable a(g_slots, 16, 16);  // clamped to 14
  for (uint64_t i = 1; i <= 40; ++i) a.observe(makeObs(i, -50), static_cast<uint32_t>(i));
  TEST_ASSERT_TRUE(a.size() <= 14);
  DeviceTable b(g_slots, 16, 0);  // 0 -> 1
  b.observe(makeObs(1, -50), 1);
  b.observe(makeObs(2, -50), 2);
  TEST_ASSERT_EQUAL_UINT32(1, b.size());
  TEST_ASSERT_NULL(lookup(b, 1));
  TEST_ASSERT_NOT_NULL(lookup(b, 2));
}

void test_expire_boundary() {
  DeviceTable t(g_slots, 16, 8);
  t.observe(makeObs(1, -50), 100);  // age 50 at 150
  t.observe(makeObs(2, -50), 99);   // age 51
  t.observe(makeObs(3, -50), 150);
  TEST_ASSERT_EQUAL_UINT32(1, t.expire(150, 50));
  TEST_ASSERT_EQUAL_UINT32(2, t.size());
  TEST_ASSERT_NOT_NULL(lookup(t, 1));
  TEST_ASSERT_NULL(lookup(t, 2));
  TEST_ASSERT_NOT_NULL(lookup(t, 3));
  TEST_ASSERT_EQUAL_UINT32(0, t.expire(150, 50));
}

void test_deletion_keeps_colliding_entries_findable() {
  // All ids share a home slot; deleting any member of the chain must keep the rest.
  for (size_t victim = 0; victim < 5; ++victim) {
    DeviceTable t(g_slots, 16, 10);
    std::vector<uint64_t> ids = collidingIds(16, 3, 5);
    for (size_t i = 0; i < ids.size(); ++i) t.observe(makeObs(ids[i], -50), static_cast<uint32_t>(10 + i));
    // Age out exactly one: refresh the others.
    for (size_t i = 0; i < ids.size(); ++i)
      if (i != victim) t.observe(makeObs(ids[i], -50), 100);
    TEST_ASSERT_EQUAL_UINT32(1, t.expire(100, 50));
    TEST_ASSERT_EQUAL_UINT32(4, t.size());
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i == victim) {
        TEST_ASSERT_NULL(lookup(t, ids[i]));
      } else {
        t.observe(makeObs(ids[i], -50), 101);  // must refresh, not duplicate
        TEST_ASSERT_NOT_NULL(lookup(t, ids[i]));
      }
    }
    TEST_ASSERT_EQUAL_UINT32(4, t.size());
  }
}

void test_wraparound_cluster_deletion() {
  // Cluster wrapping past the last slot.
  std::vector<uint64_t> ids = collidingIds(16, 14, 5);
  DeviceTable t(g_slots, 16, 10);
  for (size_t i = 0; i < ids.size(); ++i) t.observe(makeObs(ids[i], -50), static_cast<uint32_t>(i));
  TEST_ASSERT_EQUAL_UINT32(5, t.size());
  TEST_ASSERT_EQUAL_UINT32(2, t.expire(10, 8));  // removes lastSeen 0 and 1
  TEST_ASSERT_EQUAL_UINT32(3, t.size());
  for (size_t i = 2; i < ids.size(); ++i) {
    t.observe(makeObs(ids[i], -50), 10);
    TEST_ASSERT_NOT_NULL(lookup(t, ids[i]));
  }
  TEST_ASSERT_EQUAL_UINT32(3, t.size());
}

void test_eviction_removes_oldest_batch() {
  DeviceEntry slots[64];
  DeviceTable t(slots, 64, 32);  // batch = 2
  for (uint64_t i = 1; i <= 32; ++i) t.observe(makeObs(i, -50), static_cast<uint32_t>(i));
  TEST_ASSERT_EQUAL_UINT32(0, t.evictions());
  t.observe(makeObs(100, -50), 40);
  TEST_ASSERT_EQUAL_UINT32(2, t.evictions());
  TEST_ASSERT_EQUAL_UINT32(31, t.size());
  TEST_ASSERT_NULL(lookup(t, 1));
  TEST_ASSERT_NULL(lookup(t, 2));
  for (uint64_t i = 3; i <= 32; ++i) TEST_ASSERT_NOT_NULL(lookup(t, i));
  TEST_ASSERT_NOT_NULL(lookup(t, 100));
}

void test_eviction_with_equal_times_removes_exactly_batch() {
  DeviceEntry slots[128];
  DeviceTable t(slots, 128, 64);  // batch = 4
  for (uint64_t i = 1; i <= 64; ++i) t.observe(makeObs(i, -50), 500);
  t.observe(makeObs(1000, -50), 500);
  TEST_ASSERT_EQUAL_UINT32(4, t.evictions());
  TEST_ASSERT_EQUAL_UINT32(61, t.size());
  TEST_ASSERT_NOT_NULL(lookup(t, 1000));
}

void test_eviction_batch_at_least_one() {
  DeviceTable t(g_slots, 16, 4);  // 4/16 = 0 -> batch 1
  for (uint64_t i = 1; i <= 4; ++i) t.observe(makeObs(i, -50), static_cast<uint32_t>(i));
  t.observe(makeObs(9, -50), 9);
  TEST_ASSERT_EQUAL_UINT32(1, t.evictions());
  TEST_ASSERT_EQUAL_UINT32(4, t.size());
  TEST_ASSERT_NULL(lookup(t, 1));
  TEST_ASSERT_NOT_NULL(lookup(t, 2));
}

void test_eviction_wide_time_span() {
  // Span far wider than 64 s: removes the batch, never more, and the very
  // oldest entry goes first.
  DeviceEntry slots[64];
  DeviceTable t(slots, 64, 32);
  for (uint64_t i = 1; i <= 32; ++i) t.observe(makeObs(i, -50), static_cast<uint32_t>(i * 100000));
  t.observe(makeObs(99, -50), 4000000);
  TEST_ASSERT_EQUAL_UINT32(2, t.evictions());
  TEST_ASSERT_EQUAL_UINT32(31, t.size());
  TEST_ASSERT_NULL(lookup(t, 1));
}

void test_device_size_fill() {
  static DeviceEntry slots[8192];
  DeviceTable t(slots, 8192, 5000);
  uint32_t size = 0, ev = 0;
  for (uint64_t i = 1; i <= 20000; ++i) {
    if (size == 5000) {
      size -= 312;
      ev += 312;
    }
    ++size;
    t.observe(makeObs(i * 0x9E3779B97F4A7C15ULL | 1, -60), static_cast<uint32_t>(i / 4));
    TEST_ASSERT_TRUE(t.size() <= 5000);
  }
  TEST_ASSERT_EQUAL_UINT32(size, t.size());
  TEST_ASSERT_EQUAL_UINT32(ev, t.evictions());
  for (uint64_t i = 19900; i <= 20000; ++i) TEST_ASSERT_NOT_NULL(lookup(t, i * 0x9E3779B97F4A7C15ULL | 1));
}

void test_for_each_visits_each_once() {
  DeviceTable t(g_slots, 16, 10);
  for (uint64_t i = 1; i <= 8; ++i) t.observe(makeObs(i, -50), 1);
  std::multiset<uint64_t> seen;
  t.forEach([&](const DeviceEntry& e) { seen.insert(e.id); });
  TEST_ASSERT_EQUAL_UINT32(8, seen.size());
  for (uint64_t i = 1; i <= 8; ++i) TEST_ASSERT_EQUAL_UINT32(1, seen.count(i));
}

// Small deterministic PRNG.
struct Rng {
  uint64_t s;
  uint32_t next() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<uint32_t>(s >> 33);
  }
};

void test_randomized_differential() {
  DeviceEntry slots[64];
  DeviceTable t(slots, 64, 40);  // batch = 2
  struct Ref { uint32_t first, last; int8_t rssi; uint8_t markers; };
  std::map<uint64_t, Ref> ref;
  Rng rng{12345};
  uint32_t now = 0, evictionsRef = 0;
  for (int op = 0; op < 6000; ++op) {
    if (op % 60 == 59) ++now;  // stays under 64 s so histogram buckets are exact
    if (rng.next() % 40 == 0) {
      const uint32_t age = rng.next() % 6;
      uint32_t expected = 0;
      for (auto it = ref.begin(); it != ref.end();) {
        if (now - it->second.last > age) {
          it = ref.erase(it);
          ++expected;
        } else {
          ++it;
        }
      }
      TEST_ASSERT_EQUAL_UINT32(expected, t.expire(now, age));
    } else {
      const uint64_t id = 1 + rng.next() % 120;
      const int8_t rssi = static_cast<int8_t>(-100 + static_cast<int>(rng.next() % 70));
      const uint8_t mk = static_cast<uint8_t>(1u << (rng.next() % 6));
      auto it = ref.find(id);
      if (it != ref.end()) {
        const int sum = 3 * it->second.rssi + rssi;
        const int avg = (sum - ((sum % 4 + 4) % 4)) / 4;  // floor, written independently of the code under test
        it->second.rssi = static_cast<int8_t>(rssi > avg ? rssi : avg);
        it->second.last = now;
        it->second.markers |= mk;
        t.observe(makeObs(id, rssi, Signal::BLE, AddrKind::ROTATING, mk), now);
      } else {
        const bool full = ref.size() == 40;
        uint32_t minSurvivor = 0xFFFFFFFFu;
        t.observe(makeObs(id, rssi, Signal::BLE, AddrKind::ROTATING, mk), now);
        std::set<uint64_t> live;
        t.forEach([&](const DeviceEntry& e) { live.insert(e.id); });
        if (full) {
          std::vector<uint64_t> gone;
          for (auto& kv : ref) {
            if (live.count(kv.first)) minSurvivor = kv.second.last < minSurvivor ? kv.second.last : minSurvivor;
            else gone.push_back(kv.first);
          }
          TEST_ASSERT_EQUAL_UINT32(2, gone.size());
          for (uint64_t g : gone) {
            TEST_ASSERT_TRUE(ref[g].last <= minSurvivor);  // evicted were among the oldest
            ref.erase(g);
          }
          evictionsRef += 2;
        }
        ref[id] = Ref{now, now, rssi, mk};
      }
    }
    TEST_ASSERT_EQUAL_UINT32(ref.size(), t.size());
    if (op % 50 == 0) {
      std::map<uint64_t, DeviceEntry> got;
      t.forEach([&](const DeviceEntry& e) { got[e.id] = e; });
      TEST_ASSERT_EQUAL_UINT32(ref.size(), got.size());
      for (auto& kv : ref) {
        auto g = got.find(kv.first);
        TEST_ASSERT_TRUE(g != got.end());
        TEST_ASSERT_EQUAL_UINT32(kv.second.last, g->second.lastSeenS);
        TEST_ASSERT_EQUAL_INT8(kv.second.rssi, g->second.rssi);
        TEST_ASSERT_EQUAL_UINT8(kv.second.markers, g->second.markers);
      }
    }
  }
  TEST_ASSERT_EQUAL_UINT32(evictionsRef, t.evictions());
  TEST_ASSERT_TRUE(evictionsRef > 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_insert_new_entry);
  RUN_TEST(test_refresh_semantics);
  RUN_TEST(test_rssi_rule);
  RUN_TEST(test_id_zero_ignored);
  RUN_TEST(test_clear);
  RUN_TEST(test_degenerate_tables_are_safe);
  RUN_TEST(test_clear_zeroes_whole_slots);
  RUN_TEST(test_constructor_clamps);
  RUN_TEST(test_expire_boundary);
  RUN_TEST(test_deletion_keeps_colliding_entries_findable);
  RUN_TEST(test_wraparound_cluster_deletion);
  RUN_TEST(test_eviction_removes_oldest_batch);
  RUN_TEST(test_eviction_with_equal_times_removes_exactly_batch);
  RUN_TEST(test_eviction_batch_at_least_one);
  RUN_TEST(test_eviction_wide_time_span);
  RUN_TEST(test_device_size_fill);
  RUN_TEST(test_for_each_visits_each_once);
  RUN_TEST(test_randomized_differential);
  return UNITY_END();
}
