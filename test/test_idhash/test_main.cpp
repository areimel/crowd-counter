#include <unity.h>
#include <cstring>
#include "IdHash.h"
#include "Sha256.h"

using cc::Signal;

void setUp() {}
void tearDown() {}

static const uint8_t SALT[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const uint8_t MAC[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x42};

void test_matches_manual_sha256() {
  uint8_t msg[23];
  memcpy(msg, SALT, 16);
  msg[16] = 1;  // WIFI
  memcpy(msg + 17, MAC, 6);
  uint8_t d[32];
  cc::sha256(msg, sizeof msg, d);
  uint64_t want = 0;
  for (int i = 0; i < 8; i++) want = (want << 8) | d[i];
  TEST_ASSERT_TRUE(want != 0);
  TEST_ASSERT_TRUE(want == cc::hashId(SALT, Signal::WIFI, MAC));
}

void test_deterministic() {
  TEST_ASSERT_TRUE(cc::hashId(SALT, Signal::BLE, MAC) == cc::hashId(SALT, Signal::BLE, MAC));
}

void test_differs_by_signal() {
  TEST_ASSERT_TRUE(cc::hashId(SALT, Signal::BLE, MAC) != cc::hashId(SALT, Signal::WIFI, MAC));
}

void test_differs_by_salt() {
  uint8_t salt2[16];
  memcpy(salt2, SALT, 16);
  salt2[15] ^= 1;
  TEST_ASSERT_TRUE(cc::hashId(SALT, Signal::BLE, MAC) != cc::hashId(salt2, Signal::BLE, MAC));
}

void test_differs_by_mac() {
  uint8_t mac2[6];
  memcpy(mac2, MAC, 6);
  mac2[0] ^= 1;
  TEST_ASSERT_TRUE(cc::hashId(SALT, Signal::BLE, MAC) != cc::hashId(SALT, Signal::BLE, mac2));
}

void test_non_zero_id_maps_zero_to_one() {
  TEST_ASSERT_TRUE(cc::nonZeroId(0) == 1);
  TEST_ASSERT_TRUE(cc::nonZeroId(1) == 1);
  TEST_ASSERT_TRUE(cc::nonZeroId(0xFFFFFFFFFFFFFFFFull) == 0xFFFFFFFFFFFFFFFFull);
  TEST_ASSERT_TRUE(cc::nonZeroId(0x0123456789abcdefull) == 0x0123456789abcdefull);
}

void test_digestToId_big_endian() {
  uint8_t d[32] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xff};
  TEST_ASSERT_TRUE(cc::digestToId(d) == 0x0102030405060708ull);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_matches_manual_sha256);
  RUN_TEST(test_deterministic);
  RUN_TEST(test_differs_by_signal);
  RUN_TEST(test_differs_by_salt);
  RUN_TEST(test_differs_by_mac);
  RUN_TEST(test_non_zero_id_maps_zero_to_one);
  RUN_TEST(test_digestToId_big_endian);
  return UNITY_END();
}
