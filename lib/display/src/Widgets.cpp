#include "Widgets.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace Widgets {
using namespace Theme;

namespace {
int spacingFor(int size, int spacing) { return spacing >= 0 ? spacing : (size == 1 ? 1 : 0); }
}  // namespace

int textWidth(const char* s, int size, int spacing) {
  int n = int(strlen(s));
  int sp = spacingFor(size, spacing);
  return n ? n * (6 * size + sp) - sp : 0;
}

// Letter-spaced Font0 text; one glyph per write so spacing can be 0 or 1.
void label(M5Canvas& c, int x, int y, const char* s, uint16_t color, int size, int spacing) {
  int sp = spacingFor(size, spacing);
  c.setFont(&fonts::Font0);
  c.setTextSize(size);
  c.setTextColor(color);
  for (; *s; ++s) {
    c.setCursor(x, y);
    c.write(uint8_t(*s));
    x += 6 * size + sp;
  }
}

void labelRight(M5Canvas& c, int right, int y, const char* s, uint16_t color, int size,
                int spacing) {
  label(c, right - textWidth(s, size, spacing), y, s, color, size, spacing);
}

void labelCenter(M5Canvas& c, int cx, int y, const char* s, uint16_t color, int size,
                 int spacing) {
  label(c, cx - textWidth(s, size, spacing) / 2, y, s, color, size, spacing);
}

void upper(char* out, size_t cap, const char* in) {
  size_t i = 0;
  for (; in && in[i] && i + 1 < cap; ++i) {
    char ch = in[i];
    out[i] = (ch >= 'a' && ch <= 'z') ? char(ch - 32) : ch;
  }
  if (cap) out[i] = 0;
}

void row(M5Canvas& c, int y, const char* name, const char* value, bool highlighted, int maxNameSize) {
  const uint16_t nameColor = highlighted ? BG : DIM;
  const uint16_t valueColor = highlighted ? BG : INK;
  if (highlighted) {
    c.fillRect(ROW_BAR_X, y + 1, W - 2 * ROW_BAR_X, ROW_H - 2, ACCENT);
  } else {
    c.drawFastHLine(MARGIN, y + ROW_H - 1, W - 2 * MARGIN, LINE);
  }
  struct Fit { int nameSize, valueSize; };
  static const Fit fits[] = {{2, 2}, {1, 2}, {1, 1}};
  const int avail = RIGHT - MARGIN;
  for (const Fit& f : fits) {
    if (f.nameSize > maxNameSize) continue;
    if (textWidth(name, f.nameSize) + ROW_GAP + textWidth(value, f.valueSize) > avail) continue;
    label(c, MARGIN, y + (ROW_H - 8 * f.nameSize) / 2, name, nameColor, f.nameSize);
    labelRight(c, RIGHT, y + (ROW_H - 8 * f.valueSize) / 2, value, valueColor, f.valueSize);
    return;
  }
  // Nothing fits side by side: name on top, value under it (size 1, ~17 chars).
  label(c, MARGIN, y + 3, name, nameColor);
  labelRight(c, RIGHT, y + 15, value, valueColor);
}

void rowNum(M5Canvas& c, int y, const char* name, long value, bool highlighted, int maxNameSize) {
  char buf[16];
  snprintf(buf, sizeof buf, "%ld", value);
  row(c, y, name, buf, highlighted, maxNameSize);
}

void rowf(M5Canvas& c, int y, const char* name, const char* fmt, ...) {
  char buf[24];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  row(c, y, name, buf);
}

void header(M5Canvas& c, const char* title, const char* right) {
  label(c, MARGIN, HEADER_TITLE_Y, title, INK, 2);
  if (right) labelRight(c, RIGHT, HEADER_TITLE_Y + 4, right, DIM);
  c.drawFastHLine(MARGIN, HEADER_RULE_Y, W - 2 * MARGIN, LINE);
}

void battery(M5Canvas& c, int right, int y, int pct, bool charging) {
  const int x = right - 22;  // body 20x10 + 2 px nub
  c.drawRect(x, y, 20, 10, DIM);
  c.fillRect(x + 20, y + 3, 2, 4, DIM);
  if (pct >= 0) {
    int w = (pct * 16 + 50) / 100;
    if (pct > 0 && w < 1) w = 1;
    c.fillRect(x + 2, y + 2, w, 6, charging ? ACCENT : (pct <= 15 ? ALERT : INK));
  }
  char buf[16];
  if (pct < 0) snprintf(buf, sizeof buf, "--");
  else snprintf(buf, sizeof buf, "%d%%", pct);
  const uint16_t color = pct < 0 ? DIM : (pct <= 15 ? ALERT : INK);
  const int tx = x - 4;
  labelRight(c, tx, y + 1, buf, color, 1, 0);
  if (charging) labelRight(c, tx - textWidth(buf, 1, 0) - 3, y + 1, "+", ACCENT);
}

void statusBar(M5Canvas& c, int y, const char* text, uint16_t fill, uint16_t ink) {
  c.fillRect(BAR_X, y, BAR_W, BAR_H, fill);
  labelCenter(c, CENTER_X, y + (BAR_H - 16) / 2, text, ink, 2);
}

void pulse(M5Canvas& c, int x, int y, uint8_t phase) {
  for (int i = 0; i < PULSE_DOTS; ++i) {
    c.fillCircle(x + PULSE_R + i * PULSE_PITCH, y, PULSE_R, i == phase ? ACCENT : LINE);
  }
}

// Font7 (7-segment, RLE rectangles) scaled with setTextSize(sx, sy): the
// stretch is horizontal only as far as needed to fit 4 digits in BIG_MAX_W.
void bigNumber(M5Canvas& c, int y, unsigned value, uint16_t color) {
  if (value > 9999) value = 9999;
  char buf[8];
  snprintf(buf, sizeof buf, "%u", value);
  c.setFont(&fonts::Font7);
  c.setTextSize(1.0f, 1.0f);
  const int w1 = c.textWidth(buf);
  float sx = w1 > 0 ? float(BIG_MAX_W) / float(w1) : BIG_SX_MAX;
  if (sx > BIG_SX_MAX) sx = BIG_SX_MAX;
  c.setTextSize(sx, BIG_SY);
  const int w = c.textWidth(buf);
  c.setTextColor(color);
  c.setTextDatum(textdatum_t::top_left);
  c.drawString(buf, CENTER_X - w / 2, y);
  c.setTextSize(1.0f, 1.0f);
}

void meter(M5Canvas& c, int x, int y, int w, int h, int num, int den, uint16_t color) {
  c.drawRect(x, y, w, h, LINE);
  if (den <= 0 || num <= 0) return;
  if (num > den) num = den;
  c.fillRect(x + 1, y + 1, (w - 2) * num / den, h - 2, color);
}

}  // namespace Widgets
