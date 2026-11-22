#pragma once
#include <M5GFX.h>
#include <stdint.h>
#include "CounterTypes.h"

// Sprite renderer. render() is a pure function of the model (no clocks): draw
// the whole screen into a 135x240 16-bit PSRAM sprite, then push it once.
class Ui {
 public:
  // venueNames must outlive Ui. Returns false if the sprite allocation failed.
  bool begin(M5GFX& display, const char* const* venueNames, uint8_t venueCount);
  void render(const cc::UiModel& m);
  M5Canvas& canvas() { return canvas_; }  // for the serial `shot` dump

 private:
  void drawCount(const cc::UiModel& m);
  void drawDetail(const cc::UiModel& m);
  void drawSummary(const cc::UiModel& m);
  void drawSettings(const cc::UiModel& m);
  void drawPrompt(const cc::UiModel& m);
  const char* venueName(uint8_t index) const;

  M5GFX* display_ = nullptr;
  M5Canvas canvas_;
  const char* const* venues_ = nullptr;
  uint8_t venueCount_ = 0;
};

// Field-wise equality (UiModel has padding, so memcmp is unsafe).
bool sameModel(const cc::UiModel& a, const cc::UiModel& b);
