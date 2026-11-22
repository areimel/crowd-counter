#include <unity.h>
#include <cstdio>
#include <cstring>
#include "Sha256.h"

using cc::Sha256;
using cc::sha256;

void setUp() {}
void tearDown() {}

static void hex(const uint8_t d[32], char out[65]) {
  for (int i = 0; i < 32; i++) snprintf(out + i * 2, 3, "%02x", d[i]);
}

static void expectDigest(const char* msg, const char* want) {
  uint8_t d[32];
  char h[65];
  sha256(reinterpret_cast<const uint8_t*>(msg), strlen(msg), d);
  hex(d, h);
  TEST_ASSERT_EQUAL_STRING(want, h);
}

void test_empty() {
  expectDigest("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}
void test_abc() {
  expectDigest("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
void test_448bit() {
  expectDigest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
               "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}
void test_896bit() {
  expectDigest(
      "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

void test_million_a_chunked() {
  uint8_t buf[1000];
  memset(buf, 'a', sizeof buf);
  Sha256 s;
  for (int i = 0; i < 1000; i++) s.update(buf, sizeof buf);
  uint8_t d[32];
  char h[65];
  s.finish(d);
  hex(d, h);
  TEST_ASSERT_EQUAL_STRING("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", h);
}

void test_chunked_equals_oneshot_all_splits() {
  uint8_t msg[150];
  for (size_t i = 0; i < sizeof msg; i++) msg[i] = static_cast<uint8_t>(i * 7 + 3);
  uint8_t want[32], got[32];
  sha256(msg, sizeof msg, want);
  for (size_t split = 0; split <= sizeof msg; split++) {
    Sha256 s;
    s.update(msg, split);
    s.update(msg + split, sizeof msg - split);
    s.finish(got);
    TEST_ASSERT_EQUAL_MEMORY(want, got, 32);
  }
}

void test_padding_edges_byte_at_a_time() {
  const size_t lens[] = {0, 1, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128};
  for (size_t len : lens) {
    uint8_t msg[128];
    for (size_t i = 0; i < len; i++) msg[i] = static_cast<uint8_t>(i ^ 0x5a);
    uint8_t want[32], got[32];
    sha256(msg, len, want);
    Sha256 s;
    for (size_t i = 0; i < len; i++) s.update(msg + i, 1);
    s.finish(got);
    TEST_ASSERT_EQUAL_MEMORY(want, got, 32);
  }
}

void test_reset_reuses_object() {
  Sha256 s;
  s.update(reinterpret_cast<const uint8_t*>("junk"), 4);
  s.reset();
  s.update(reinterpret_cast<const uint8_t*>("abc"), 3);
  uint8_t d[32], want[32];
  s.finish(d);
  sha256(reinterpret_cast<const uint8_t*>("abc"), 3, want);
  TEST_ASSERT_EQUAL_MEMORY(want, d, 32);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty);
  RUN_TEST(test_abc);
  RUN_TEST(test_448bit);
  RUN_TEST(test_896bit);
  RUN_TEST(test_million_a_chunked);
  RUN_TEST(test_chunked_equals_oneshot_all_splits);
  RUN_TEST(test_padding_edges_byte_at_a_time);
  RUN_TEST(test_reset_reuses_object);
  return UNITY_END();
}
