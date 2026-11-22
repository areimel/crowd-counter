#include <math.h>
#include <string.h>
#include <unity.h>

#include "../support/CounterTestSupport.h"
#include "NightLogFormat.h"

using namespace cc;
using namespace cctest;

void setUp() {}
void tearDown() {}

static const char* COLUMNS =
    "minute,epoch,ble_phone,ble_wearable,ble_audio,ble_tracker,ble_computer,ble_beacon,"
    "ble_other,wifi,wifi_rotating,background,allowlisted,ble_signal,wifi_signal,estimate,"
    "smoothed,peak,threshold,battery_mv,flags\n";

static NightMethod method(const char* venue) {
  NightMethod m;
  memset(&m, 0, sizeof m);
  m.venue = venue;
  m.ratios = ratios(2.5f, 1.25f, true);
  m.baseline = baseline(3.5f, 4.0f, 7);
  m.blend = 0.8f;
  m.windowS = 300;
  m.rssiGate = -85;
  m.weights = distinctWeights();
  return m;
}

static const char* METHOD_FIELDS =
    "venue=\"Bar\" ratio_ble=2.500 ratio_wifi=1.250 wifi_fitted=1 blend=0.80 window_s=300 "
    "rssi_gate=-85 w_phone=1.00 w_wearable=0.50 w_audio=0.25 w_tracker=0.00 "
    "w_computer=0.75 w_beacon=0.00 w_other=0.12 baseline_ble=3.50 baseline_wifi=4.00 "
    "baseline_fixed=7 baseline_valid=1";

static MinuteRecord record(uint16_t minute) {
  MinuteRecord r;
  memset(&r, 0, sizeof r);
  r.minute = minute;
  r.epoch = 1700000000u + minute * 60u;
  for (uint8_t i = 0; i < CLASS_COUNT; ++i) r.ble[i] = static_cast<uint16_t>(i + 1);
  r.wifi = 40;
  r.wifiRotating = 30;
  r.background = 5;
  r.allowlisted = 2;
  r.bleSignal = 12.5f;
  r.wifiSignal = 9.25f;
  r.estimate = 60;
  r.smoothed = 58;
  r.peak = 61;
  r.threshold = 100;
  r.batteryMv = 3987;
  r.flags = 0;
  return r;
}

void test_title() {
  char b[100];
  size_t n = formatTitle(b, sizeof b, 42, "1.2.3");
  TEST_ASSERT_EQUAL_STRING("# crowd-counter night=42 firmware=1.2.3\n", b);
  TEST_ASSERT_EQUAL(strlen(b), n);
}

void test_columns() {
  char b[400];
  size_t n = formatColumns(b, sizeof b);
  TEST_ASSERT_EQUAL_STRING(COLUMNS, b);
  TEST_ASSERT_EQUAL(strlen(COLUMNS), n);
}

void test_method_line() {
  char b[600];
  char expect[700];
  snprintf(expect, sizeof expect, "# method %s\n", METHOD_FIELDS);
  size_t n = formatMethod(b, sizeof b, method("Bar"));
  TEST_ASSERT_EQUAL_STRING(expect, b);
  TEST_ASSERT_EQUAL(strlen(expect), n);
}

void test_method_event_line() {
  char b[600];
  char expect[700];
  snprintf(expect, sizeof expect, "# event minute=17 method %s\n", METHOD_FIELDS);
  formatMethodEvent(b, sizeof b, 17, method("Bar"));
  TEST_ASSERT_EQUAL_STRING(expect, b);
}

void test_venue_sanitised() {
  char b[600];
  formatMethod(b, sizeof b, method("A\"b\\c,d\te"));
  TEST_ASSERT_NOT_NULL(strstr(b, "venue=\"A_b_c_d_e\" "));
  formatMethod(b, sizeof b, method(nullptr));
  TEST_ASSERT_NOT_NULL(strstr(b, "venue=\"\" "));
}

void test_resume_and_manual() {
  char b[200];
  formatResumeEvent(b, sizeof b, 33);
  TEST_ASSERT_EQUAL_STRING("# event minute=33 resume\n", b);
  formatAlertEvent(b, sizeof b, 77, 120);
  TEST_ASSERT_EQUAL_STRING("# event minute=77 alert threshold=120\n", b);
  TEST_ASSERT_EQUAL(0, formatAlertEvent(b, 10, 77, 120));
  formatManualCountEvent(b, sizeof b, 12, 85, 20.5f, 3.0f);
  TEST_ASSERT_EQUAL_STRING(
      "# event minute=12 manual_count truth=85 ble_signal=20.50 wifi_signal=3.00\n", b);
}

void test_row_exact() {
  char b[200];
  MinuteRecord r = record(5);
  r.flags = rowflag::WARMUP;
  size_t n = formatRow(b, sizeof b, r);
  const char* e = "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1\n";
  TEST_ASSERT_EQUAL_STRING(e, b);
  TEST_ASSERT_EQUAL(strlen(e), n);
}

