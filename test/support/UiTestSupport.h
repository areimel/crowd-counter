#pragma once
// Shared helpers for the UiController host tests.
#include <unity.h>
#include "UiController.h"

using cc::Screen;
using cc::SettingsItem;
using cc::UiAction;
using G = GestureEvent;

static inline cc::UiLimits limits() {
  cc::UiLimits l{};
  l.thresholdMin = 10; l.thresholdMax = 990; l.thresholdStep = 10;
  l.gateMin = -100; l.gateMax = -50; l.gateStep = 5;
  l.venueCount = 2; l.manualStep = 5;
  l.dimAfterMs = 30000; l.promptTimeoutMs = 15000;
  l.settingsTimeoutMs = 60000; l.confirmTimeoutMs = 5000;
  return l;
}

static inline cc::Settings settings(uint16_t th = 100, uint8_t v = 0, int8_t g = -80) {
  cc::Settings s{}; s.threshold = th; s.venue = v; s.rssiGate = g; return s;
}

static inline cc::UiEvent press(UiController& c, uint8_t b, G ev, uint16_t est = 0, uint32_t t = 0) {
  return c.onGesture(b, ev, est, t);
}

static inline void check(cc::UiEvent e, UiAction a, int32_t v = 0) {
  TEST_ASSERT_EQUAL((int)a, (int)e.action);
  TEST_ASSERT_EQUAL_INT32(v, e.value);
}

static inline UiController started(bool prompt = false, uint32_t t = 0) {
  UiController c(limits());
  c.begin(settings(), prompt, t);
  return c;
}
