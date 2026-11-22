// USB serial commands. Output is aggregates only: nothing here can print a
// device id or address, because nothing here reads the device table's entries.
// Wire formats for `export` and `shot` are documented in scripts/serial_link.py.
#include <Arduino.h>

#include "App.h"
#include "Board.h"
#include "CliParse.h"
#include "RtcState.h"

namespace {
cc::LineAccumulator<64> input;

// The night a command refers to: its argument, else the current night if it has
// a file yet, else the newest file. 0 when there is none.
uint32_t resolveNight(const cc::CliRequest& r) {
  if (r.hasArg) return app.nights.exists(r.arg) ? r.arg : 0;
  if (app.nightActive && app.fileCreated) return app.night.nightId;
  return app.nights.newestId();
}

void printSummary(uint32_t id) {
  const cc::NightSummary s = night::summarize(id);
  Serial.printf("night=%lu minutes=%u peak=%u minutes_above=%u threshold=%u start_epoch=%lu end_epoch=%lu\n",
                static_cast<unsigned long>(id), s.minutes, s.peak, s.minutesAbove, s.threshold,
                static_cast<unsigned long>(s.startEpoch), static_cast<unsigned long>(s.endEpoch));
}

void status(uint32_t nowS) {
  const cc::RadioStats rs = app.radio.stats();
  uint16_t ble = 0;
  for (uint8_t i = 0; i < cc::CLASS_COUNT; ++i) ble += app.counts.ble[i];
  Serial.printf("%s %s uptime_s=%lu reset=%s epoch=%lu\n", AppConfig::PROJECT_NAME, FIRMWARE_VERSION,
                static_cast<unsigned long>(nowS), cc::rtc::resetReasonName(),
                static_cast<unsigned long>(epochNow(nowS)));
  Serial.printf("night=%lu active=%d minute=%lu file=%d warming_up=%d setup_stale=%d\n",
                static_cast<unsigned long>(app.night.nightId), app.nightActive,
                static_cast<unsigned long>(app.nightActive ? night::elapsedS(nowS) / 60 : 0),
                app.fileCreated, warmingUp(nowS), app.setupStale);
  Serial.printf("estimate=%u smoothed=%u peak=%u minutes_above=%u threshold=%u venue=\"%s\"\n",
                app.snap.instant, app.snap.smoothed, app.snap.peak, app.snap.minutesAbove,
                app.settings.threshold, config::venueName(app.settings.venue));
  Serial.printf("ble=%u ble_weighted=%.2f wifi=%u wifi_rotating=%u seen_ble=%u seen_wifi=%u below_gate=%u "
                "allowlisted=%u background=%u\n",
                ble, app.counts.bleWeighted, app.counts.wifi, app.counts.wifiRotating,
                app.counts.bleSeen, app.counts.wifiSeen, app.counts.belowGate, app.counts.allowlisted,
                app.counts.background);
  Serial.printf("ratio_ble=%.3f ratio_wifi=%.3f wifi_fitted=%d cal_pairs=%u gate=%d baseline_ble=%.2f "
                "baseline_wifi=%.2f baseline_fixed=%u\n",
                app.ratios.ble, app.ratios.wifi, app.ratios.wifiFitted, app.cal.count,
                app.settings.rssiGate, app.night.baseline.ble, app.night.baseline.wifi,
                app.night.baseline.fixedCount);
  Serial.printf("radio ble_ready=%d wifi_ready=%d adverts=%lu mgmt_frames=%lu probes=%lu/%lu/%lu drops=%lu\n",
                app.radio.bleReady(), app.radio.wifiReady(), static_cast<unsigned long>(rs.bleAdverts),
                static_cast<unsigned long>(rs.wifiFrames), static_cast<unsigned long>(rs.wifiProbes[0]),
                static_cast<unsigned long>(rs.wifiProbes[1]), static_cast<unsigned long>(rs.wifiProbes[2]),
                static_cast<unsigned long>(rs.queueDrops));
  Serial.printf("table=%lu evictions=%lu battery_mv=%d battery_pct=%d charging=%d heap=%lu psram=%lu "
                "fs_used=%u fs_total=%u\n",
                static_cast<unsigned long>(app.table->size()),
                static_cast<unsigned long>(app.table->evictions()), board::batteryMv(),
                board::batteryPercent(), board::charging(), static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getFreePsram()), static_cast<unsigned>(app.nights.usedBytes()),
                static_cast<unsigned>(app.nights.totalBytes()));
}

void exportNight(uint32_t id) {
  if (id == 0) {
    Serial.println("ERR no such night");
    return;
  }
  Serial.printf("BEGIN night=%lu\n", static_cast<unsigned long>(id));
  app.nights.forEachLine(id, [](const char* line) {
    Serial.println(line);
    return true;
  });
  Serial.println("END");
}

void shot() {
  M5Canvas& c = app.ui.canvas();
  const int w = c.width(), h = c.height();
  if (w <= 0 || h <= 0) {
    Serial.println("ERR no sprite");
    return;
  }
  Serial.printf("SHOT %d %d RGB565BE\n", w, h);
  static char row[4 * 135 + 2];
  for (int y = 0; y < h; ++y) {
    int n = 0;
    for (int x = 0; x < w && n + 5 < int(sizeof row); ++x)
      n += snprintf(row + n, sizeof row - n, "%04X", unsigned(c.readPixel(x, y)) & 0xFFFF);
    Serial.println(row);
  }
  Serial.println("END");
}

void setTime(uint32_t epoch, uint32_t nowS) {
  const bool rtc = board::setRtcEpoch(epoch);
  app.epochOffset = epoch - nowS;
  if (app.nightActive && app.night.startEpoch == 0) {
    app.night.startEpoch = epoch - night::elapsedS(nowS);
    app.meta.startEpoch = app.night.startEpoch;
    app.store.saveNightMeta(app.meta);
  }
  Serial.printf("time set, rtc=%d\n", rtc);
}

void run(const cc::CliRequest& r, uint32_t nowS) {
  switch (r.command) {
    case cc::CliCommand::HELP:
      Serial.println("commands: help | status | nights | export [n] | summary [n] | time <epoch> | shot");
      break;
    case cc::CliCommand::STATUS: status(nowS); break;
    case cc::CliCommand::NIGHTS: {
      uint32_t ids[config::NIGHTS_KEPT + 4];
      const size_t n = app.nights.list(ids, sizeof ids / sizeof ids[0]);
      Serial.printf("nights=%u\n", static_cast<unsigned>(n));
      for (size_t i = 0; i < n; ++i) printSummary(ids[i]);
      break;
    }
    case cc::CliCommand::EXPORT: exportNight(resolveNight(r)); break;
    case cc::CliCommand::SUMMARY: {
      const uint32_t id = resolveNight(r);
      if (id) printSummary(id);
      else Serial.println("ERR no such night");
      break;
    }
    case cc::CliCommand::TIME: setTime(r.arg, nowS); break;
    case cc::CliCommand::SHOT: shot(); break;
    case cc::CliCommand::UNKNOWN: Serial.println("ERR unknown command; try help"); break;
    case cc::CliCommand::EMPTY: break;
  }
}
}  // namespace

namespace cli {
void poll(uint32_t nowS) {
  while (Serial.available() > 0) {
    if (input.feed(char(Serial.read()))) run(cc::parseCli(input.line()), nowS);
  }
}
}  // namespace cli
