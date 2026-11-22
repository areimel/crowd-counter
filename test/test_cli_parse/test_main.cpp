#include <unity.h>
#include <string.h>
#include "CliParse.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static void expect(const char* line, CliCommand c, bool hasArg = false, uint32_t arg = 0) {
  CliRequest r = parseCli(line);
  TEST_ASSERT_EQUAL_MESSAGE(int(c), int(r.command), line);
  TEST_ASSERT_EQUAL_MESSAGE(hasArg, r.hasArg, line);
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(arg, r.arg, line);
}

void test_simple_commands() {
  expect("help", CliCommand::HELP);
  expect("status", CliCommand::STATUS);
  expect("nights", CliCommand::NIGHTS);
  expect("shot", CliCommand::SHOT);
  expect("export", CliCommand::EXPORT);
  expect("summary", CliCommand::SUMMARY);
}

void test_case_and_whitespace() {
  expect("HELP", CliCommand::HELP);
  expect("StAtUs", CliCommand::STATUS);
  expect("  \t shot \t ", CliCommand::SHOT);
  expect("help\r\n", CliCommand::HELP);
  expect("export\t 7  \r", CliCommand::EXPORT, true, 7);
}

void test_empty() {
  expect("", CliCommand::EMPTY);
  expect("   \t ", CliCommand::EMPTY);
  expect("\r\n", CliCommand::EMPTY);
  expect(nullptr, CliCommand::EMPTY);
}

void test_optional_args() {
  expect("export 3", CliCommand::EXPORT, true, 3);
  expect("EXPORT 0", CliCommand::EXPORT, true, 0);
  expect("summary 12", CliCommand::SUMMARY, true, 12);
  expect("summary", CliCommand::SUMMARY, false, 0);
  expect("export x", CliCommand::UNKNOWN);
  expect("export -1", CliCommand::UNKNOWN);
  expect("export +1", CliCommand::UNKNOWN);
  expect("export 1x", CliCommand::UNKNOWN);
}

void test_time() {
  expect("time 1700000000", CliCommand::TIME, true, 1700000000u);
  expect("time 4294967295", CliCommand::TIME, true, 4294967295u);
  expect("time 0", CliCommand::TIME, true, 0);
  expect("time", CliCommand::UNKNOWN);
  expect("time 4294967296", CliCommand::UNKNOWN);
  expect("time 99999999999999999999", CliCommand::UNKNOWN);
  expect("time -5", CliCommand::UNKNOWN);
  expect("time abc", CliCommand::UNKNOWN);
  expect("time 1 2", CliCommand::UNKNOWN);
}

void test_unknown() {
  expect("garbage", CliCommand::UNKNOWN);
  expect("hel", CliCommand::UNKNOWN);
  expect("helpme", CliCommand::UNKNOWN);
  expect("stat", CliCommand::UNKNOWN);
  expect("help me", CliCommand::UNKNOWN);
  expect("status 1", CliCommand::UNKNOWN);
  expect("nights 1", CliCommand::UNKNOWN);
  expect("shot 1", CliCommand::UNKNOWN);
  expect("export 1 2", CliCommand::UNKNOWN);
  expect("1234", CliCommand::UNKNOWN);
}

void test_command_name() {
  TEST_ASSERT_EQUAL_STRING("export", cliCommandName(CliCommand::EXPORT));
  TEST_ASSERT_EQUAL_STRING("unknown", cliCommandName(CliCommand::UNKNOWN));
}

static bool feedStr(LineAccumulator<8>& a, const char* s) {
  bool any = false;
  for (; *s; ++s) any = a.feed(*s) || any;
  return any;
}

void test_acc_terminators() {
  LineAccumulator<8> a;
  TEST_ASSERT_FALSE(a.feed('a'));
  TEST_ASSERT_FALSE(a.feed('b'));
  TEST_ASSERT_TRUE(a.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("ab", a.line());
  TEST_ASSERT_FALSE(a.feed('c'));
  TEST_ASSERT_TRUE(a.feed('\r'));
  TEST_ASSERT_EQUAL_STRING("c", a.line());
}

void test_acc_crlf_single_report() {
  LineAccumulator<8> a;
  TEST_ASSERT_FALSE(a.feed('h'));
  TEST_ASSERT_TRUE(a.feed('\r'));
  TEST_ASSERT_FALSE(a.feed('\n'));
  TEST_ASSERT_FALSE(a.feed('\n'));
  TEST_ASSERT_FALSE(a.feed('\r'));
}

void test_acc_stream() {
  LineAccumulator<16> a;
  const char* s = "one\r\ntwo\nthree\r";
  const char* want[] = {"one", "two", "three"};
  int n = 0;
  for (; *s; ++s)
    if (a.feed(*s)) {
      TEST_ASSERT_TRUE(n < 3);
      TEST_ASSERT_EQUAL_STRING(want[n], a.line());
      ++n;
    }
  TEST_ASSERT_EQUAL_INT(3, n);
}

void test_acc_overlong_dropped() {
  LineAccumulator<8> a;
  TEST_ASSERT_FALSE(feedStr(a, "123456789012345\n"));
  TEST_ASSERT_TRUE(feedStr(a, "ok\n"));
  TEST_ASSERT_EQUAL_STRING("ok", a.line());
  // exactly 8 chars (one over N-1) dropped, with CRLF
  TEST_ASSERT_FALSE(feedStr(a, "12345678\r\n"));
  TEST_ASSERT_TRUE(feedStr(a, "next\r\n"));
  TEST_ASSERT_EQUAL_STRING("next", a.line());
}

void test_acc_exact_length() {
  LineAccumulator<8> a;
  TEST_ASSERT_TRUE(feedStr(a, "1234567\n"));
  TEST_ASSERT_EQUAL_STRING("1234567", a.line());
}

void test_acc_nul_ignored() {
  LineAccumulator<8> a;
  TEST_ASSERT_FALSE(a.feed('\0'));
  TEST_ASSERT_FALSE(a.feed('a'));
  TEST_ASSERT_FALSE(a.feed('\0'));
  TEST_ASSERT_TRUE(a.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("a", a.line());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple_commands);
  RUN_TEST(test_case_and_whitespace);
  RUN_TEST(test_empty);
  RUN_TEST(test_optional_args);
  RUN_TEST(test_time);
  RUN_TEST(test_unknown);
  RUN_TEST(test_command_name);
  RUN_TEST(test_acc_terminators);
  RUN_TEST(test_acc_crlf_single_report);
  RUN_TEST(test_acc_stream);
  RUN_TEST(test_acc_overlong_dropped);
  RUN_TEST(test_acc_exact_length);
  RUN_TEST(test_acc_nul_ignored);
  return UNITY_END();
}
