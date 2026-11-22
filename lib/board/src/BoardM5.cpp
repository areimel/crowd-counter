#include "Board.h"
#include <M5Unified.h>
#include "Log.h"

namespace {
// Howard Hinnant's days-from-civil / civil-from-days (proleptic Gregorian, UTC).
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + int64_t(doe) - 719468;
}

void civilFromDays(int64_t z, int &y, unsigned &m, unsigned &d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = unsigned(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = int(yoe + era * 400) + (m <= 2);
}
}  // namespace

namespace board {
void begin(uint8_t brightness) {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  cfg.internal_mic = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = true;
  cfg.output_power = false;
  M5.begin(cfg);
  // StickS3 ST7789 panel is 135x240 with rotation_offset 0 (M5GFX
  // esp32s3_board_data.hpp, panel_st7789v2), so rotation 0 is portrait.
  M5.Display.setRotation(0);
  M5.Display.setBrightness(brightness);
  // Callers own debounce; M5Unified only supplies the current button state.
  M5.BtnA.setDebounceThresh(0);
  M5.BtnB.setDebounceThresh(0);
  if (M5.getBoard() != m5::board_t::board_M5StickS3)
    LOG_W("expected Stick-S3, detected M5 board id %d", int(M5.getBoard()));
}

void update() { M5.update(); }

bool isPressed(uint8_t i) {
  return i == 0 ? M5.BtnA.isPressed() : i == 1 ? M5.BtnB.isPressed() : false;
}

int batteryPercent() {
  const int level = M5.Power.getBatteryLevel();
  return level < 0 ? -1 : level > 100 ? 100 : level;
}

bool charging() { return M5.Power.isCharging() == m5::Power_Class::is_charging; }

int batteryMv() {
  const int mv = M5.Power.getBatteryVoltage();
  return mv > 0 ? mv : -1;
}

void setBrightness(uint8_t level) {
  M5.Display.setBrightness(level);
}

void tone(uint16_t hz, uint16_t ms, uint8_t volume) {
  if (!M5.Speaker.isEnabled() || hz == 0 || ms == 0 || volume == 0) return;
  M5.Speaker.setVolume(volume > kMaxToneVolume ? kMaxToneVolume : volume);
  M5.Speaker.tone(hz, ms);  // asynchronous: queued to the speaker task
}

uint32_t rtcEpoch() {
  if (!M5.Rtc.isEnabled()) return 0;
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt) || dt.date.year < 2024) return 0;
  const int64_t days = daysFromCivil(dt.date.year, dt.date.month, dt.date.date);
  const int64_t s = days * 86400 + dt.time.hours * 3600 + dt.time.minutes * 60 + dt.time.seconds;
  return s > 0 && s <= 0xFFFFFFFFLL ? uint32_t(s) : 0;
}

bool setRtcEpoch(uint32_t epoch) {
  if (!M5.Rtc.isEnabled()) return false;
  int y;
  unsigned m, d;
  civilFromDays(epoch / 86400, y, m, d);
  const uint32_t sod = epoch % 86400;
  m5::rtc_datetime_t dt;
  dt.date.year = int16_t(y);
  dt.date.month = int8_t(m);
  dt.date.date = int8_t(d);
  dt.date.weekDay = int8_t((epoch / 86400 + 4) % 7);  // 1970-01-01 was a Thursday
  dt.time.hours = int8_t(sod / 3600);
  dt.time.minutes = int8_t(sod % 3600 / 60);
  dt.time.seconds = int8_t(sod % 60);
  M5.Rtc.setDateTime(dt);
  // The I2C write can fail silently: read the time back and compare.
  const uint32_t back = rtcEpoch();
  const uint32_t diff = back > epoch ? back - epoch : epoch - back;
  return back != 0 && diff <= 2;
}

M5GFX& display() { return M5.Display; }
}  // namespace board
