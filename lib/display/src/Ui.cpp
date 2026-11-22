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
constexpr int LOGERR_Y = 15;              // "LOG ERR" mark, y 15..23 (clear of top row and digits)
constexpr int LIST_NOTICE_Y = 206;        // confirm notice under the 6-row DETAIL/SUMMARY lists (end y 198)
// ---- Settings footer / prompt layout ------------------------------------------
constexpr int FOOT_Y = 190, FOOT_PITCH = 10;  // 4 footer lines: y 190..238
constexpr int CHOICE_X = 4, CHOICE_W = W - 8, CHOICE_H = 46;
constexpr int CHOICE1_Y = 34, CHOICE2_Y = 88;
constexpr int COUNTDOWN_Y = 150, METER_Y = 174, METER_H = 10, PROMPT_HINT_Y = 192;

// Two short lines telling the user what a second KEY2 hold will do (<= 90 px wide).
void confirmNotice(M5Canvas& c, int y) {
  labelCenter(c, CENTER_X, y, "HOLD K2 AGAIN", ACCENT);
  labelCenter(c, CENTER_X, y + 10, "TO CAPTURE", ACCENT);
}

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

  if (m.logError) label(c, MARGIN, LOGERR_Y, "LOG ERR", ALERT);

  // flash is a COUNT-only effect; the other screens ignore it by design.
  if (!m.flash) {
    if (m.confirmCapture) {
      confirmNotice(c, NOTICE_Y);
    } else if (m.capturing) {
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
  header(c, "DETAIL");
  int y = ROWS_Y;
  rowNum(c, y, "BLE", m.bleCount), y += ROW_H;
  rowNum(c, y, "WIFI", m.wifiCount), y += ROW_H;
  rowf(c, y, "RATIO", "%u.%02u", unsigned(m.ratioX100 / 100), unsigned(m.ratioX100 % 100)), y += ROW_H;
  rowNum(c, y, "BKGND", m.background), y += ROW_H;
  rowNum(c, y, "BASE", m.baseline), y += ROW_H;
  rowNum(c, y, "GATE", m.rssiGate);
  if (m.confirmCapture) confirmNotice(c, LIST_NOTICE_Y);
}

void Ui::drawSummary(const cc::UiModel& m) {
  M5Canvas& c = canvas_;
  char name[16], buf[24];
  header(c, "NIGHT", m.startEpoch ? "UTC" : nullptr);
  int y = ROWS_Y;
  rowNum(c, y, "PEAK", m.peak), y += ROW_H;
  snprintf(name, sizeof name, ">=%u", unsigned(m.threshold));
  rowf(c, y, name, "%um", unsigned(m.minutesAbove)), y += ROW_H;
  if (m.startEpoch) {
    const uint32_t s = m.startEpoch % 86400u;
    rowf(c, y, "START", "%02u:%02u", unsigned(s / 3600), unsigned(s / 60 % 60)), y += ROW_H;
  }
  rowf(c, y, "ELAPSED", "%u:%02u", unsigned(m.nightMinutes / 60), unsigned(m.nightMinutes % 60)),
      y += ROW_H;
  rowNum(c, y, "X", m.threshold), y += ROW_H;
  upper(buf, sizeof buf, venueName(m.venue));
  row(c, y, "VENUE", buf);
  if (m.confirmCapture) confirmNotice(c, LIST_NOTICE_Y);
}

void Ui::drawSettings(const cc::UiModel& m) {
  M5Canvas& c = canvas_;
  char buf[24];
  header(c, "SETTINGS");
  for (uint8_t i = 0; i < cc::SETTINGS_ITEM_COUNT; ++i) {
    const int y = ROWS_Y + i * ROW_H;
    const bool hl = uint8_t(m.item) == i;
    switch (cc::SettingsItem(i)) {
      case cc::SettingsItem::THRESHOLD: rowNum(c, y, ITEM_NAMES[i], m.threshold, hl, 1); break;
      case cc::SettingsItem::VENUE:
        upper(buf, sizeof buf, venueName(m.venue));
        row(c, y, ITEM_NAMES[i], buf, hl, 1);
        break;
      case cc::SettingsItem::RSSI_GATE: rowNum(c, y, ITEM_NAMES[i], m.rssiGate, hl, 1); break;
      case cc::SettingsItem::MANUAL_COUNT: rowNum(c, y, ITEM_NAMES[i], m.manualCount, hl, 1); break;
      case cc::SettingsItem::END_NIGHT: row(c, y, ITEM_NAMES[i], "HOLD", hl, 1); break;
    }
  }
  int y = FOOT_Y;
  label(c, MARGIN, y, "K1 NEXT", DIM), y += FOOT_PITCH;
  label(c, MARGIN, y, "K1 HOLD EXIT", DIM), y += FOOT_PITCH;
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
  meter(c, MARGIN, METER_Y, W - 2 * MARGIN, METER_H, m.promptSeconds,
        m.promptTotalSeconds, ACCENT);  // total 0: meter draws an empty frame
  labelCenter(c, CENTER_X, PROMPT_HINT_Y, "NO PRESS = NEW", DIM);
}

static_assert(sizeof(cc::UiModel) == 48, "update sameModel when UiModel changes");

bool sameModel(const cc::UiModel& a, const cc::UiModel& b) {
  return a.screen == b.screen && a.item == b.item && a.dimmed == b.dimmed &&
         a.estimate == b.estimate && a.threshold == b.threshold && a.peak == b.peak &&
         a.state == b.state && a.warmingUp == b.warmingUp && a.capturing == b.capturing &&
         a.confirmCapture == b.confirmCapture && a.logError == b.logError &&
         a.setupStale == b.setupStale && a.flash == b.flash && a.pulse == b.pulse &&
         a.batteryPct == b.batteryPct && a.charging == b.charging && a.bleCount == b.bleCount &&
         a.wifiCount == b.wifiCount && a.background == b.background && a.baseline == b.baseline &&
         a.ratioX100 == b.ratioX100 && a.rssiGate == b.rssiGate &&
         a.minutesAbove == b.minutesAbove && a.nightMinutes == b.nightMinutes &&
         a.startEpoch == b.startEpoch && a.venue == b.venue && a.manualCount == b.manualCount &&
         a.promptSeconds == b.promptSeconds &&
         a.promptTotalSeconds == b.promptTotalSeconds;
}
