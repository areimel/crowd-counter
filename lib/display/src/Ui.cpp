#include "Ui.h"
#include <stdio.h>
#include "Widgets.h"

using namespace Widgets;
using namespace Theme;

namespace {
// ---- Count screen layout ----------------------------------------------------
constexpr int TOP_Y = 4;                  // battery / pulse row, y 4..14
constexpr int PULSE_X = 8, PULSE_Y = 9;
constexpr int BIG_Y = 26;                 // digits y 26..122
constexpr int EST_Y = 126;                // "est." size 2, y 126..142
constexpr int NOTICE_Y = 150;             // capture / stale notice, y 150..166
constexpr int STATUS_Y = 176;             // status bar y 176..204
constexpr int SMALL_Y = 214;              // "X=120 PK151" size 1, y 214..222
// ---- Settings footer / prompt layout ------------------------------------------
constexpr int FOOT_Y = 204, FOOT_PITCH = 10;
constexpr int CHOICE_X = 4, CHOICE_W = W - 8, CHOICE_H = 46;
constexpr int CHOICE1_Y = 34, CHOICE2_Y = 88;
constexpr int COUNTDOWN_Y = 150, METER_Y = 174, METER_H = 10, PROMPT_HINT_Y = 192;
constexpr int PROMPT_TOTAL_S = 15;

constexpr const char* ITEM_NAMES[cc::SETTINGS_ITEM_COUNT] = {"THRESHOLD", "VENUE", "RSSI GATE",
                                                             "MANUAL", "END NIGHT"};
}  // namespace

bool Ui::begin(M5GFX& display, const char* const* venueNames, uint8_t venueCount) {
  display_ = &display;
  venues_ = venueNames;
  venueCount_ = venueCount;
  canvas_.setPsram(true);
  canvas_.setColorDepth(16);
  return canvas_.createSprite(W, H) != nullptr;
}

const char* Ui::venueName(uint8_t index) const {
  return (venues_ && index < venueCount_ && venues_[index]) ? venues_[index] : "?";
}

void Ui::render(const cc::UiModel& m) {
  if (!display_) return;
  canvas_.fillScreen(BG);
  switch (m.screen) {
    case cc::Screen::COUNT: drawCount(m); break;
    case cc::Screen::DETAIL: drawDetail(m); break;
    case cc::Screen::SUMMARY: drawSummary(m); break;
    case cc::Screen::SETTINGS: drawSettings(m); break;
    case cc::Screen::NIGHT_PROMPT: drawPrompt(m); break;
  }
  canvas_.pushSprite(display_, 0, 0);
}

void Ui::drawCount(const cc::UiModel& m) {
  M5Canvas& c = canvas_;
  char buf[24];
  if (m.flash) c.fillScreen(STATUS_GREEN);  // alert: dark digits on green, never white
  const uint16_t digits = m.flash ? BG : (m.warmingUp ? DIM : INK);

  if (!m.flash) {
    pulse(c, PULSE_X, PULSE_Y, m.pulse);
    battery(c, RIGHT, TOP_Y, m.batteryPct, m.charging);
  }
  bigNumber(c, BIG_Y, m.estimate, digits);
  labelRight(c, RIGHT, EST_Y, "est.", m.flash ? BG : DIM, 2);

  if (!m.flash) {
    if (m.capturing) {
      labelCenter(c, CENTER_X, NOTICE_Y, "CAPTURING", ACCENT, 2);
    } else if (m.setupStale) {
      labelCenter(c, CENTER_X, NOTICE_Y, "RE-RUN SETUP", ACCENT);
      labelCenter(c, CENTER_X, NOTICE_Y + 10, "KEY2 HOLD", DIM);
    }
  }

  uint16_t fill = statusColor(m.state), ink = (m.state == cc::ThresholdState::BELOW) ? INK : BG;
  if (m.warmingUp) {
    snprintf(buf, sizeof buf, "WARMING UP");
    fill = STATUS_GREY, ink = INK;
  } else {
    snprintf(buf, sizeof buf, "%s %u", m.state == cc::ThresholdState::ABOVE ? "ABOVE" : "BELOW",
             unsigned(m.threshold));
  }
  if (m.flash) fill = BG, ink = STATUS_GREEN;
  statusBar(c, STATUS_Y, buf, fill, ink);

  snprintf(buf, sizeof buf, "X=%u PK%u", unsigned(m.threshold), unsigned(m.peak));
  labelCenter(c, CENTER_X, SMALL_Y, buf, m.flash ? BG : DIM);
}

void Ui::drawDetail(const cc::UiModel& m) {
  M5Canvas& c = canvas_;
  char buf[16];
  header(c, "DETAIL");
  int y = ROWS_Y;
  snprintf(buf, sizeof buf, "%u", unsigned(m.bleCount));
  row(c, y, "BLE", buf), y += ROW_H;
  snprintf(buf, sizeof buf, "%u", unsigned(m.wifiCount));
  row(c, y, "WIFI", buf), y += ROW_H;
  snprintf(buf, sizeof buf, "%u.%02u", unsigned(m.ratioX100 / 100), unsigned(m.ratioX100 % 100));
  row(c, y, "RATIO", buf), y += ROW_H;
  snprintf(buf, sizeof buf, "%u", unsigned(m.background));
  row(c, y, "BKGND", buf), y += ROW_H;
  snprintf(buf, sizeof buf, "%u", unsigned(m.baseline));
  row(c, y, "BASE", buf), y += ROW_H;
  snprintf(buf, sizeof buf, "%d", int(m.rssiGate));
  row(c, y, "GATE", buf);
}

