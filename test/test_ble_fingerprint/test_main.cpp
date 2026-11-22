#include <string.h>
#include <unity.h>

#include "BleFingerprint.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static const uint8_t FLAGS[] = {0x02, 0x01, 0x06};

// Builds: flags AD + manufacturer AD. Returns total length.
static size_t withFlags(uint8_t* out, const uint8_t* ad, size_t adLen) {
  memcpy(out, FLAGS, sizeof FLAGS);
  memcpy(out + sizeof FLAGS, ad, adLen);
  return sizeof FLAGS + adLen;
}

static uint8_t run(const uint8_t* ad, size_t adLen) {
  uint8_t buf[96];
  size_t n = withFlags(buf, ad, adLen);
  return bleMarkers(buf, n);
}

// Manufacturer AD for Apple with the given continuity bytes.
static size_t appleAd(uint8_t* out, const uint8_t* cont, size_t n) {
  out[0] = (uint8_t)(3 + n);
  out[1] = 0xFF;
  out[2] = 0x4C;
  out[3] = 0x00;
  memcpy(out + 4, cont, n);
  return 4 + n;
}

static uint8_t apple(const uint8_t* cont, size_t n) {
  uint8_t ad[64];
  size_t l = appleAd(ad, cont, n);
  return run(ad, l);
}

void test_apple_nearby_info() {
  const uint8_t c[] = {0x10, 0x05, 0x01, 0x18, 0x2A, 0x3B, 0x4C};
  TEST_ASSERT_EQUAL_UINT8(marker::PHONE, apple(c, sizeof c));
}
void test_apple_handoff() {
  const uint8_t c[] = {0x0C, 0x02, 0xAA, 0xBB};
  TEST_ASSERT_EQUAL_UINT8(marker::PHONE, apple(c, sizeof c));
}
void test_apple_nearby_action() {
  const uint8_t c[] = {0x0F, 0x03, 0x00, 0x01, 0x02};
  TEST_ASSERT_EQUAL_UINT8(marker::PHONE, apple(c, sizeof c));
}
void test_apple_airdrop() {
  const uint8_t c[] = {0x05, 0x02, 0x00, 0x00};
  TEST_ASSERT_EQUAL_UINT8(marker::PHONE, apple(c, sizeof c));
}
void test_apple_proximity_pairing() {
  const uint8_t c[] = {0x07, 0x03, 0x01, 0x02, 0x03};
  TEST_ASSERT_EQUAL_UINT8(marker::AUDIO, apple(c, sizeof c));
}
void test_apple_find_my() {
  const uint8_t c[] = {0x12, 0x02, 0x00, 0x01};
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER, apple(c, sizeof c));
}
void test_apple_magic_switch() {
  const uint8_t c[] = {0x0B, 0x02, 0x00, 0x01};
  TEST_ASSERT_EQUAL_UINT8(marker::WEARABLE, apple(c, sizeof c));
}
void test_apple_airplay() {
  const uint8_t a[] = {0x09, 0x01, 0x00};
  const uint8_t b[] = {0x0A, 0x01, 0x00};
  TEST_ASSERT_EQUAL_UINT8(marker::BEACON, apple(a, sizeof a));
  TEST_ASSERT_EQUAL_UINT8(marker::BEACON, apple(b, sizeof b));
}
void test_apple_ibeacon_layout() {
  uint8_t c[23];
  c[0] = 0x02;
  c[1] = 0x15;
  memset(c + 2, 0x11, 21);
  TEST_ASSERT_EQUAL_UINT8(marker::BEACON, apple(c, sizeof c));
}
void test_apple_unknown_type() {
  const uint8_t c[] = {0x77, 0x02, 0x00, 0x00};
  TEST_ASSERT_EQUAL_UINT8(0, apple(c, sizeof c));
}
void test_apple_multiple_entries_phone() {
  const uint8_t c[] = {0x10, 0x02, 0x01, 0x02, 0x0C, 0x01, 0x09};
  TEST_ASSERT_EQUAL_UINT8(marker::PHONE, apple(c, sizeof c));
}
void test_apple_multiple_entries_two_bits() {
  const uint8_t c[] = {0x12, 0x01, 0x00, 0x10, 0x01, 0x05};
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER | marker::PHONE, apple(c, sizeof c));
}
void test_apple_unknown_then_known() {
  const uint8_t c[] = {0x77, 0x02, 0x00, 0x00, 0x07, 0x01, 0x00};
  TEST_ASSERT_EQUAL_UINT8(marker::AUDIO, apple(c, sizeof c));
}
void test_apple_entry_len_past_end() {
  // Second entry claims 9 bytes but only 1 remains; first is kept.
  const uint8_t c[] = {0x10, 0x01, 0x00, 0x12, 0x09, 0x00};
  TEST_ASSERT_EQUAL_UINT8(marker::PHONE, apple(c, sizeof c));
  // Sole entry overrunning yields nothing.
  const uint8_t d[] = {0x10, 0x09, 0x00};
  TEST_ASSERT_EQUAL_UINT8(0, apple(d, sizeof d));
}
void test_apple_truncated_entry_header() {
  const uint8_t c[] = {0x10};
  TEST_ASSERT_EQUAL_UINT8(0, apple(c, sizeof c));
}

