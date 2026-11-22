#include <unity.h>
#include "Gesture.h"
#include <cstdint>

static const GestureEvent NONE = GestureEvent::NONE, TAP = GestureEvent::SINGLE,
                          DOUBLE = GestureEvent::DOUBLE, HOLD = GestureEvent::HOLD;

void setUp() {}
void tearDown() {}

// Times supplied to press/release denote raw electrical edges; debounce is 25ms.
static void press(Gesture& g, uint32_t t) {
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, t));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, t + 25));
}
static GestureEvent release(Gesture& g, uint32_t t) {
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, t));
  return g.update(false, t + 25);
}
static Gesture make(bool dbl, bool hold) {
  Gesture g(25, 250, 650);
  g.setAssigned(dbl, hold);
  return g;
}

void test_noise_is_not_a_press_and_unassigned_taps_fire_immediately() {
  Gesture g = make(false, false);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, 10));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 20)); // noise never becomes a press
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 45));
  TEST_ASSERT_FALSE(g.pressed());
  press(g, 100);
  TEST_ASSERT_TRUE(g.pressed());
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 150));
  press(g, 180);
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 230)); // no double assignment: two immediate taps
  press(g, 300);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, 2000));
  TEST_ASSERT_FALSE(g.holdFired());
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 2100)); // unassigned hold is ordinary tap
}

void test_hold_fires_once_at_threshold_and_release_is_silent() {
  Gesture g = make(false, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, 684));
  TEST_ASSERT_FALSE(g.holdFired());
  TEST_ASSERT_EQUAL_INT((int)HOLD, (int)g.update(true, 685));
  TEST_ASSERT_TRUE(g.holdFired());
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, 2000));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 2100));
  TEST_ASSERT_FALSE(g.holdFired());
  press(g, 2200);
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 2300));
}

void test_release_before_hold_threshold_is_tap() {
  Gesture g = make(false, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 680));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 685)); // release before threshold cannot hold
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)g.update(false, 705));
}

void test_double_and_hold_arbitration_resolves_single_or_double() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 60));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 334));
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)g.update(false, 335));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 500));
  press(g, 600);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 650));
  press(g, 700);
  TEST_ASSERT_EQUAL_INT((int)DOUBLE, (int)release(g, 800));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 1100));
}

void test_second_press_locks_arbitration_and_second_hold_wins() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 60));
  press(g, 100);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, 400)); // second press locks arbitration
  TEST_ASSERT_EQUAL_INT((int)HOLD, (int)g.update(true, 775)); // second hold wins
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 900));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 1500));
}

void test_reset_discards_pending_and_held_button_must_release_first() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 60));
  g.reset(false, 100); // pending action discarded on disconnect
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 500));
  g.reset(true, 600); // held button across reconnect must release first
  TEST_ASSERT_TRUE(g.pressed());
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, 2000));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 2100));
  press(g, 2200);
  TEST_ASSERT_EQUAL_INT((int)HOLD, (int)g.update(true, 2875));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 3000));
}

void test_timing_survives_unsigned_clock_wrap() {
  Gesture g = make(true, true);
  const uint32_t start = UINT32_MAX - 100;
  g.reset(false, start);
  press(g, start + 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, start + 60));
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)g.update(false, start + 335)); // wrapped unsigned clock
  press(g, start + 400);
  TEST_ASSERT_EQUAL_INT((int)HOLD, (int)g.update(true, start + 1075));
}

void test_independent_gestures_do_not_interfere() {
  Gesture a = make(false, false), b = make(false, true);
  press(a, 10);
  press(b, 10);
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(a, 100));
  TEST_ASSERT_EQUAL_INT((int)HOLD, (int)b.update(true, 685));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(b, 700));
}

// cancel(): drops a pending single so it never fires
void test_cancel_drops_pending_single() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 60)); // pending
  g.cancel();
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 400));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 2000));
  press(g, 3000);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 3050)); // new pending works normally
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)g.update(false, 3400));
}

// cancel(): drops a second press in progress (no double on release)
void test_cancel_drops_second_press_in_progress() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 60));
  press(g, 100); // second press
  g.cancel();
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 200));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 1000));
}

// cancel(): blocks a held button until release; no tap, no hold
void test_cancel_blocks_held_button_until_release() {
  Gesture g = make(false, true);
  press(g, 10);
  g.cancel();
  TEST_ASSERT_TRUE(g.pressed());
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, 2000)); // no hold after cancel
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 2100));
  press(g, 2200);
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 2300)); // fully recovered
}

// cancel() after a hold fired: holdFired cleared, release silent
void test_cancel_after_hold_fired_clears_hold_and_release_is_silent() {
  Gesture g = make(false, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)HOLD, (int)g.update(true, 685));
  g.cancel();
  TEST_ASSERT_FALSE(g.holdFired());
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 800));
  press(g, 900);
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 950));
}

// cancel() while idle is a no-op
void test_cancel_while_idle_is_noop() {
  Gesture g = make(false, false);
  g.cancel();
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 60));
}

// setAssigned between gestures takes effect immediately
void test_setAssigned_between_gestures_takes_effect_immediately() {
  Gesture g = make(false, false);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 60)); // immediate single
  g.setAssigned(true, false);
  press(g, 200);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 250)); // now waits for double window
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)g.update(false, 525));
  g.setAssigned(false, true);
  press(g, 600);
  TEST_ASSERT_EQUAL_INT((int)HOLD, (int)g.update(true, 1275));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 1400));
  g.setAssigned(false, false);
  press(g, 1500);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(true, 3000));
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)release(g, 3100));
}

// setAssigned mid-gesture is safe: disabling hold while held keeps release silent
void test_setAssigned_mid_gesture_keeps_release_silent() {
  Gesture g = make(false, true);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)HOLD, (int)g.update(true, 685));
  g.setAssigned(false, false);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 800));
  TEST_ASSERT_FALSE(g.holdFired());
}

// disabling double while single is pending: the pending single still resolves
void test_disabling_double_resolves_pending_single() {
  Gesture g = make(true, false);
  press(g, 10);
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)release(g, 60));
  g.setAssigned(false, false);
  TEST_ASSERT_EQUAL_INT((int)TAP, (int)g.update(false, 335));
  TEST_ASSERT_EQUAL_INT((int)NONE, (int)g.update(false, 1000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_noise_is_not_a_press_and_unassigned_taps_fire_immediately);
  RUN_TEST(test_hold_fires_once_at_threshold_and_release_is_silent);
  RUN_TEST(test_release_before_hold_threshold_is_tap);
  RUN_TEST(test_double_and_hold_arbitration_resolves_single_or_double);
  RUN_TEST(test_second_press_locks_arbitration_and_second_hold_wins);
  RUN_TEST(test_reset_discards_pending_and_held_button_must_release_first);
  RUN_TEST(test_timing_survives_unsigned_clock_wrap);
  RUN_TEST(test_independent_gestures_do_not_interfere);
  RUN_TEST(test_cancel_drops_pending_single);
  RUN_TEST(test_cancel_drops_second_press_in_progress);
  RUN_TEST(test_cancel_blocks_held_button_until_release);
  RUN_TEST(test_cancel_after_hold_fired_clears_hold_and_release_is_silent);
  RUN_TEST(test_cancel_while_idle_is_noop);
  RUN_TEST(test_setAssigned_between_gestures_takes_effect_immediately);
  RUN_TEST(test_setAssigned_mid_gesture_keeps_release_silent);
  RUN_TEST(test_disabling_double_resolves_pending_single);
  return UNITY_END();
}
