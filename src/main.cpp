// Wiring only: radio -> device table -> census -> estimator -> UI / night log.
#include <Arduino.h>
#include <esp_timer.h>

#include "App.h"
#include "Board.h"
#include "Calibration.h"
#include "Census.h"
#include "Every.h"
#include "Log.h"
#include "RtcState.h"

App app;

namespace {
Every recompute(uint32_t(AppConfig::RECOMPUTE_S) * 1000);
Every second(1000);
Every batteryPoll(5000);
const char* venueNames[AppConfig::VENUE_COUNT];
cc::UiModel shown = {};
bool drawn = false, wasDimmed = false, charging = false;
int8_t batteryPct = -1;
uint8_t pulse = 0;
uint32_t lastRadioTotal = 0, flashUntilMs = 0, captureNoteUntilMs = 0;

// Every sound follows alert.chime, so `chime: false` makes the device silent.
void tone(uint16_t hz, uint16_t ms) {
  if (AppConfig::ALERT_CHIME) board::tone(hz, ms, AppConfig::ALERT_VOLUME);
}
void beep() { tone(config::BEEP_HZ, config::BEEP_MS); }

cc::RawCounts census(uint32_t nowS) {
  return cc::takeCensus(*app.table, app.night.allowlist, config::census(app.settings.rssiGate), nowS);
}

// Stored settings can predate a config.yaml change: pull them back in range.
cc::Settings sanitized(cc::Settings s) {
  const cc::UiLimits l = config::uiLimits();
  if (s.venue >= l.venueCount) s.venue = 0;
  if (s.threshold < l.thresholdMin || s.threshold > l.thresholdMax) s.threshold = AppConfig::THRESHOLD_DEFAULT;
  if (s.rssiGate < l.gateMin || s.rssiGate > l.gateMax) s.rssiGate = AppConfig::RSSI_GATE_DBM;
  return s;
}

void loadVenue() {
  app.cal = {};
  app.store.loadCal(app.settings.venue, app.cal);
  app.ratios = cc::fitRatios(app.cal, config::defaultRatios(app.settings.venue));
}

void capture(uint32_t nowS, uint32_t nowMs) {
  cc::captureSetup(*app.table, config::census(app.settings.rssiGate), nowS, app.night.allowlist,
                   app.night.baseline);
  app.meta.baseline = app.night.baseline;
  app.store.saveNightMeta(app.meta);
  app.setupStale = app.capturePending = false;
  captureNoteUntilMs = nowMs + config::CAPTURE_NOTE_MS;
  // The method just changed: recount under it, and keep the mixed samples out
  // of peak, time-above and the alert while the smoothing refills.
  app.counts = census(nowS);
  const uint32_t settled = nowS + AppConfig::SMOOTHING_S;
  if (settled > app.warmUntilS) app.warmUntilS = settled;
  night::logMethod(nowS);
  beep();
  LOG_I("setup captured: baseline ble=%.1f wifi=%.1f, %u fixed devices allowlisted",
        app.night.baseline.ble, app.night.baseline.wifi, app.night.baseline.fixedCount);
}

void saveManualCount(uint32_t nowS, uint16_t truth) {
  if (!app.nightActive || app.snap.warmingUp || truth == 0) {  // no usable signal yet
    tone(config::ERROR_HZ, config::ERROR_MS);
    LOG_W("manual count rejected: warming up or zero");
    return;
  }
  cc::addPair(app.cal, {app.snap.bleSignal, app.snap.wifiSignal, truth});
  app.store.saveCal(app.settings.venue, app.cal);
  app.ratios = cc::fitRatios(app.cal, config::defaultRatios(app.settings.venue));
  night::logManualCount(nowS, truth);
  night::logMethod(nowS);
  beep();
}

void handle(cc::UiEvent ev, uint32_t nowS, uint32_t nowMs) {
  switch (ev.action) {
    case cc::UiAction::SET_THRESHOLD:
      app.settings.threshold = uint16_t(ev.value);
      app.store.saveSettings(app.settings);
      break;
    case cc::UiAction::SET_VENUE:
      app.settings.venue = uint8_t(ev.value);
      app.store.saveSettings(app.settings);
      loadVenue();
      night::logMethod(nowS);
      break;
    case cc::UiAction::SET_RSSI_GATE:
      app.settings.rssiGate = int8_t(ev.value);
      app.store.saveSettings(app.settings);
      if (app.night.baseline.valid) app.setupStale = true;  // baseline was measured under the old gate
      night::logMethod(nowS);
      break;
    case cc::UiAction::SAVE_MANUAL_COUNT: saveManualCount(nowS, uint16_t(ev.value)); break;
    case cc::UiAction::END_NIGHT:
      night::end(nowS);
      night::startNew(nowS, true);
      break;
    case cc::UiAction::START_CAPTURE:
      if (!app.nightActive) break;
      if (warmingUp(nowS)) app.capturePending = true;  // finishes once a full window exists
      else capture(nowS, nowMs);
      break;
    case cc::UiAction::NEW_NIGHT: night::startNew(nowS, false); break;
    case cc::UiAction::RESUME_NIGHT: night::resumeCold(nowS); break;
    case cc::UiAction::NONE: break;
  }
}

void estimate(uint32_t nowS, uint32_t nowMs) {
  app.table->expire(nowS, AppConfig::WINDOW_S);
  app.counts = census(nowS);
  if (!app.nightActive) return;
  if (night::elapsedS(nowS) >= config::NIGHT_MAX_S) {  // never keep one salt longer than this
    LOG_I("night reached its maximum length: rotating");
    night::end(nowS);
    night::startNew(nowS, true);
    return;
  }
  const bool warm = warmingUp(nowS);
  if (app.capturePending && !warm) capture(nowS, nowMs);
  app.snap = app.estimator.update(app.counts, app.night.baseline, app.ratios,
                                  app.settings.threshold, warm);
  if (app.snap.alertNow) {
    if (AppConfig::ALERT_FLASH) flashUntilMs = nowMs + config::ALERT_FLASH_MS;
    tone(config::CHIME_HZ, config::CHIME_MS);
    app.controller.showCount(nowMs);  // the flash is drawn on the count screen
    night::logAlert(nowS);
    LOG_I("threshold %u crossed", app.settings.threshold);
  }
  night::tick(nowS);
}

cc::UiModel buildModel(uint32_t nowS, uint32_t nowMs) {
  cc::UiModel m = {};
  const UiController& c = app.controller;
  m.screen = c.screen();
  m.item = c.item();
  m.dimmed = c.dimmed();
  m.estimate = app.snap.smoothed;
  m.threshold = c.threshold();
  m.peak = app.snap.peak;
  m.state = app.snap.state;
  m.warmingUp = warmingUp(nowS);
  m.capturing = app.capturePending || nowMs < captureNoteUntilMs;
  m.confirmCapture = c.confirmingCapture();
  m.logError = app.logError;
  m.setupStale = app.setupStale;
  m.flash = nowMs < flashUntilMs && (nowMs / 250) % 2 == 0;
  m.pulse = pulse;
  m.batteryPct = batteryPct;
  m.charging = charging;
  for (uint8_t i = 0; i < cc::CLASS_COUNT; ++i) m.bleCount += app.counts.ble[i];
  m.wifiCount = app.counts.wifi;
  m.background = app.counts.background;
  m.baseline = uint16_t(app.night.baseline.ble + app.night.baseline.wifi + 0.5f);
  m.ratioX100 = uint16_t(app.ratios.ble * 100 + 0.5f);
  m.rssiGate = c.rssiGate();
  m.minutesAbove = app.snap.minutesAbove;
  m.nightMinutes = app.nightActive ? uint16_t(night::elapsedS(nowS) / 60) : 0;
  m.startEpoch = app.night.startEpoch;
  m.venue = c.venue();
  m.manualCount = c.manualCount();
  m.promptSeconds = c.promptSeconds();
  m.promptTotalSeconds = uint8_t(config::PROMPT_TIMEOUT_MS / 1000);
  return m;
}

void everySecond() {
  // The pulse only advances while the radios are actually delivering.
  const cc::RadioStats s = app.radio.stats();
  const uint32_t total = s.bleAdverts + s.wifiFrames;
  if (total != lastRadioTotal) pulse = (pulse + 1) & 3;
  lastRadioTotal = total;
}
}  // namespace

