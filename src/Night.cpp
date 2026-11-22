// Night lifecycle and the per-minute log. A night is open from its start until
// "End night"; the log file is created with its first row, so a power-on that
// lasts under a minute leaves nothing behind.
#include <Arduino.h>

#include "App.h"
#include "Board.h"
#include "Log.h"
#include "NightLogFormat.h"
#include "RtcState.h"

namespace {

cc::NightMethod method() {
  cc::NightMethod m = {};
  m.venue = config::venueName(app.settings.venue);
  m.ratios = app.ratios;
  m.baseline = app.night.baseline;
  m.blend = AppConfig::BLEND_BLE;
  m.windowS = AppConfig::WINDOW_S;
  m.rssiGate = app.settings.rssiGate;
  m.weights = config::census(0).weights;
  return m;
}

// Creates the night file with its title, method and column lines.
bool ensureFile() {
  if (app.fileCreated) return true;
  static char header[1024];
  size_t n = cc::formatTitle(header, sizeof header, app.night.nightId, FIRMWARE_VERSION);
  n += cc::formatMethod(header + n, sizeof header - n, method());
  n += cc::formatColumns(header + n, sizeof header - n);
  if (n > 0 && header[n - 1] == '\n') header[n - 1] = 0;  // the store adds the final newline
  if (!app.nights.create(app.night.nightId, header)) {
    LOG_E("night %lu: cannot create log", static_cast<unsigned long>(app.night.nightId));
    return false;
  }
  app.fileCreated = true;
  app.nights.prune(config::NIGHTS_KEPT, config::MIN_FREE_BYTES, app.night.nightId);
  return true;
}

void appendLine(char* line) {
  size_t n = strlen(line);
  if (n == 0) return;  // the formatter refused: line did not fit
  if (line[n - 1] == '\n') line[n - 1] = 0;
  if (!app.nights.append(app.night.nightId, line)) LOG_E("night log append failed");
}

void writeRow(uint32_t nowS) {
  if (!ensureFile()) return;
  cc::MinuteRecord r = {};
  r.minute = uint16_t(app.lastMinute);
  r.epoch = epochNow(nowS);
  for (uint8_t i = 0; i < cc::CLASS_COUNT; ++i) r.ble[i] = app.counts.ble[i];
  r.wifi = app.counts.wifi;
  r.wifiRotating = app.counts.wifiRotating;
  r.background = app.counts.background;
  r.allowlisted = app.counts.allowlisted;
  r.bleSignal = app.snap.bleSignal;
  r.wifiSignal = app.snap.wifiSignal;
  r.estimate = app.snap.instant;
  r.smoothed = app.snap.smoothed;
  r.peak = app.snap.peak;
  r.threshold = app.settings.threshold;
  const int mv = board::batteryMv();
  r.batteryMv = mv > 0 ? uint16_t(mv) : 0;
  r.flags = app.snap.warmingUp ? cc::rowflag::WARMUP : 0;
  char line[256];
  cc::formatRow(line, sizeof line, r);
  appendLine(line);
}

void mirrorToRtc(uint32_t nowS) {
  app.night.elapsedS = night::elapsedS(nowS);
  app.night.estimator = app.estimator.state();
  cc::rtc::save(app.night);
}

void activate(uint32_t nowS, uint32_t baseElapsedS) {
  app.nightActive = true;
  app.nightBaseS = nowS;
  app.baseElapsedS = baseElapsedS;
  app.lastMinute = baseElapsedS / 60;
  app.capturePending = false;
}

}  // namespace