void test_row_nonfinite_is_zero() {
  char b[200];
  MinuteRecord r = record(1);
  r.bleSignal = NAN;
  r.wifiSignal = INFINITY;
  formatRow(b, sizeof b, r);
  TEST_ASSERT_NOT_NULL(strstr(b, ",0.00,0.00,"));
}

static void roundTrip(const MinuteRecord& r) {
  char b[200];
  MinuteRecord o;
  memset(&o, 0xAB, sizeof o);
  TEST_ASSERT_TRUE(formatRow(b, sizeof b, r) > 0);
  TEST_ASSERT_TRUE(parseRow(b, o));
  TEST_ASSERT_EQUAL_UINT16(r.minute, o.minute);
  TEST_ASSERT_EQUAL_UINT32(r.epoch, o.epoch);
  for (uint8_t i = 0; i < CLASS_COUNT; ++i) TEST_ASSERT_EQUAL_UINT16(r.ble[i], o.ble[i]);
  TEST_ASSERT_EQUAL_UINT16(r.wifi, o.wifi);
  TEST_ASSERT_EQUAL_UINT16(r.wifiRotating, o.wifiRotating);
  TEST_ASSERT_EQUAL_UINT16(r.background, o.background);
  TEST_ASSERT_EQUAL_UINT16(r.allowlisted, o.allowlisted);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, r.bleSignal, o.bleSignal);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, r.wifiSignal, o.wifiSignal);
  TEST_ASSERT_EQUAL_UINT16(r.estimate, o.estimate);
  TEST_ASSERT_EQUAL_UINT16(r.smoothed, o.smoothed);
  TEST_ASSERT_EQUAL_UINT16(r.peak, o.peak);
  TEST_ASSERT_EQUAL_UINT16(r.threshold, o.threshold);
  TEST_ASSERT_EQUAL_UINT16(r.batteryMv, o.batteryMv);
  TEST_ASSERT_EQUAL_UINT8(r.flags, o.flags);
}

void test_round_trip() {
  roundTrip(record(0));
  roundTrip(record(1439));
  MinuteRecord r = record(7);
  r.flags = rowflag::WARMUP;
  r.epoch = 0;
  roundTrip(r);
  MinuteRecord m;
  memset(&m, 0xFF, sizeof m);
  m.bleSignal = 123.45f;
  m.wifiSignal = 0.0f;
  roundTrip(m);
}

void test_parse_rejects() {
  MinuteRecord o = record(99);
  MinuteRecord keep = o;
  char good[200];
  formatRow(good, sizeof good, record(3));
  char col[400];
  formatColumns(col, sizeof col);
  char ev[100];
  formatResumeEvent(ev, sizeof ev, 4);
  const char* bad[] = {
      nullptr,
      "",
      "\n",
      "# comment\n",
      col,
      ev,
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987\n",         // 20
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1,0\n",     // 22
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1,\n",      // trailing comma
      "5,1700000300,1,2,x,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1\n",       // letter
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1x\n",      // junk at end
      "5,1700000300,1,2,-3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1\n",      // negative
      "65536,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1\n",   // u16
      "5,4294967296,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1\n",       // u32
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,256\n",     // u8
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,abc,9.25,60,58,61,100,3987,1\n",         // float
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,-5,9.25,60,58,61,100,3987,1\n",         // negative ble_signal
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,-0.5,60,58,61,100,3987,1\n",      // negative wifi_signal
      "5,1700000300,1,2,3,4,5,6,7,40,30,5,2,nan,9.25,60,58,61,100,3987,1\n",         // nan
  };
  for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
    TEST_ASSERT_FALSE_MESSAGE(parseRow(bad[i], o), bad[i] ? bad[i] : "null");
    TEST_ASSERT_EQUAL_MEMORY(&keep, &o, sizeof o);
  }
  TEST_ASSERT_TRUE(parseRow(good, o));
}

void test_parse_crlf() {
  MinuteRecord o;
  TEST_ASSERT_TRUE(parseRow("5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1\r\n", o));
  TEST_ASSERT_EQUAL_UINT16(5, o.minute);
  TEST_ASSERT_TRUE(parseRow("5,1700000300,1,2,3,4,5,6,7,40,30,5,2,12.50,9.25,60,58,61,100,3987,1", o));
}