void test_microsoft_cdp() {
  const uint8_t ad[] = {0x08, 0xFF, 0x06, 0x00, 0x01, 0x09, 0x20, 0x02, 0x00};
  TEST_ASSERT_EQUAL_UINT8(marker::COMPUTER, run(ad, sizeof ad));
}
void test_microsoft_other_first_byte() {
  const uint8_t ad[] = {0x08, 0xFF, 0x06, 0x00, 0x02, 0x09, 0x20, 0x02, 0x00};
  TEST_ASSERT_EQUAL_UINT8(0, run(ad, sizeof ad));
}
void test_microsoft_no_data() {
  const uint8_t ad[] = {0x03, 0xFF, 0x06, 0x00};
  TEST_ASSERT_EQUAL_UINT8(0, run(ad, sizeof ad));
}
void test_samsung_nothing() {
  const uint8_t ad[] = {0x08, 0xFF, 0x75, 0x00, 0x42, 0x04, 0x01, 0x80, 0x5A};
  TEST_ASSERT_EQUAL_UINT8(0, run(ad, sizeof ad));
}
void test_other_company_nothing() {
  const uint8_t ad[] = {0x06, 0xFF, 0x59, 0x00, 0x01, 0x02, 0x03};
  TEST_ASSERT_EQUAL_UINT8(0, run(ad, sizeof ad));
}

void test_uuid_tile() {
  const uint8_t a[] = {0x03, 0x03, 0xED, 0xFE};
  const uint8_t b[] = {0x03, 0x02, 0xEC, 0xFE};
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER, run(a, sizeof a));
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER, run(b, sizeof b));
}
void test_uuid_smarttag() {
  const uint8_t a[] = {0x03, 0x03, 0x5A, 0xFD};
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER, run(a, sizeof a));
}
void test_uuid_eddystone() {
  const uint8_t a[] = {0x03, 0x03, 0xAA, 0xFE};
  TEST_ASSERT_EQUAL_UINT8(marker::BEACON, run(a, sizeof a));
}
void test_uuid_list_several() {
  // 0x1800, 0xFEAA, 0xFEED
  const uint8_t a[] = {0x07, 0x03, 0x00, 0x18, 0xAA, 0xFE, 0xED, 0xFE};
  TEST_ASSERT_EQUAL_UINT8(marker::BEACON | marker::TRACKER, run(a, sizeof a));
}
void test_uuid_unknown() {
  const uint8_t a[] = {0x03, 0x03, 0x0F, 0x18};
  TEST_ASSERT_EQUAL_UINT8(0, run(a, sizeof a));
}
void test_uuid_odd_trailing_byte() {
  const uint8_t a[] = {0x04, 0x03, 0xED, 0xFE, 0xAA};
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER, run(a, sizeof a));
  const uint8_t b[] = {0x03, 0x03, 0xAA};  // lone byte
  TEST_ASSERT_EQUAL_UINT8(0, run(b, sizeof b));
}

