// Setup-capture confirmation, showCount() and the settings idle timeout.
#include "../support/UiTestSupport.h"

void setUp() {}
void tearDown() {}

static void toManual(UiController& c, uint32_t t) {
  press(c, 0, G::HOLD, 0, t);
  while (c.item() != SettingsItem::MANUAL_COUNT) press(c, 0, G::SINGLE, 100, t);
}

void test_first_hold_arms_without_action() {
  UiController c = started();
  check(press(c, 1, G::HOLD, 0, 100), UiAction::NONE);
  TEST_ASSERT_TRUE(c.confirmingCapture());
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
}
void test_second_hold_emits_exactly_one_capture() {
  UiController c = started();
  press(c, 1, G::HOLD, 0, 100);
  check(press(c, 1, G::HOLD, 0, 4000), UiAction::START_CAPTURE);
  TEST_ASSERT_FALSE(c.confirmingCapture());
  check(press(c, 1, G::HOLD, 0, 4100), UiAction::NONE);  // a third hold re-arms only
  TEST_ASSERT_TRUE(c.confirmingCapture());
  check(c.tick(4200), UiAction::NONE);
}
void test_confirm_works_on_detail_and_summary() {
  UiController c = started();
  for (int i = 0; i < 2; i++) {
    press(c, 0, G::SINGLE);
    press(c, 1, G::HOLD);
    TEST_ASSERT_TRUE(c.confirmingCapture());
    check(press(c, 1, G::HOLD), UiAction::START_CAPTURE);
  }
}
void test_timeout_does_nothing() {
  UiController c = started();
  press(c, 1, G::HOLD, 0, 1000);
  check(c.tick(5999), UiAction::NONE);
  TEST_ASSERT_TRUE(c.confirmingCapture());
  check(c.tick(6000), UiAction::NONE);
  TEST_ASSERT_FALSE(c.confirmingCapture());
  // a hold after expiry arms again instead of capturing, even without a tick
  press(c, 1, G::HOLD, 0, 7000);
  TEST_ASSERT_TRUE(c.confirmingCapture());
  check(press(c, 1, G::HOLD, 0, 12000), UiAction::NONE);  // late: re-arms
  TEST_ASSERT_TRUE(c.confirmingCapture());
}
void test_key1_short_cancels_and_does_not_cycle() {
  UiController c = started();
  press(c, 1, G::HOLD);
  check(press(c, 0, G::SINGLE), UiAction::NONE);
  TEST_ASSERT_FALSE(c.confirmingCapture());
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  press(c, 0, G::SINGLE);  // next press works normally
  TEST_ASSERT_EQUAL((int)Screen::DETAIL, (int)c.screen());
}
void test_other_gestures_cancel() {
  UiController c = started();
  press(c, 1, G::HOLD);
  press(c, 1, G::SINGLE);  // KEY2 short: cancels, does not dim
  TEST_ASSERT_FALSE(c.confirmingCapture());
  TEST_ASSERT_FALSE(c.dimmed());
  press(c, 1, G::HOLD);
  press(c, 0, G::HOLD);  // KEY1 hold: cancels, does not open settings
  TEST_ASSERT_FALSE(c.confirmingCapture());
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  check(press(c, 1, G::HOLD), UiAction::NONE);  // and the next hold arms afresh
  TEST_ASSERT_TRUE(c.confirmingCapture());
}
void test_no_auto_dim_while_confirming() {
  cc::UiLimits l = limits();
  l.confirmTimeoutMs = 60000;  // longer than the dim delay
  UiController c(l);
  c.begin(settings(), false, 0);
  press(c, 1, G::HOLD, 0, 1000);
  c.tick(40000);
  TEST_ASSERT_FALSE(c.dimmed());
  c.tick(61000);  // expired: now it may dim
  TEST_ASSERT_FALSE(c.confirmingCapture());
  c.tick(61001);
  TEST_ASSERT_TRUE(c.dimmed());
}
void test_wants_in_confirm_state() {
  UiController c = started();
  press(c, 1, G::HOLD);
  TEST_ASSERT_TRUE(c.wantsHold(0));
  TEST_ASSERT_TRUE(c.wantsHold(1));
  TEST_ASSERT_FALSE(c.wantsDouble(0));
  TEST_ASSERT_FALSE(c.wantsDouble(1));
}
void test_show_count_from_other_screens() {
  UiController c = started();
  press(c, 0, G::SINGLE);
  c.tick(30000);
  TEST_ASSERT_TRUE(c.dimmed());
  c.showCount(31000);
  TEST_ASSERT_FALSE(c.dimmed());
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  press(c, 0, G::SINGLE); press(c, 0, G::SINGLE);
  TEST_ASSERT_EQUAL((int)Screen::SUMMARY, (int)c.screen());
  c.showCount(32000);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  c.tick(61999);
  TEST_ASSERT_FALSE(c.dimmed());  // dim timer restarted
}
void test_show_count_from_settings_discards_manual() {
  UiController c = started();
  toManual(c, 0);
  press(c, 1, G::SINGLE, 100);
  TEST_ASSERT_TRUE(c.manualCount() != 0);
  c.showCount(1000);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  TEST_ASSERT_EQUAL(0, c.manualCount());
  press(c, 0, G::HOLD);
  TEST_ASSERT_EQUAL((int)SettingsItem::THRESHOLD, (int)c.item());
}
void test_show_count_on_prompt_only_wakes() {
  UiController c = started(true);
  c.showCount(1000);
  TEST_ASSERT_EQUAL((int)Screen::NIGHT_PROMPT, (int)c.screen());
  check(c.tick(15000), UiAction::NEW_NIGHT);
}
void test_show_count_keeps_confirm_on_count_only() {
  UiController c = started();
  press(c, 1, G::HOLD, 0, 100);
  c.showCount(200);
  TEST_ASSERT_TRUE(c.confirmingCapture());
  c = started();
  press(c, 0, G::SINGLE);
  press(c, 1, G::HOLD);
  c.showCount(200);
  TEST_ASSERT_FALSE(c.confirmingCapture());
}
void test_settings_timeout_returns_to_count() {
  UiController c = started();
  press(c, 0, G::HOLD, 0, 1000);
  press(c, 0, G::SINGLE, 0, 20000);  // activity restarts the idle timer
  check(c.tick(79999), UiAction::NONE);
  TEST_ASSERT_EQUAL((int)Screen::SETTINGS, (int)c.screen());
  check(c.tick(80000), UiAction::NONE);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
}
void test_settings_timeout_discards_manual_and_zero_disables() {
  UiController c = started();
  toManual(c, 0);
  press(c, 1, G::SINGLE, 100, 10);
  c.tick(60010);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  TEST_ASSERT_EQUAL(0, c.manualCount());
  cc::UiLimits l = limits();
  l.settingsTimeoutMs = 0;
  UiController d(l);
  d.begin(settings(), false, 0);
  press(d, 0, G::HOLD, 0, 0);
  d.tick(10u * 60u * 1000u);
  TEST_ASSERT_EQUAL((int)Screen::SETTINGS, (int)d.screen());
}
void test_settings_timeout_while_dimmed_wake_press_consumed() {
  UiController c = started();
  press(c, 0, G::HOLD, 0, 0);
  c.tick(30000);
  TEST_ASSERT_TRUE(c.dimmed());
  check(press(c, 1, G::SINGLE, 0, 30001), UiAction::NONE);  // wake: stays in settings
  TEST_ASSERT_EQUAL((int)Screen::SETTINGS, (int)c.screen());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_hold_arms_without_action); RUN_TEST(test_second_hold_emits_exactly_one_capture);
  RUN_TEST(test_confirm_works_on_detail_and_summary); RUN_TEST(test_timeout_does_nothing);
  RUN_TEST(test_key1_short_cancels_and_does_not_cycle); RUN_TEST(test_other_gestures_cancel);
  RUN_TEST(test_no_auto_dim_while_confirming); RUN_TEST(test_wants_in_confirm_state);
  RUN_TEST(test_show_count_from_other_screens); RUN_TEST(test_show_count_from_settings_discards_manual);
  RUN_TEST(test_show_count_on_prompt_only_wakes); RUN_TEST(test_show_count_keeps_confirm_on_count_only);
  RUN_TEST(test_settings_timeout_returns_to_count); RUN_TEST(test_settings_timeout_discards_manual_and_zero_disables);
  RUN_TEST(test_settings_timeout_while_dimmed_wake_press_consumed);
  return UNITY_END();
}
