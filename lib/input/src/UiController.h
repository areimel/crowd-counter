#pragma once
#include <stdint.h>

#include "CounterTypes.h"
#include "Gesture.h"

// Pure button -> screen/settings state machine (global namespace, like Gesture).
// No Arduino or hardware includes; time is passed in and wrap-safe.
//
// Wiring contract: after begin() and after EVERY onGesture()/tick()/wake()
// call, main must refresh the gesture engines:
//   for i in {0,1}: gesture[i].setAssigned(ui.wantsDouble(i), ui.wantsHold(i));
// so singles are never delayed by a double window outside SETTINGS and a long
// press acts as a plain press on the night prompt.
//
// Buttons: 0 = KEY1 (front), 1 = KEY2 (side).
class UiController {
 public:
  static constexpr uint16_t MANUAL_COUNT_MAX = 9999;

  explicit UiController(const cc::UiLimits& limits) : lim_(limits) {}

  void begin(const cc::Settings& s, bool promptNight, uint32_t nowMs) {
    threshold_ = s.threshold;
    venue_ = s.venue;
    rssiGate_ = s.rssiGate;
    manual_ = 0;
    item_ = cc::SettingsItem::THRESHOLD;
    dimmed_ = false;
    lastPress_ = nowMs;
    now_ = nowMs;
    if (promptNight) {
      screen_ = cc::Screen::NIGHT_PROMPT;
      promptStart_ = nowMs;
    } else {
      screen_ = cc::Screen::COUNT;
    }
  }

  cc::UiEvent onGesture(uint8_t button, GestureEvent e, uint16_t currentEstimate,
                        uint32_t nowMs) {
    if (e == GestureEvent::NONE || button > 1) return none();
    now_ = nowMs;
    lastPress_ = nowMs;
    if (dimmed_) {  // wake-press is consumed
      dimmed_ = false;
      return none();
    }
    switch (screen_) {
      case cc::Screen::NIGHT_PROMPT:
        return endPrompt(button == 0 ? cc::UiAction::NEW_NIGHT : cc::UiAction::RESUME_NIGHT);
      case cc::Screen::SETTINGS:
        return button == 0 ? settingsKey1(e, currentEstimate) : settingsKey2(e);
      default:
        return normal(button, e);
    }
  }

  cc::UiEvent tick(uint32_t nowMs) {
    now_ = nowMs;
    if (screen_ == cc::Screen::NIGHT_PROMPT) {
      if (nowMs - promptStart_ >= lim_.promptTimeoutMs) return endPrompt(cc::UiAction::NEW_NIGHT);
      return none();
    }
    if (!dimmed_ && nowMs - lastPress_ >= lim_.dimAfterMs) dimmed_ = true;
    return none();
  }

  // Threshold alert: undim and restart the dim timer.
  void wake(uint32_t nowMs) {
    now_ = nowMs;
    dimmed_ = false;
    lastPress_ = nowMs;
  }

  bool wantsDouble(uint8_t button) const {
    return button == 1 && screen_ == cc::Screen::SETTINGS;
  }
  bool wantsHold(uint8_t button) const {
    return button <= 1 && screen_ != cc::Screen::NIGHT_PROMPT;
  }

  cc::Screen screen() const { return screen_; }
  cc::SettingsItem item() const { return item_; }
  bool dimmed() const { return dimmed_; }
  uint16_t threshold() const { return threshold_; }
  uint8_t venue() const { return venue_; }
  int8_t rssiGate() const { return rssiGate_; }
  uint16_t manualCount() const { return manual_; }

  // Whole seconds left on the night prompt (ceil), as of the last call that
  // carried a timestamp (begin/onGesture/tick/wake). 0 when not prompting.
  uint8_t promptSeconds() const {
    if (screen_ != cc::Screen::NIGHT_PROMPT) return 0;
    uint32_t elapsed = now_ - promptStart_;
    if (elapsed >= lim_.promptTimeoutMs) return 0;
    return static_cast<uint8_t>((lim_.promptTimeoutMs - elapsed + 999) / 1000);
  }

 private:
  static cc::UiEvent none() { return {cc::UiAction::NONE, 0}; }

