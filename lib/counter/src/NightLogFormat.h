#pragma once
// The only place that knows the night-log CSV layout: title, method, column
// header, minute rows, event lines, and the row parser / summary used to read
// them back. Pure and header-only.
//
// Privacy by construction: no function here takes a device id, an address or an
// Observation. The log can only ever hold the aggregates in MinuteRecord and
// NightMethod, so there is nothing identifying to leak.
//
// Every formatter writes one '\n'-terminated, NUL-terminated line into the
// caller's buffer and returns its length (excluding the NUL). If the whole line
// does not fit it writes "" and returns 0: a truncated log line is worse than
// none. Null buffers and zero capacity are safe.
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CounterTypes.h"

namespace cc {
namespace nightlog_detail {

constexpr const char* CLASS_NAMES[CLASS_COUNT] = {"phone",    "wearable", "audio", "tracker",
                                                  "computer", "beacon",   "other"};
constexpr uint8_t COLUMN_COUNT = 21;

// Bounds-checked printf appender. Any overflow sticks; finish() then blanks the
// buffer. The final '\n' goes through the same check, so the NUL always fits.
struct LineWriter {
  char* buf;
  size_t cap;
  size_t len = 0;
  bool bad;

  LineWriter(char* b, size_t c) : buf(b), cap(c), bad(b == nullptr || c == 0) {}

  void add(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    if (bad) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + len, cap - len, fmt, ap);
    va_end(ap);
    if (n < 0 || static_cast<size_t>(n) >= cap - len) {
      bad = true;
      return;
    }
    len += static_cast<size_t>(n);
  }

  size_t finish() {
    add("\n");
    if (bad) {
      if (buf != nullptr && cap > 0) buf[0] = 0;
      return 0;
    }
    return len;
  }
};

inline float finiteOrZero(float v) { return isfinite(v) ? v : 0.0f; }

// Free text in a CSV/quoted field: quotes, backslashes, control characters and
// commas become '_'.
inline void addVenue(LineWriter& w, const char* venue) {
  w.add("venue=\"");
  for (const char* p = venue; p != nullptr && *p; ++p) {
    unsigned char c = static_cast<unsigned char>(*p);
    char out = (c == '"' || c == '\\' || c == ',' || c < 0x20 || c == 0x7F) ? '_' : *p;
    w.add("%c", out);
  }
  w.add("\"");
}

inline void addMethodFields(LineWriter& w, const NightMethod& m) {
  addVenue(w, m.venue);
  w.add(" ratio_ble=%.3f ratio_wifi=%.3f wifi_fitted=%u blend=%.2f window_s=%lu rssi_gate=%d",
        finiteOrZero(m.ratios.ble), finiteOrZero(m.ratios.wifi), m.ratios.wifiFitted ? 1u : 0u,
        finiteOrZero(m.blend), static_cast<unsigned long>(m.windowS), static_cast<int>(m.rssiGate));
  for (uint8_t i = 0; i < CLASS_COUNT; ++i) w.add(" w_%s=%.2f", CLASS_NAMES[i], finiteOrZero(m.weights.w[i]));
  w.add(" baseline_ble=%.2f baseline_wifi=%.2f baseline_fixed=%u baseline_valid=%u",
        finiteOrZero(m.baseline.ble), finiteOrZero(m.baseline.wifi),
        static_cast<unsigned>(m.baseline.fixedCount), m.baseline.valid ? 1u : 0u);
}

// Parses one field at `p`, advancing past its delimiter. `last` fields must end
// the line (NUL, "\n" or "\r\n").
struct FieldReader {
  const char* p;
  uint8_t index = 0;

  bool delimiter(const char* end) {
    ++index;
    if (index < COLUMN_COUNT) {
      if (*end != ',') return false;
      p = end + 1;
      return true;
    }
    if (end[0] == '\r' && end[1] == '\n') end += 2;
    else if (*end == '\n') end += 1;
    p = end;
    return *p == 0;
  }

  bool uns(uint32_t maxValue, uint32_t& out) {
    if (*p < '0' || *p > '9') return false;  // no sign, no space, no letters
    char* end = nullptr;
    unsigned long long v = strtoull(p, &end, 10);
    if (end - p > 10 || v > maxValue) return false;
    out = static_cast<uint32_t>(v);
    return delimiter(end);
  }