void Ui::drawSummary(const cc::UiModel& m) {
  M5Canvas& c = canvas_;
  char name[16], buf[24];
  header(c, "NIGHT", m.startEpoch ? "UTC" : nullptr);
  int y = ROWS_Y;
  snprintf(buf, sizeof buf, "%u", unsigned(m.peak));
  row(c, y, "PEAK", buf), y += ROW_H;
  snprintf(name, sizeof name, ">=%u", unsigned(m.threshold));
  snprintf(buf, sizeof buf, "%um", unsigned(m.minutesAbove));
  row(c, y, name, buf), y += ROW_H;
  if (m.startEpoch) {
    const uint32_t s = m.startEpoch % 86400u;
    snprintf(buf, sizeof buf, "%02u:%02u", unsigned(s / 3600), unsigned(s / 60 % 60));
    row(c, y, "START", buf), y += ROW_H;
  }
  snprintf(buf, sizeof buf, "%u:%02u", unsigned(m.nightMinutes / 60), unsigned(m.nightMinutes % 60));
  row(c, y, "NOW", buf), y += ROW_H;
  snprintf(buf, sizeof buf, "%u", unsigned(m.threshold));
  row(c, y, "X", buf), y += ROW_H;
  upper(buf, sizeof buf, venueName(m.venue));
  row(c, y, "VENUE", buf);
}

void Ui::drawSettings(const cc::UiModel& m) {
  M5Canvas& c = canvas_;
  char buf[24];
  header(c, "SETTINGS");
  for (uint8_t i = 0; i < cc::SETTINGS_ITEM_COUNT; ++i) {
    switch (cc::SettingsItem(i)) {
      case cc::SettingsItem::THRESHOLD: snprintf(buf, sizeof buf, "%u", unsigned(m.threshold)); break;
      case cc::SettingsItem::VENUE: upper(buf, sizeof buf, venueName(m.venue)); break;
      case cc::SettingsItem::RSSI_GATE: snprintf(buf, sizeof buf, "%d", int(m.rssiGate)); break;
      case cc::SettingsItem::MANUAL_COUNT: snprintf(buf, sizeof buf, "%u", unsigned(m.manualCount)); break;
      case cc::SettingsItem::END_NIGHT: snprintf(buf, sizeof buf, "HOLD"); break;
    }
    row(c, ROWS_Y + i * ROW_H, ITEM_NAMES[i], buf, uint8_t(m.item) == i, 1);
  }
  int y = FOOT_Y;
  label(c, MARGIN, y, "K1 NEXT", DIM), y += FOOT_PITCH;
  switch (m.item) {
    case cc::SettingsItem::END_NIGHT: label(c, MARGIN, y, "K2 HOLD = END", DIM); break;
    case cc::SettingsItem::MANUAL_COUNT:
      label(c, MARGIN, y, "K2 +  K2x2 -", DIM), y += FOOT_PITCH;
      label(c, MARGIN, y, "K2 HOLD SAVE", DIM);
      break;
    default: label(c, MARGIN, y, "K2 +  K2x2 -", DIM); break;
  }
}

void Ui::drawPrompt(const cc::UiModel& m) {
  M5Canvas& c = canvas_;
  char buf[24];
  header(c, "NIGHT OPEN");
  c.fillRect(CHOICE_X, CHOICE1_Y, CHOICE_W, CHOICE_H, ACCENT);  // default choice
  label(c, MARGIN + 2, CHOICE1_Y + 6, "K1", BG);
  label(c, MARGIN + 2, CHOICE1_Y + 20, "NEW NIGHT", BG, 2);
  c.drawRect(CHOICE_X, CHOICE2_Y, CHOICE_W, CHOICE_H, DIM);
  label(c, MARGIN + 2, CHOICE2_Y + 6, "K2", DIM);
  label(c, MARGIN + 2, CHOICE2_Y + 20, "RESUME", INK, 2);
  snprintf(buf, sizeof buf, "NEW IN %us", unsigned(m.promptSeconds));
  labelCenter(c, CENTER_X, COUNTDOWN_Y, buf, INK, 2);
  meter(c, MARGIN, METER_Y, W - 2 * MARGIN, METER_H, m.promptSeconds, PROMPT_TOTAL_S, ACCENT);
  labelCenter(c, CENTER_X, PROMPT_HINT_Y, "NO PRESS = NEW", DIM);
}

bool sameModel(const cc::UiModel& a, const cc::UiModel& b) {
  return a.screen == b.screen && a.item == b.item && a.dimmed == b.dimmed &&
         a.estimate == b.estimate && a.threshold == b.threshold && a.peak == b.peak &&
         a.state == b.state && a.warmingUp == b.warmingUp && a.capturing == b.capturing &&
         a.setupStale == b.setupStale && a.flash == b.flash && a.pulse == b.pulse &&
         a.batteryPct == b.batteryPct && a.charging == b.charging && a.bleCount == b.bleCount &&
         a.wifiCount == b.wifiCount && a.background == b.background && a.baseline == b.baseline &&
         a.ratioX100 == b.ratioX100 && a.rssiGate == b.rssiGate &&
         a.minutesAbove == b.minutesAbove && a.nightMinutes == b.nightMinutes &&
         a.startEpoch == b.startEpoch && a.venue == b.venue && a.manualCount == b.manualCount &&
         a.promptSeconds == b.promptSeconds;
}