uint32_t nowSeconds() { return uint32_t(esp_timer_get_time() / 1000000); }

uint32_t epochNow(uint32_t nowS) {
  const uint32_t rtc = board::rtcEpoch();
  return rtc ? rtc : (app.epochOffset ? app.epochOffset + nowS : 0);
}

bool warmingUp(uint32_t nowS) { return nowS < app.warmUntilS; }

void setup() {
  Serial.begin(115200);  // never wait for a USB serial host
  board::begin(AppConfig::DISPLAY_BRIGHTNESS);
  LOG_I("%s %s on %s, reset: %s", AppConfig::PROJECT_NAME, FIRMWARE_VERSION, BOARD_LABEL,
        cc::rtc::resetReasonName());

  for (uint8_t i = 0; i < AppConfig::VENUE_COUNT; ++i) venueNames[i] = AppConfig::VENUES[i].name;
  if (!app.ui.begin(board::display(), venueNames, AppConfig::VENUE_COUNT)) LOG_E("no sprite memory");

  if (!app.store.begin()) LOG_E("settings store unavailable: using defaults");
  app.settings = sanitized(app.store.loadSettings(config::defaultSettings()));
  app.meta = app.store.loadNightMeta();
  loadVenue();
  if (!app.nights.begin()) {
    LOG_E("night store unavailable: nothing will be logged");
    app.logError = true;
  }

  auto* slots = static_cast<cc::DeviceEntry*>(ps_calloc(config::TABLE_SLOTS, sizeof(cc::DeviceEntry)));
  uint32_t slotCount = config::TABLE_SLOTS, capacity = AppConfig::TABLE_CAPACITY;
  if (!slots) {  // keep counting, badly, rather than crash-loop
    LOG_E("no PSRAM for the device table: falling back to %lu slots",
          static_cast<unsigned long>(config::FALLBACK_SLOTS));
    static cc::DeviceEntry fallback[config::FALLBACK_SLOTS];
    slots = fallback;
    slotCount = config::FALLBACK_SLOTS;
    capacity = config::FALLBACK_SLOTS * 5 / 8;
  }
  static cc::DeviceTable table(slots, slotCount, capacity);
  app.table = &table;

  if (!app.radio.begin(config::radio())) LOG_E("radio failed to start");
  const uint32_t nowS = nowSeconds();
  app.warmUntilS = nowS + AppConfig::WINDOW_S;  // the table is empty after every boot

  bool prompt = false;
  if (!night::resumeWarm(nowS)) {
    // Scanning starts now, whatever the prompt decides: the salt is per boot.
    cc::rtc::freshSalt(app.night.salt);
    app.radio.setSalt(app.night.salt);
    prompt = app.meta.openNightId != 0 && app.nights.exists(app.meta.openNightId);
    if (!prompt) night::startNew(nowS, false);
  }
  app.controller.begin(app.settings, prompt, millis());
  // A button held through boot must be released before it can mean anything.
  for (uint8_t i = 0; i < board::BUTTON_COUNT; ++i) app.keys[i].reset(board::isPressed(i), millis());
}