void test_buffer_bounds() {
  char tmp[200];
  MinuteRecord r = record(5);
  size_t need = formatRow(tmp, sizeof tmp, r);
  char b[200];
  memset(b, 'x', sizeof b);
  TEST_ASSERT_EQUAL(need, formatRow(b, need + 1, r));
  TEST_ASSERT_EQUAL_STRING(tmp, b);
  memset(b, 'x', sizeof b);
  TEST_ASSERT_EQUAL(0, formatRow(b, need, r));
  TEST_ASSERT_EQUAL_CHAR(0, b[0]);
  TEST_ASSERT_EQUAL(0, formatRow(b, 1, r));
  TEST_ASSERT_EQUAL_CHAR(0, b[0]);
  memset(b, 'x', sizeof b);
  TEST_ASSERT_EQUAL(0, formatRow(b, 0, r));
  TEST_ASSERT_EQUAL_CHAR('x', b[0]);
  TEST_ASSERT_EQUAL(0, formatRow(nullptr, 10, r));
  TEST_ASSERT_EQUAL(0, formatColumns(nullptr, 0));
  char m[50];
  TEST_ASSERT_EQUAL(0, formatMethod(m, sizeof m, method("Bar")));
  TEST_ASSERT_EQUAL_CHAR(0, m[0]);
  TEST_ASSERT_EQUAL(0, formatTitle(m, 5, 1, "v"));
}

void test_summary_empty() {
  NightSummaryBuilder s;
  NightSummary r = s.result();
  TEST_ASSERT_EQUAL_UINT16(0, r.minutes);
  TEST_ASSERT_EQUAL_UINT16(0, r.peak);
  TEST_ASSERT_EQUAL_UINT16(0, r.minutesAbove);
  TEST_ASSERT_EQUAL_UINT16(0, r.threshold);
  TEST_ASSERT_EQUAL_UINT32(0, r.startEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, r.endEpoch);
}

void test_summary_sequence() {
  NightSummaryBuilder s;
  MinuteRecord a = record(0);
  a.epoch = 0; a.smoothed = 200; a.threshold = 100; a.flags = rowflag::WARMUP; a.peak = 300;
  MinuteRecord b = record(1);
  b.epoch = 1000; b.smoothed = 100; b.threshold = 100; b.peak = 120;  // counts (>=)
  MinuteRecord c = record(2);
  c.epoch = 1060; c.smoothed = 99; c.threshold = 100; c.peak = 90;
  MinuteRecord d = record(3);
  d.epoch = 0; d.smoothed = 500; d.threshold = 0; d.peak = 10;  // threshold 0: never above
  MinuteRecord e = record(4);
  e.epoch = 1240; e.smoothed = 150; e.threshold = 140; e.peak = 150;
  s.add(a); s.add(b); s.add(c); s.add(d); s.add(e);
  NightSummary r = s.result();
  TEST_ASSERT_EQUAL_UINT16(5, r.minutes);
  TEST_ASSERT_EQUAL_UINT16(300, r.peak);
  TEST_ASSERT_EQUAL_UINT16(2, r.minutesAbove);
  TEST_ASSERT_EQUAL_UINT16(140, r.threshold);
  TEST_ASSERT_EQUAL_UINT32(1000, r.startEpoch);
  TEST_ASSERT_EQUAL_UINT32(1240, r.endEpoch);
}

void test_flags_warmup_alert_round_trip() {
  MinuteRecord r = record(9);
  r.flags = rowflag::WARMUP | rowflag::ALERT;
  roundTrip(r);
}

void test_summary_last_minute_and_alert() {
  NightSummaryBuilder s;
  TEST_ASSERT_FALSE(s.result().alertFired);
  MinuteRecord a = record(5), b = record(2), c = record(9);
  s.add(a); s.add(b);
  TEST_ASSERT_EQUAL_UINT16(5, s.result().lastMinute);  // highest, not last added
  TEST_ASSERT_FALSE(s.result().alertFired);
  c.flags = rowflag::ALERT;
  s.add(c);
  TEST_ASSERT_EQUAL_UINT16(9, s.result().lastMinute);
  TEST_ASSERT_EQUAL_UINT16(3, s.result().minutes);
  TEST_ASSERT_TRUE(s.result().alertFired);
  s.add(b);  // later row without the flag does not clear it
  TEST_ASSERT_TRUE(s.result().alertFired);
}

void test_summary_saturates() {
  NightSummaryBuilder s;
  MinuteRecord a = record(0);
  for (uint32_t i = 0; i < 70000; ++i) s.add(a);
  TEST_ASSERT_EQUAL_UINT16(65535, s.result().minutes);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_title);
  RUN_TEST(test_columns);
  RUN_TEST(test_method_line);
  RUN_TEST(test_method_event_line);
  RUN_TEST(test_venue_sanitised);
  RUN_TEST(test_resume_and_manual);
  RUN_TEST(test_row_exact);
  RUN_TEST(test_row_nonfinite_is_zero);
  RUN_TEST(test_round_trip);
  RUN_TEST(test_parse_rejects);
  RUN_TEST(test_parse_crlf);
  RUN_TEST(test_buffer_bounds);
  RUN_TEST(test_summary_empty);
  RUN_TEST(test_summary_sequence);
  RUN_TEST(test_flags_warmup_alert_round_trip);
  RUN_TEST(test_summary_last_minute_and_alert);
  RUN_TEST(test_summary_saturates);
  return UNITY_END();
}