void test_service_data_fast_pair() {
  const uint8_t a[] = {0x06, 0x16, 0x2C, 0xFE, 0x01, 0x02, 0x03};
  TEST_ASSERT_EQUAL_UINT8(marker::AUDIO, run(a, sizeof a));
}
void test_service_data_eddystone() {
  const uint8_t a[] = {0x06, 0x16, 0xAA, 0xFE, 0x00, 0xE7, 0x01};
  TEST_ASSERT_EQUAL_UINT8(marker::BEACON, run(a, sizeof a));
}
void test_service_data_tracker() {
  const uint8_t a[] = {0x05, 0x16, 0x5A, 0xFD, 0x01, 0x02};
  const uint8_t b[] = {0x05, 0x16, 0xEC, 0xFE, 0x01, 0x02};
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER, run(a, sizeof a));
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER, run(b, sizeof b));
}
void test_service_data_too_short() {
  const uint8_t a[] = {0x02, 0x16, 0x2C};
  TEST_ASSERT_EQUAL_UINT8(0, run(a, sizeof a));
}

void test_multiple_ads_combine() {
  const uint8_t ad[] = {0x03, 0x03, 0xAA, 0xFE,                        // Eddystone UUID
                        0x06, 0x16, 0x2C, 0xFE, 0x00, 0x00, 0x00,      // Fast Pair
                        0x07, 0xFF, 0x4C, 0x00, 0x10, 0x02, 0x00, 0x00};  // Nearby Info
  TEST_ASSERT_EQUAL_UINT8(marker::BEACON | marker::AUDIO | marker::PHONE, run(ad, sizeof ad));
}

// ---- malformed ----
void test_null_and_empty() {
  TEST_ASSERT_EQUAL_UINT8(0, bleMarkers(nullptr, 0));
  TEST_ASSERT_EQUAL_UINT8(0, bleMarkers(nullptr, 31));
  const uint8_t x[] = {0x03, 0x03, 0xED, 0xFE};
  TEST_ASSERT_EQUAL_UINT8(0, bleMarkers(x, 0));
}
void test_zero_length_ad_stops() {
  const uint8_t buf[] = {0x02, 0x01, 0x06, 0x00, 0x03, 0x03, 0xED, 0xFE};
  TEST_ASSERT_EQUAL_UINT8(0, bleMarkers(buf, sizeof buf));
}
void test_ad_length_past_end() {
  const uint8_t buf[] = {0x03, 0x03, 0xED, 0xFE, 0x20, 0x03, 0xAA, 0xFE};
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER, bleMarkers(buf, sizeof buf));
  const uint8_t only[] = {0x20, 0x03, 0xED, 0xFE};
  TEST_ASSERT_EQUAL_UINT8(0, bleMarkers(only, sizeof only));
}
void test_manufacturer_ad_len_2() {
  const uint8_t ad[] = {0x02, 0xFF, 0x4C};
  TEST_ASSERT_EQUAL_UINT8(0, run(ad, sizeof ad));
  const uint8_t ad1[] = {0x01, 0xFF};
  TEST_ASSERT_EQUAL_UINT8(0, run(ad1, sizeof ad1));
}
void test_len_ignores_bytes_beyond() {
  // Valid tracker UUID AD but len says only the first 3 bytes are readable.
  const uint8_t buf[] = {0x03, 0x03, 0xED, 0xFE};
  TEST_ASSERT_EQUAL_UINT8(0, bleMarkers(buf, 3));
}

