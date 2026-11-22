#include <string.h>
#include <unity.h>

#include "ProbeParse.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static void makeFrame(uint8_t* f, size_t n, uint8_t fc, const uint8_t src[6]) {
  memset(f, 0, n);
  f[0] = fc;
  memcpy(f + 10, src, 6);
}

static const uint8_t GLOBAL_MAC[6] = {0x3C, 0x22, 0xFB, 0x01, 0x02, 0x03};
static const uint8_t LOCAL_MAC[6] = {0xDA, 0xA1, 0x19, 0x0A, 0x0B, 0x0C};

void test_valid_global() {
  uint8_t f[32];
  makeFrame(f, sizeof f, 0x40, GLOBAL_MAC);
  ProbeInfo p;
  memset(&p, 0, sizeof p);
  TEST_ASSERT_TRUE(parseProbeRequest(f, sizeof f, p));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(GLOBAL_MAC, p.mac, 6);
  TEST_ASSERT_FALSE(p.randomized);
}

void test_valid_local() {
  uint8_t f[24];
  makeFrame(f, sizeof f, 0x40, LOCAL_MAC);
  ProbeInfo p;
  memset(&p, 0, sizeof p);
  TEST_ASSERT_TRUE(parseProbeRequest(f, sizeof f, p));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(LOCAL_MAC, p.mac, 6);
  TEST_ASSERT_TRUE(p.randomized);
}

static void expectRejected(uint8_t fc, size_t len) {
  uint8_t f[32];
  makeFrame(f, sizeof f, fc, LOCAL_MAC);
  ProbeInfo p;
  memset(&p, 0xAB, sizeof p);
  TEST_ASSERT_FALSE(parseProbeRequest(f, len, p));
  for (int i = 0; i < 6; ++i) TEST_ASSERT_EQUAL_UINT8(0xAB, p.mac[i]);
  TEST_ASSERT_EQUAL_UINT8(0xAB, (uint8_t)p.randomized);
}

void test_beacon_rejected() { expectRejected(0x80, 32); }
void test_probe_response_rejected() { expectRejected(0x50, 32); }
void test_data_frame_rejected() { expectRejected(0x08, 32); }
void test_short_rejected() { expectRejected(0x40, 23); }
void test_len_zero_rejected() { expectRejected(0x40, 0); }

void test_null_rejected() {
  ProbeInfo p;
  memset(&p, 0xAB, sizeof p);
  TEST_ASSERT_FALSE(parseProbeRequest(nullptr, 64, p));
  TEST_ASSERT_EQUAL_UINT8(0xAB, p.mac[0]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_global);
  RUN_TEST(test_valid_local);
  RUN_TEST(test_beacon_rejected);
  RUN_TEST(test_probe_response_rejected);
  RUN_TEST(test_data_frame_rejected);
  RUN_TEST(test_short_rejected);
  RUN_TEST(test_len_zero_rejected);
  RUN_TEST(test_null_rejected);
  return UNITY_END();
}