void loop() {
  board::update();
  const uint32_t nowMs = millis();
  const uint32_t nowS = nowSeconds();

  cc::Observation o;
  for (int i = 0; i < config::DRAIN_PER_LOOP && app.radio.poll(o); ++i) app.table->observe(o, nowS);

  const cc::Screen before = app.controller.screen();
  for (uint8_t i = 0; i < board::BUTTON_COUNT; ++i) {
    const GestureEvent e = app.keys[i].update(board::isPressed(i), nowMs);
    if (e != GestureEvent::NONE)
      handle(app.controller.onGesture(i, e, app.snap.smoothed, nowMs), nowS, nowMs);
  }
  handle(app.controller.tick(nowMs), nowS, nowMs);
  // The buttons mean something else on a new screen: a press that is still
  // down (e.g. when the prompt times out) must not carry over.
  const bool screenChanged = app.controller.screen() != before;
  for (uint8_t i = 0; i < board::BUTTON_COUNT; ++i) {
    if (screenChanged) app.keys[i].cancel();
    app.keys[i].setAssigned(app.controller.wantsDouble(i), app.controller.wantsHold(i));
  }

  if (recompute.due(nowMs)) estimate(nowS, nowMs);
  if (second.due(nowMs)) everySecond();
  if (batteryPoll.due(nowMs)) {
    batteryPct = int8_t(board::batteryPercent());
    charging = board::charging();
  }
  cli::poll(nowS);

  if (app.controller.dimmed() != wasDimmed || !drawn) {
    wasDimmed = app.controller.dimmed();
    board::setBrightness(wasDimmed ? AppConfig::DIM_BRIGHTNESS : AppConfig::DISPLAY_BRIGHTNESS);
  }
  const cc::UiModel model = buildModel(nowS, nowMs);
  if (!drawn || !sameModel(model, shown)) {
    app.ui.render(model);
    shown = model;
    drawn = true;
  }
  delay(1);
}