void test_truncation_subset() {
  const uint8_t full[] = {0x02, 0x01, 0x06,
                          0x07, 0x03, 0xED, 0xFE, 0xAA, 0xFE, 0x5A, 0xFD,
                          0x0B, 0xFF, 0x4C, 0x00, 0x10, 0x02, 0x01, 0x02, 0x12, 0x02, 0x00, 0x01,
                          0x08, 0xFF, 0x06, 0x00, 0x01, 0x09, 0x20, 0x02, 0x00,
                          0x05, 0x16, 0x2C, 0xFE, 0x01, 0x02};
  uint8_t all = bleMarkers(full, sizeof full);
  TEST_ASSERT_EQUAL_UINT8(marker::TRACKER | marker::BEACON | marker::PHONE | marker::COMPUTER |
                              marker::AUDIO,
                          all);
  for (size_t n = 0; n <= sizeof full; ++n) {
    uint8_t part = bleMarkers(full, n);
    TEST_ASSERT_EQUAL_UINT8(part, part & all);
  }
}

void test_classOf() {
  TEST_ASSERT_EQUAL_UINT8((uint8_t)DeviceClass::OTHER, (uint8_t)classOf(0));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)DeviceClass::PHONE, (uint8_t)classOf(marker::PHONE));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)DeviceClass::BEACON,
                          (uint8_t)classOf(marker::PHONE | marker::BEACON));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)DeviceClass::COMPUTER,
                          (uint8_t)classOf(marker::BEACON | marker::COMPUTER));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)DeviceClass::WEARABLE,
                          (uint8_t)classOf(marker::COMPUTER | marker::WEARABLE));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)DeviceClass::AUDIO,
                          (uint8_t)classOf(marker::WEARABLE | marker::AUDIO));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)DeviceClass::TRACKER,
                          (uint8_t)classOf(0x3F));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_apple_nearby_info);
  RUN_TEST(test_apple_handoff);
  RUN_TEST(test_apple_nearby_action);
  RUN_TEST(test_apple_airdrop);
  RUN_TEST(test_apple_proximity_pairing);
  RUN_TEST(test_apple_find_my);
  RUN_TEST(test_apple_magic_switch);
  RUN_TEST(test_apple_airplay);
  RUN_TEST(test_apple_ibeacon_layout);
  RUN_TEST(test_apple_unknown_type);
  RUN_TEST(test_apple_multiple_entries_phone);
  RUN_TEST(test_apple_multiple_entries_two_bits);
  RUN_TEST(test_apple_unknown_then_known);
  RUN_TEST(test_apple_entry_len_past_end);
  RUN_TEST(test_apple_truncated_entry_header);
  RUN_TEST(test_microsoft_cdp);
  RUN_TEST(test_microsoft_other_first_byte);
  RUN_TEST(test_microsoft_no_data);
  RUN_TEST(test_samsung_nothing);
  RUN_TEST(test_other_company_nothing);
  RUN_TEST(test_uuid_tile);
  RUN_TEST(test_uuid_smarttag);
  RUN_TEST(test_uuid_eddystone);
  RUN_TEST(test_uuid_list_several);
  RUN_TEST(test_uuid_unknown);
  RUN_TEST(test_uuid_odd_trailing_byte);
  RUN_TEST(test_service_data_fast_pair);
  RUN_TEST(test_service_data_eddystone);
  RUN_TEST(test_service_data_tracker);
  RUN_TEST(test_service_data_too_short);
  RUN_TEST(test_multiple_ads_combine);
  RUN_TEST(test_null_and_empty);
  RUN_TEST(test_zero_length_ad_stops);
  RUN_TEST(test_ad_length_past_end);
  RUN_TEST(test_manufacturer_ad_len_2);
  RUN_TEST(test_len_ignores_bytes_beyond);
  RUN_TEST(test_truncation_subset);
  RUN_TEST(test_classOf);
  return UNITY_END();
}