  cc::UiEvent endPrompt(cc::UiAction a) {
    screen_ = cc::Screen::COUNT;
    lastPress_ = now_;  // dim timer starts fresh
    return {a, 0};
  }

  cc::UiEvent normal(uint8_t button, GestureEvent e) {
    if (button == 0) {
      if (e == GestureEvent::SINGLE) {
        screen_ = screen_ == cc::Screen::COUNT    ? cc::Screen::DETAIL
                  : screen_ == cc::Screen::DETAIL ? cc::Screen::SUMMARY
                                                  : cc::Screen::COUNT;
      } else if (e == GestureEvent::HOLD) {
        screen_ = cc::Screen::SETTINGS;
        item_ = cc::SettingsItem::THRESHOLD;
      }
      return none();
    }
    if (e == GestureEvent::SINGLE) dimmed_ = true;
    if (e == GestureEvent::HOLD) return {cc::UiAction::START_CAPTURE, 0};
    return none();
  }

  cc::UiEvent settingsKey1(GestureEvent e, uint16_t estimate) {
    if (e == GestureEvent::HOLD) {
      screen_ = cc::Screen::COUNT;
    } else if (e == GestureEvent::SINGLE) {
      item_ = static_cast<cc::SettingsItem>((static_cast<uint8_t>(item_) + 1) %
                                            cc::SETTINGS_ITEM_COUNT);
      if (item_ == cc::SettingsItem::MANUAL_COUNT) seedManual(estimate);
    }
    return none();
  }

  void seedManual(uint16_t estimate) {
    uint32_t step = lim_.manualStep ? lim_.manualStep : 1;
    uint32_t v = ((estimate + step / 2) / step) * step;
    manual_ = v > MANUAL_COUNT_MAX ? MANUAL_COUNT_MAX : static_cast<uint16_t>(v);
  }

  cc::UiEvent settingsKey2(GestureEvent e) {
    if (e == GestureEvent::HOLD) {
      if (item_ == cc::SettingsItem::MANUAL_COUNT) return {cc::UiAction::SAVE_MANUAL_COUNT, manual_};
      if (item_ == cc::SettingsItem::END_NIGHT) {
        screen_ = cc::Screen::COUNT;
        return {cc::UiAction::END_NIGHT, 0};
      }
      return none();
    }
    int dir = e == GestureEvent::SINGLE ? 1 : e == GestureEvent::DOUBLE ? -1 : 0;
    if (dir == 0) return none();
    switch (item_) {
      case cc::SettingsItem::THRESHOLD: {
        int32_t v = clampi(int32_t(threshold_) + dir * int32_t(lim_.thresholdStep),
                           lim_.thresholdMin, lim_.thresholdMax);
        if (v == threshold_) return none();
        threshold_ = static_cast<uint16_t>(v);
        return {cc::UiAction::SET_THRESHOLD, v};
      }
      case cc::SettingsItem::VENUE: {
        if (lim_.venueCount <= 1) return none();
        venue_ = static_cast<uint8_t>((venue_ + lim_.venueCount + dir) % lim_.venueCount);
        return {cc::UiAction::SET_VENUE, venue_};
      }
      case cc::SettingsItem::RSSI_GATE: {
        int32_t v = clampi(int32_t(rssiGate_) + dir * int32_t(lim_.gateStep), lim_.gateMin,
                           lim_.gateMax);
        if (v == rssiGate_) return none();
        rssiGate_ = static_cast<int8_t>(v);
        return {cc::UiAction::SET_RSSI_GATE, v};
      }
      case cc::SettingsItem::MANUAL_COUNT:
        manual_ = static_cast<uint16_t>(
            clampi(int32_t(manual_) + dir * int32_t(lim_.manualStep), 0, MANUAL_COUNT_MAX));
        return none();
      default:
        return none();
    }
  }

  static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

  cc::UiLimits lim_;
  cc::Screen screen_ = cc::Screen::COUNT;
  cc::SettingsItem item_ = cc::SettingsItem::THRESHOLD;
  bool dimmed_ = false;
  uint16_t threshold_ = 0;
  uint8_t venue_ = 0;
  int8_t rssiGate_ = 0;
  uint16_t manual_ = 0;
  uint32_t lastPress_ = 0;
  uint32_t promptStart_ = 0;
  uint32_t now_ = 0;
};
