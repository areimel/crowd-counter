#include "../support/UiTestSupport.h"

void setUp() {}
void tearDown() {}

// Move into settings and advance to `item` (KEY1 single from THRESHOLD).
static void toItem(UiController& c, SettingsItem item, uint16_t est = 0) {
  press(c, 0, G::HOLD);
  while (c.item() != item) press(c, 0, G::SINGLE, est);
}
void test_begin_copies_settings() {
  UiController c(limits());
  c.begin(settings(120, 1, -70), false, 0);
  TEST_ASSERT_EQUAL(120, c.threshold());
  TEST_ASSERT_EQUAL(1, c.venue());
  TEST_ASSERT_EQUAL(-70, c.rssiGate());
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  TEST_ASSERT_FALSE(c.dimmed());
}
void test_none_is_noop_and_not_a_press() {
  UiController c = started();
  check(press(c, 0, G::NONE, 0, 29000), UiAction::NONE);
  c.tick(30000);
  TEST_ASSERT_TRUE(c.dimmed());
  check(press(c, 1, G::NONE, 0, 31000), UiAction::NONE);
  TEST_ASSERT_TRUE(c.dimmed());
}
void test_key1_cycles_screens() {
  UiController c = started();
  press(c, 0, G::SINGLE);
  TEST_ASSERT_EQUAL((int)Screen::DETAIL, (int)c.screen());
  press(c, 0, G::SINGLE);
  TEST_ASSERT_EQUAL((int)Screen::SUMMARY, (int)c.screen());
  press(c, 0, G::SINGLE);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  check(press(c, 1, G::DOUBLE), UiAction::NONE);  // doubles ignored here
  check(press(c, 0, G::DOUBLE), UiAction::NONE);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
}
void test_settings_enter_exit() {
  UiController c = started();
  press(c, 0, G::SINGLE);  // DETAIL
  press(c, 0, G::HOLD);
  TEST_ASSERT_EQUAL((int)Screen::SETTINGS, (int)c.screen());
  TEST_ASSERT_EQUAL((int)SettingsItem::THRESHOLD, (int)c.item());
  press(c, 0, G::SINGLE);
  press(c, 0, G::HOLD);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  press(c, 0, G::HOLD);  // re-entry starts at THRESHOLD
  TEST_ASSERT_EQUAL((int)SettingsItem::THRESHOLD, (int)c.item());
}
void test_item_wraps() {
  UiController c = started();
  press(c, 0, G::HOLD);
  const SettingsItem order[] = {SettingsItem::VENUE, SettingsItem::RSSI_GATE,
                                SettingsItem::MANUAL_COUNT, SettingsItem::END_NIGHT,
                                SettingsItem::THRESHOLD};
  for (SettingsItem it : order) {
    press(c, 0, G::SINGLE);
    TEST_ASSERT_EQUAL((int)it, (int)c.item());
  }
}
void test_threshold_up_down_clamp() {
  UiController c(limits());
  c.begin(settings(980), false, 0);
  press(c, 0, G::HOLD);
  check(press(c, 1, G::SINGLE), UiAction::SET_THRESHOLD, 990);
  check(press(c, 1, G::SINGLE), UiAction::NONE);  // clamped, unchanged
  TEST_ASSERT_EQUAL(990, c.threshold());
  check(press(c, 1, G::DOUBLE), UiAction::SET_THRESHOLD, 980);
  c.begin(settings(20), false, 0);
  press(c, 0, G::HOLD);
  check(press(c, 1, G::DOUBLE), UiAction::SET_THRESHOLD, 10);
  check(press(c, 1, G::DOUBLE), UiAction::NONE);
  TEST_ASSERT_EQUAL(10, c.threshold());
}
void test_venue_wraps_both_ways() {
  UiController c = started();
  toItem(c, SettingsItem::VENUE);
  check(press(c, 1, G::SINGLE), UiAction::SET_VENUE, 1);
  check(press(c, 1, G::SINGLE), UiAction::SET_VENUE, 0);
  check(press(c, 1, G::DOUBLE), UiAction::SET_VENUE, 1);
  check(press(c, 1, G::DOUBLE), UiAction::SET_VENUE, 0);
  TEST_ASSERT_EQUAL(0, c.venue());
}
void test_single_venue_is_noop() {
  cc::UiLimits l = limits();
  l.venueCount = 1;
  UiController c(l);
  c.begin(settings(), false, 0);
  toItem(c, SettingsItem::VENUE);
  check(press(c, 1, G::SINGLE), UiAction::NONE);
  check(press(c, 1, G::DOUBLE), UiAction::NONE);
}
void test_gate_clamp() {
  UiController c(limits());
  c.begin(settings(100, 0, -55), false, 0);
  toItem(c, SettingsItem::RSSI_GATE);
  check(press(c, 1, G::SINGLE), UiAction::SET_RSSI_GATE, -50);
  check(press(c, 1, G::SINGLE), UiAction::NONE);
  check(press(c, 1, G::DOUBLE), UiAction::SET_RSSI_GATE, -55);
  c.begin(settings(100, 0, -95), false, 0);
  toItem(c, SettingsItem::RSSI_GATE);
  check(press(c, 1, G::DOUBLE), UiAction::SET_RSSI_GATE, -100);
  check(press(c, 1, G::DOUBLE), UiAction::NONE);
  TEST_ASSERT_EQUAL(-100, c.rssiGate());
}
void test_manual_seeded_rounded_and_reseeded() {
  UiController c = started();
  toItem(c, SettingsItem::MANUAL_COUNT, 122);
  TEST_ASSERT_EQUAL(120, c.manualCount());
  press(c, 1, G::SINGLE);
  TEST_ASSERT_EQUAL(125, c.manualCount());
  // go around and re-enter with a new estimate (123 -> 125)
  while (c.item() != SettingsItem::THRESHOLD) press(c, 0, G::SINGLE, 123);
  while (c.item() != SettingsItem::MANUAL_COUNT) press(c, 0, G::SINGLE, 123);
  TEST_ASSERT_EQUAL(125, c.manualCount());
}
void test_manual_rounding_tie_goes_up() {
  cc::UiLimits l = limits();
  l.manualStep = 10;
  UiController c(l);
  c.begin(settings(), false, 0);
  toItem(c, SettingsItem::MANUAL_COUNT, 125);
  TEST_ASSERT_EQUAL(130, c.manualCount());
}
void test_manual_clamps_and_saves() {
  UiController c = started();
  toItem(c, SettingsItem::MANUAL_COUNT, 3);
  TEST_ASSERT_EQUAL(5, c.manualCount());
  check(press(c, 1, G::DOUBLE), UiAction::NONE);
  TEST_ASSERT_EQUAL(0, c.manualCount());
  check(press(c, 1, G::DOUBLE), UiAction::NONE);
  TEST_ASSERT_EQUAL(0, c.manualCount());
  press(c, 1, G::SINGLE);
  check(press(c, 1, G::HOLD), UiAction::SAVE_MANUAL_COUNT, 5);
  TEST_ASSERT_EQUAL((int)Screen::SETTINGS, (int)c.screen());
  TEST_ASSERT_EQUAL((int)SettingsItem::MANUAL_COUNT, (int)c.item());
}
void test_manual_upper_clamp() {
  UiController c = started();
  toItem(c, SettingsItem::MANUAL_COUNT, 9998);
  TEST_ASSERT_EQUAL(UiController::MANUAL_COUNT_MAX, c.manualCount());
  press(c, 1, G::SINGLE);
  TEST_ASSERT_EQUAL(UiController::MANUAL_COUNT_MAX, c.manualCount());
}
void test_end_night_confirm() {
  UiController c = started();
  toItem(c, SettingsItem::END_NIGHT);
  check(press(c, 1, G::SINGLE), UiAction::NONE);
  check(press(c, 1, G::DOUBLE), UiAction::NONE);
  TEST_ASSERT_EQUAL((int)Screen::SETTINGS, (int)c.screen());
  check(press(c, 1, G::HOLD), UiAction::END_NIGHT);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
}
void test_key2_hold_other_items_none() {
  UiController c = started();
  press(c, 0, G::HOLD);
  check(press(c, 1, G::HOLD), UiAction::NONE);
  press(c, 0, G::SINGLE);
  check(press(c, 1, G::HOLD), UiAction::NONE);
  press(c, 0, G::SINGLE);
  check(press(c, 1, G::HOLD), UiAction::NONE);
}
void test_capture_from_normal_screens_only() {
  UiController c = started();
  for (int i = 0; i < 3; i++) {
    Screen s = c.screen();
    check(press(c, 1, G::HOLD), UiAction::NONE);  // arms
    TEST_ASSERT_TRUE(c.confirmingCapture());
    check(press(c, 1, G::HOLD), UiAction::START_CAPTURE);
    TEST_ASSERT_FALSE(c.confirmingCapture());
    TEST_ASSERT_EQUAL((int)s, (int)c.screen());
    press(c, 0, G::SINGLE);
  }
  press(c, 0, G::HOLD);  // threshold item: no capture, no arming
  check(press(c, 1, G::HOLD), UiAction::NONE);
  TEST_ASSERT_FALSE(c.confirmingCapture());
}
void test_wake_press_consumed_normal() {
  UiController c = started();
  check(press(c, 1, G::SINGLE), UiAction::NONE);  // KEY2 short dims
  TEST_ASSERT_TRUE(c.dimmed());
  check(press(c, 0, G::SINGLE), UiAction::NONE);
  TEST_ASSERT_FALSE(c.dimmed());
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  press(c, 1, G::SINGLE);
  check(press(c, 1, G::HOLD), UiAction::NONE);  // no arming on wake press
  TEST_ASSERT_FALSE(c.dimmed());
  TEST_ASSERT_FALSE(c.confirmingCapture());
  press(c, 1, G::SINGLE);
  press(c, 1, G::SINGLE);  // wake only: does not re-dim
  TEST_ASSERT_FALSE(c.dimmed());
}
void test_wake_press_consumed_settings() {
  UiController c = started();
  press(c, 0, G::HOLD);
  c.tick(30000);
  TEST_ASSERT_TRUE(c.dimmed());
  check(press(c, 1, G::SINGLE, 0, 30001), UiAction::NONE);
  TEST_ASSERT_FALSE(c.dimmed());
  TEST_ASSERT_EQUAL(100, c.threshold());
  c.tick(60001);
  TEST_ASSERT_TRUE(c.dimmed());
  check(press(c, 0, G::SINGLE, 0, 60002), UiAction::NONE);
  TEST_ASSERT_EQUAL((int)SettingsItem::THRESHOLD, (int)c.item());
}
void test_auto_dim_boundary() {
  UiController c = started(false, 1000);
  c.tick(30999);
  TEST_ASSERT_FALSE(c.dimmed());
  c.tick(31000);
  TEST_ASSERT_TRUE(c.dimmed());
}
void test_press_restarts_dim_timer() {
  UiController c = started();
  press(c, 0, G::SINGLE, 0, 20000);
  c.tick(49999);
  TEST_ASSERT_FALSE(c.dimmed());
  c.tick(50000);
  TEST_ASSERT_TRUE(c.dimmed());
}
void test_auto_dim_in_settings() {
  UiController c = started();
  press(c, 0, G::HOLD, 0, 0);
  c.tick(30000);
  TEST_ASSERT_TRUE(c.dimmed());
}
void test_wake_undims_and_restarts() {
  UiController c = started();
  c.tick(30000);
  TEST_ASSERT_TRUE(c.dimmed());
  c.wake(40000);
  TEST_ASSERT_FALSE(c.dimmed());
  c.tick(69999);
  TEST_ASSERT_FALSE(c.dimmed());
  c.tick(70000);
  TEST_ASSERT_TRUE(c.dimmed());
}
void test_prompt_key1_new_night() {
  UiController c = started(true);
  TEST_ASSERT_EQUAL((int)Screen::NIGHT_PROMPT, (int)c.screen());
  check(press(c, 0, G::SINGLE, 0, 100), UiAction::NEW_NIGHT);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  c = started(true);
  check(press(c, 0, G::HOLD, 0, 100), UiAction::NEW_NIGHT);
}
void test_prompt_key2_resume() {
  UiController c = started(true);
  check(press(c, 1, G::SINGLE, 0, 100), UiAction::RESUME_NIGHT);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  c = started(true);
  check(press(c, 1, G::HOLD, 0, 100), UiAction::RESUME_NIGHT);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
}
void test_prompt_timeout_and_countdown() {
  UiController c = started(true, 5000);
  TEST_ASSERT_EQUAL(15, c.promptSeconds());
  c.tick(5001);
  TEST_ASSERT_EQUAL(15, c.promptSeconds());
  c.tick(6000);
  TEST_ASSERT_EQUAL(14, c.promptSeconds());
  c.tick(19999);
  TEST_ASSERT_EQUAL(1, c.promptSeconds());
  check(c.tick(19999), UiAction::NONE);
  check(c.tick(20000), UiAction::NEW_NIGHT);
  TEST_ASSERT_EQUAL((int)Screen::COUNT, (int)c.screen());
  TEST_ASSERT_EQUAL(0, c.promptSeconds());
  check(c.tick(20001), UiAction::NONE);
}
void test_prompt_never_dims_and_dim_timer_fresh_after() {
  UiController c = started(true);
  c.tick(14999);
  TEST_ASSERT_FALSE(c.dimmed());
  c.tick(15000);  // timeout -> NEW_NIGHT
  c.tick(44999);
  TEST_ASSERT_FALSE(c.dimmed());
  c.tick(45000);
  TEST_ASSERT_TRUE(c.dimmed());
}
void test_wants_per_screen() {
  UiController c = started();
  TEST_ASSERT_FALSE(c.wantsDouble(0));
  TEST_ASSERT_FALSE(c.wantsDouble(1));
  TEST_ASSERT_TRUE(c.wantsHold(0));
  TEST_ASSERT_TRUE(c.wantsHold(1));
  press(c, 0, G::HOLD);
  TEST_ASSERT_FALSE(c.wantsDouble(0));
  TEST_ASSERT_TRUE(c.wantsDouble(1));
  TEST_ASSERT_TRUE(c.wantsHold(0));
  TEST_ASSERT_TRUE(c.wantsHold(1));
  c = started(true);
  TEST_ASSERT_FALSE(c.wantsDouble(1));
  TEST_ASSERT_FALSE(c.wantsHold(0));
  TEST_ASSERT_FALSE(c.wantsHold(1));
  TEST_ASSERT_FALSE(c.wantsHold(2));
  TEST_ASSERT_FALSE(c.wantsDouble(2));
  c = started();
  TEST_ASSERT_FALSE(c.wantsHold(2));
  check(press(c, 2, G::SINGLE), UiAction::NONE);  // bad button
}
void test_millis_wrap() {
  UiController c = started(false, 0xFFFFFFF0u);
  c.tick(0xFFFFFFF0u + 29999u);
  TEST_ASSERT_FALSE(c.dimmed());
  c.tick(0xFFFFFFF0u + 30000u);  // wraps past zero
  TEST_ASSERT_TRUE(c.dimmed());
  UiController p = started(true, 0xFFFFFFF0u);
  p.tick(0xFFFFFFF0u + 1000u);
  TEST_ASSERT_EQUAL(14, p.promptSeconds());
  check(p.tick(0xFFFFFFF0u + 15000u), UiAction::NEW_NIGHT);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_begin_copies_settings); RUN_TEST(test_none_is_noop_and_not_a_press); RUN_TEST(test_key1_cycles_screens);
  RUN_TEST(test_settings_enter_exit); RUN_TEST(test_item_wraps); RUN_TEST(test_threshold_up_down_clamp);
  RUN_TEST(test_venue_wraps_both_ways); RUN_TEST(test_single_venue_is_noop); RUN_TEST(test_gate_clamp);
  RUN_TEST(test_manual_seeded_rounded_and_reseeded); RUN_TEST(test_manual_rounding_tie_goes_up); RUN_TEST(test_manual_clamps_and_saves);
  RUN_TEST(test_manual_upper_clamp); RUN_TEST(test_end_night_confirm); RUN_TEST(test_key2_hold_other_items_none);
  RUN_TEST(test_capture_from_normal_screens_only); RUN_TEST(test_wake_press_consumed_normal); RUN_TEST(test_wake_press_consumed_settings);
  RUN_TEST(test_auto_dim_boundary); RUN_TEST(test_press_restarts_dim_timer); RUN_TEST(test_auto_dim_in_settings);
  RUN_TEST(test_wake_undims_and_restarts); RUN_TEST(test_prompt_key1_new_night); RUN_TEST(test_prompt_key2_resume);
  RUN_TEST(test_prompt_timeout_and_countdown); RUN_TEST(test_prompt_never_dims_and_dim_timer_fresh_after); RUN_TEST(test_wants_per_screen);
  RUN_TEST(test_millis_wrap);
  return UNITY_END();
}