namespace night {

uint32_t elapsedS(uint32_t nowS) { return app.baseElapsedS + (nowS - app.nightBaseS); }

void startNew(uint32_t nowS, bool freshSalt) {
  uint8_t salt[16];
  if (freshSalt) cc::rtc::freshSalt(salt);
  else memcpy(salt, app.night.salt, sizeof salt);
  app.night = {};
  memcpy(app.night.salt, salt, sizeof salt);
  if (freshSalt) {
    // Ids from the old salt are meaningless now: start the window again.
    app.radio.setSalt(app.night.salt);
    app.table->clear();
    app.warmUntilS = nowS + AppConfig::WINDOW_S;
  }
  app.night.nightId = app.meta.nextNightId++;
  app.night.startEpoch = epochNow(nowS);
  app.meta.openNightId = app.night.nightId;
  app.meta.baseline = {};
  app.meta.startEpoch = app.night.startEpoch;
  app.store.saveNightMeta(app.meta);
  app.estimator.reset();
  app.snap = {};
  app.fileCreated = false;
  app.setupStale = false;
  activate(nowS, 0);
  mirrorToRtc(nowS);
  LOG_I("night %lu started", static_cast<unsigned long>(app.night.nightId));
}

bool resumeWarm(uint32_t nowS) {
  cc::NightState saved;
  if (!cc::rtc::load(saved) || saved.nightId == 0 || saved.nightId != app.meta.openNightId)
    return false;
  app.night = saved;
  app.radio.setSalt(app.night.salt);
  app.estimator.restore(app.night.estimator);
  app.fileCreated = app.nights.exists(app.night.nightId);
  activate(nowS, app.night.elapsedS);
  if (app.fileCreated) {
    char line[64];
    cc::formatResumeEvent(line, sizeof line, uint16_t(app.lastMinute));
    appendLine(line);
  }
  LOG_I("night %lu resumed after %s reset", static_cast<unsigned long>(app.night.nightId),
        cc::rtc::resetReasonName());
  return true;
}

void resumeCold(uint32_t nowS) {
  // The salt generated at boot stays: the table already holds ids made with it.
  const cc::NightSummary s = summarize(app.meta.openNightId);
  app.night.nightId = app.meta.openNightId;
  app.night.startEpoch = app.meta.startEpoch;
  app.night.baseline = app.meta.baseline;
  app.night.allowlist = {};
  app.night.estimator = {s.peak, uint32_t(s.minutesAbove) * 60, s.minutesAbove > 0};
  app.estimator.restore(app.night.estimator);
  app.fileCreated = true;
  app.setupStale = app.night.baseline.valid;  // fixed-address allowlist is gone
  activate(nowS, uint32_t(s.minutes) * 60);
  char line[64];
  cc::formatResumeEvent(line, sizeof line, uint16_t(app.lastMinute));
  appendLine(line);
  mirrorToRtc(nowS);
  LOG_I("night %lu resumed after power loss at minute %lu",
        static_cast<unsigned long>(app.night.nightId), static_cast<unsigned long>(app.lastMinute));
}

void end(uint32_t nowS) {
  LOG_I("night %lu ended at minute %lu", static_cast<unsigned long>(app.night.nightId),
        static_cast<unsigned long>(elapsedS(nowS) / 60));
  app.meta.openNightId = 0;
  app.store.saveNightMeta(app.meta);
  cc::rtc::clear();
  app.nightActive = false;
}

void tick(uint32_t nowS) {
  if (!app.nightActive) return;
  const uint32_t minute = elapsedS(nowS) / 60;
  if (minute > app.lastMinute) {
    app.lastMinute = minute;
    writeRow(nowS);
  }
  mirrorToRtc(nowS);
}

void logMethod(uint32_t nowS) {
  if (!app.nightActive) return;
  mirrorToRtc(nowS);
  if (!app.fileCreated) return;  // the header will carry the current method
  char line[512];
  cc::formatMethodEvent(line, sizeof line, uint16_t(elapsedS(nowS) / 60), method());
  appendLine(line);
}

void logManualCount(uint32_t nowS, uint16_t truth) {
  if (!app.nightActive || !ensureFile()) return;
  char line[128];
  cc::formatManualCountEvent(line, sizeof line, uint16_t(elapsedS(nowS) / 60), truth,
                             app.snap.bleSignal, app.snap.wifiSignal);
  appendLine(line);
}

cc::NightSummary summarize(uint32_t nightId) {
  cc::NightSummaryBuilder b;
  app.nights.forEachLine(nightId, [&b](const char* line) {
    cc::MinuteRecord r;
    if (cc::parseRow(line, r)) b.add(r);
    return true;
  });
  return b.result();
}

}  // namespace night
