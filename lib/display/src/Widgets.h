#pragma once
#include <M5GFX.h>
#include <stddef.h>
#include <stdint.h>
#include "CounterTypes.h"
#include "Theme.h"

// Shared drawing primitives for every screen. All of them draw into an
// M5Canvas and keep no state; text is ASCII-only Font0 (6x8 cell, scaled).
namespace Widgets {

// ---- Layout constants (portrait 135x240). Change a layout here, once. -------
constexpr int W = 135, H = 240;
constexpr int MARGIN = 6;             // left/right text margin
constexpr int RIGHT = W - MARGIN;     // x of right-aligned text (exclusive edge)
constexpr int CENTER_X = W / 2;
constexpr int HEADER_TITLE_Y = 4;     // size-2 title: y 4..20
constexpr int HEADER_RULE_Y = 24;     // hairline under the header
constexpr int ROWS_Y = 30;            // first row of a list screen
constexpr int ROW_H = 28;             // list row pitch (size-2 text is 16 tall)
constexpr int ROW_BAR_X = 3;          // highlight bar spans x 3..131
constexpr int ROW_GAP = 6;            // minimum label-to-value gap
constexpr int BAR_X = 4, BAR_W = W - 2 * BAR_X, BAR_H = 28;  // status bar
constexpr int BIG_MAX_W = 131;        // widest big number (127 px target + slack)
constexpr float BIG_SX_MAX = 1.5f;    // horizontal stretch cap (1-2 digits)
constexpr float BIG_SY = 2.0f;        // Font7 is 48 tall: 96 px digits
constexpr int PULSE_DOTS = 4, PULSE_PITCH = 10, PULSE_R = 3;

// ---- Text -------------------------------------------------------------------
// spacing < 0 selects the default: 1 px for size 1, 0 px for larger sizes.
int textWidth(const char* s, int size, int spacing = -1);
void label(M5Canvas& c, int x, int y, const char* s, uint16_t color, int size = 1,
           int spacing = -1);
void labelRight(M5Canvas& c, int right, int y, const char* s, uint16_t color, int size = 1,
                int spacing = -1);
void labelCenter(M5Canvas& c, int cx, int y, const char* s, uint16_t color, int size = 1,
                 int spacing = -1);
void upper(char* out, size_t cap, const char* in);

// ---- Composite widgets ------------------------------------------------------
// Label left (DIM), value right (INK). Tries label/value sizes 2/2, 1/2, 1/1,
// then stacks the value under the label (size 1) so any string fits ROW_H.
// highlighted: filled ACCENT bar, BG text. maxNameSize 1 forces small names
// (settings list) so every row of a screen looks alike.
void row(M5Canvas& c, int y, const char* name, const char* value, bool highlighted = false,
         int maxNameSize = 2);
// Numeric value / printf-formatted value convenience wrappers around row().
void rowNum(M5Canvas& c, int y, const char* name, long value, bool highlighted = false,
            int maxNameSize = 2);
void rowf(M5Canvas& c, int y, const char* name, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));
// Size-2 title at left, optional size-1 DIM text at right, hairline below.
void header(M5Canvas& c, const char* title, const char* right = nullptr);
// Right-aligned battery icon + percent; pct < 0 shows "--"; low (<=15) in ALERT.
void battery(M5Canvas& c, int right, int y, int pct, bool charging);
// Coloured block with centred size-2 text.
void statusBar(M5Canvas& c, int y, const char* text, uint16_t fill, uint16_t ink);
// PULSE_DOTS dots, dot number `phase` (0..3) lit; (x, y) = left edge, centre line.
void pulse(M5Canvas& c, int x, int y, uint8_t phase);
// Estimate in Font7, centred on the screen, top edge y, scaled to fit.
void bigNumber(M5Canvas& c, int y, unsigned value, uint16_t color);
// Draining bar: x..x+w, filled fraction num/den.
void meter(M5Canvas& c, int x, int y, int w, int h, int num, int den, uint16_t color);

inline uint16_t statusColor(cc::ThresholdState s) {
  return s == cc::ThresholdState::ABOVE ? Theme::STATUS_GREEN
         : s == cc::ThresholdState::NEAR ? Theme::STATUS_AMBER
                                         : Theme::STATUS_GREY;
}

}  // namespace Widgets