  bool real(float& out) {
    if (!((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')) return false;
    char* end = nullptr;
    float v = strtof(p, &end);
    if (end == p || !isfinite(v)) return false;
    out = v;
    return delimiter(end);
  }

  bool u16(uint16_t& out) {
    uint32_t v;
    if (!uns(0xFFFFu, v)) return false;
    out = static_cast<uint16_t>(v);
    return true;
  }
};

}  // namespace nightlog_detail

inline size_t formatTitle(char* buf, size_t cap, uint32_t nightId, const char* firmwareVersion) {
  nightlog_detail::LineWriter w(buf, cap);
  w.add("# crowd-counter night=%lu firmware=%s", static_cast<unsigned long>(nightId),
        firmwareVersion != nullptr ? firmwareVersion : "");
  return w.finish();
}

inline size_t formatMethod(char* buf, size_t cap, const NightMethod& m) {
  nightlog_detail::LineWriter w(buf, cap);
  w.add("# method ");
  nightlog_detail::addMethodFields(w, m);
  return w.finish();
}

inline size_t formatColumns(char* buf, size_t cap) {
  nightlog_detail::LineWriter w(buf, cap);
  w.add("minute,epoch");
  for (uint8_t i = 0; i < CLASS_COUNT; ++i) w.add(",ble_%s", nightlog_detail::CLASS_NAMES[i]);
  w.add(",wifi,wifi_rotating,background,allowlisted,ble_signal,wifi_signal,estimate,smoothed,"
        "peak,threshold,battery_mv,flags");
  return w.finish();
}

inline size_t formatRow(char* buf, size_t cap, const MinuteRecord& r) {
  nightlog_detail::LineWriter w(buf, cap);
  w.add("%u,%lu", static_cast<unsigned>(r.minute), static_cast<unsigned long>(r.epoch));
  for (uint8_t i = 0; i < CLASS_COUNT; ++i) w.add(",%u", static_cast<unsigned>(r.ble[i]));
  w.add(",%u,%u,%u,%u,%.2f,%.2f,%u,%u,%u,%u,%u,%u", static_cast<unsigned>(r.wifi),
        static_cast<unsigned>(r.wifiRotating), static_cast<unsigned>(r.background),
        static_cast<unsigned>(r.allowlisted), nightlog_detail::finiteOrZero(r.bleSignal),
        nightlog_detail::finiteOrZero(r.wifiSignal), static_cast<unsigned>(r.estimate),
        static_cast<unsigned>(r.smoothed), static_cast<unsigned>(r.peak),
        static_cast<unsigned>(r.threshold), static_cast<unsigned>(r.batteryMv),
        static_cast<unsigned>(r.flags));
  return w.finish();
}

inline size_t formatMethodEvent(char* buf, size_t cap, uint16_t minute, const NightMethod& m) {
  nightlog_detail::LineWriter w(buf, cap);
  w.add("# event minute=%u method ", static_cast<unsigned>(minute));
  nightlog_detail::addMethodFields(w, m);
  return w.finish();
}

inline size_t formatResumeEvent(char* buf, size_t cap, uint16_t minute) {
  nightlog_detail::LineWriter w(buf, cap);
  w.add("# event minute=%u resume", static_cast<unsigned>(minute));
  return w.finish();
}

inline size_t formatManualCountEvent(char* buf, size_t cap, uint16_t minute, uint16_t truth,
                                     float bleSignal, float wifiSignal) {
  nightlog_detail::LineWriter w(buf, cap);
  w.add("# event minute=%u manual_count truth=%u ble_signal=%.2f wifi_signal=%.2f",
        static_cast<unsigned>(minute), static_cast<unsigned>(truth),
        nightlog_detail::finiteOrZero(bleSignal), nightlog_detail::finiteOrZero(wifiSignal));
  return w.finish();
}

// Reads one minute row back. False (and `out` untouched) for anything that is
// not exactly a well-formed row: comments, the column header, events, wrong
// field count, non-numeric, negative or out-of-range values.
inline bool parseRow(const char* line, MinuteRecord& out) {
  if (line == nullptr) return false;
  nightlog_detail::FieldReader f{line};
  MinuteRecord r;
  memset(&r, 0, sizeof r);
  uint32_t v = 0;
  bool ok = f.u16(r.minute) && f.uns(0xFFFFFFFFu, r.epoch);
  for (uint8_t i = 0; ok && i < CLASS_COUNT; ++i) ok = f.u16(r.ble[i]);
  ok = ok && f.u16(r.wifi) && f.u16(r.wifiRotating) && f.u16(r.background) &&
       f.u16(r.allowlisted) && f.real(r.bleSignal) && f.real(r.wifiSignal) && f.u16(r.estimate) &&
       f.u16(r.smoothed) && f.u16(r.peak) && f.u16(r.threshold) && f.u16(r.batteryMv) &&
       f.uns(0xFFu, v);
  if (!ok) return false;
  r.flags = static_cast<uint8_t>(v);
  out = r;
  return true;
}

// Folds minute rows into the end-of-night summary.
struct NightSummaryBuilder {
  void add(const MinuteRecord& r) {
    if (s_.minutes < 0xFFFFu) ++s_.minutes;
    if (r.peak > s_.peak) s_.peak = r.peak;
    if (r.threshold > 0 && r.smoothed >= r.threshold && !(r.flags & rowflag::WARMUP)) {
      if (s_.minutesAbove < 0xFFFFu) ++s_.minutesAbove;
    }
    s_.threshold = r.threshold;
    if (r.epoch != 0) {
      if (s_.startEpoch == 0) s_.startEpoch = r.epoch;
      s_.endEpoch = r.epoch;
    }
  }
  NightSummary result() const { return s_; }

 private:
  NightSummary s_ = {0, 0, 0, 0, 0, 0};
};

}  // namespace cc
