#pragma once
#include "Board.h"
#include "Config.h"
#include "CounterTypes.h"
#include "DeviceTable.h"
#include "Estimator.h"
#include "Gesture.h"
#include "NightStore.h"
#include "RadioScheduler.h"
#include "SettingsStore.h"
#include "Ui.h"
#include "UiController.h"

// Everything the firmware owns, in one statically allocated object. main.cpp
// wires it; Night.cpp runs the night lifecycle and log; Cli.cpp reads it.
struct App {
  cc::RadioScheduler radio;
  cc::SettingsStore store;
  cc::NightStore nights;
  Ui ui;
  UiController controller{config::uiLimits()};
  Gesture keys[board::BUTTON_COUNT] = {{AppConfig::DEBOUNCE_MS, AppConfig::DOUBLE_CLICK_MS, AppConfig::LONG_PRESS_MS},
                     {AppConfig::DEBOUNCE_MS, AppConfig::DOUBLE_CLICK_MS, AppConfig::LONG_PRESS_MS}};
  cc::DeviceTable* table = nullptr;  // slots live in PSRAM
  cc::Estimator estimator{config::estimator()};

  cc::Settings settings = {};
  cc::NightMeta meta;
  cc::NightState night = {};  // mirrored to RTC memory
  cc::CalProfile cal = {};    // active venue
  cc::Ratios ratios = {};
  cc::RawCounts counts = {};
  cc::EstimateSnapshot snap = {};

  bool nightActive = false;     // false while the New night / Resume prompt is up
  bool fileCreated = false;     // the night file is created with its first row
  bool setupStale = false;      // cold resume lost the allowlist
  bool capturePending = false;  // setup capture waiting for a full window
  bool logError = false;        // the last night-log write failed
  uint32_t warmUntilS = 0;      // window still filling before this
  uint32_t nightBaseS = 0;      // boot time when the night (re)started this boot
  uint32_t baseElapsedS = 0;    // night time accumulated before that
  uint32_t lastMinute = 0;
  uint32_t epochOffset = 0;  // epoch - bootSeconds, set by the `time` command
};

extern App app;

uint32_t nowSeconds();           // seconds since boot
uint32_t epochNow(uint32_t nowS);  // UTC seconds, 0 when the clock is unknown
bool warmingUp(uint32_t nowS);

namespace night {
// freshSalt: rotate the salt and empty the table (a new night mid-session).
void startNew(uint32_t nowS, bool freshSalt);
bool resumeWarm(uint32_t nowS);  // after a crash/soft reset; false if RTC state is unusable
void resumeCold(uint32_t nowS);  // after power loss, chosen at the prompt
void end(uint32_t nowS);
void tick(uint32_t nowS);  // minute rows and the RTC mirror; call after each estimate
uint32_t elapsedS(uint32_t nowS);
void logMethod(uint32_t nowS);  // call after venue, gate, baseline or ratio changes
void logManualCount(uint32_t nowS, uint16_t truth);
void logAlert(uint32_t nowS);
cc::NightSummary summarize(uint32_t nightId);
}  // namespace night

namespace cli {
void poll(uint32_t nowS);
}
