#pragma once
#include "CounterTypes.h"
#include "generated_config.h"

// The one place AppConfig (generated from config.yaml) is mapped into the
// params structs the pure modules take. Constants that are not user-tunable
// live here too.
namespace config {

constexpr uint32_t TABLE_SLOTS = 8192;  // power of two, > TABLE_CAPACITY
constexpr int8_t RSSI_FLOOR = -95;      // dropped in the radio callback
constexpr int8_t GATE_MIN = -95, GATE_MAX = -50, GATE_STEP = 5;
constexpr uint16_t MANUAL_STEP = 5;
constexpr uint32_t PROMPT_TIMEOUT_MS = 15000;  // Ui.cpp draws the same 15 s bar
constexpr size_t NIGHTS_KEPT = 20;
constexpr size_t MIN_FREE_BYTES = 64 * 1024;
constexpr uint32_t ALERT_FLASH_MS = 3000;
constexpr uint16_t CHIME_HZ = 1320, CHIME_MS = 400;
constexpr uint16_t BEEP_HZ = 880, BEEP_MS = 80;  // manual count / capture feedback

static_assert(AppConfig::TABLE_CAPACITY < TABLE_SLOTS, "table_capacity must leave free slots");
static_assert(AppConfig::WIFI_CHANNEL_COUNT <= cc::WIFI_CHANNELS_MAX, "too many Wi-Fi channels");

inline cc::CensusParams census(int8_t rssiGate) {
  cc::CensusParams p = {};
  p.windowS = AppConfig::WINDOW_S;
  p.rssiGate = rssiGate;
  p.backgroundAfterS = AppConfig::BACKGROUND_AFTER_S;
  for (uint8_t i = 0; i < cc::CLASS_COUNT; ++i) p.weights.w[i] = AppConfig::CLASS_WEIGHTS[i];
  return p;
}

inline cc::EstimatorParams estimator() {
  return {AppConfig::BLEND_BLE, AppConfig::RECOMPUTE_S, AppConfig::SMOOTHING_S,
          AppConfig::PEAK_HOLD_S, AppConfig::ALERT_HOLD_S};
}

inline cc::RadioParams radio() {
  cc::RadioParams p = {};
  p.bleSliceMs = AppConfig::BLE_SLICE_MS;
  p.wifiDwellMs = AppConfig::WIFI_DWELL_MS;
  p.channelCount = AppConfig::WIFI_CHANNEL_COUNT;
  for (uint8_t i = 0; i < p.channelCount; ++i) p.channels[i] = AppConfig::WIFI_CHANNELS[i];
  p.rssiFloor = RSSI_FLOOR;
  return p;
}

inline cc::UiLimits uiLimits() {
  cc::UiLimits l = {};
  l.thresholdMin = AppConfig::THRESHOLD_STEP;
  l.thresholdMax = AppConfig::THRESHOLD_MAX;
  l.thresholdStep = AppConfig::THRESHOLD_STEP;
  l.gateMin = GATE_MIN;
  l.gateMax = GATE_MAX;
  l.gateStep = GATE_STEP;
  l.venueCount = AppConfig::VENUE_COUNT;
  l.manualStep = MANUAL_STEP;
  l.dimAfterMs = uint32_t(AppConfig::DIM_AFTER_S) * 1000;
  l.promptTimeoutMs = PROMPT_TIMEOUT_MS;
  return l;
}

inline cc::Settings defaultSettings() {
  return {AppConfig::THRESHOLD_DEFAULT, 0, AppConfig::RSSI_GATE_DBM};
}

inline cc::Ratios defaultRatios(uint8_t venue) {
  const AppConfig::Venue& v = AppConfig::VENUES[venue < AppConfig::VENUE_COUNT ? venue : 0];
  return {v.ratioBle, v.ratioWifi, false};
}

inline const char* venueName(uint8_t venue) {
  return AppConfig::VENUES[venue < AppConfig::VENUE_COUNT ? venue : 0].name;
}

}  // namespace config
