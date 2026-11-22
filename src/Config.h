#pragma once
#include "CounterTypes.h"
#include "generated_config.h"

// The one place AppConfig (generated from config.yaml) is mapped into the
// params structs the pure modules take. Constants that are not user-tunable
// live here too.
namespace config {

constexpr uint32_t TABLE_SLOTS = 8192;  // power of two, > TABLE_CAPACITY
constexpr uint32_t FALLBACK_SLOTS = 256;  // DRAM table if PSRAM is unavailable
constexpr int8_t RSSI_FLOOR = AppConfig::RSSI_GATE_MIN;  // dropped in the radio callback
constexpr int8_t GATE_STEP = 5;
constexpr uint16_t MANUAL_STEP = 5;
constexpr uint32_t PROMPT_TIMEOUT_MS = 15000;
constexpr uint32_t SETTINGS_TIMEOUT_MS = 60000;
constexpr uint32_t CONFIRM_TIMEOUT_MS = 5000;
constexpr uint32_t CAPTURE_NOTE_MS = 2000;
// A night that is never ended still rotates its salt: no id outlives this.
constexpr uint32_t NIGHT_MAX_S = 18 * 3600;
constexpr uint32_t PRUNE_EVERY_MIN = 30;
constexpr int DRAIN_PER_LOOP = 512;  // observations taken from the queue per loop
constexpr size_t NIGHTS_KEPT = 20;
constexpr size_t MIN_FREE_BYTES = 64 * 1024;
constexpr uint32_t ALERT_FLASH_MS = 3000;
constexpr uint16_t CHIME_HZ = 1320, CHIME_MS = 400;
constexpr uint16_t BEEP_HZ = 880, BEEP_MS = 80;     // manual count / capture confirmed
constexpr uint16_t ERROR_HZ = 220, ERROR_MS = 250;  // manual count rejected

static_assert(AppConfig::TABLE_CAPACITY < TABLE_SLOTS, "table_capacity must leave free slots");
static_assert(AppConfig::WIFI_CHANNEL_COUNT <= cc::WIFI_CHANNELS_MAX, "too many Wi-Fi channels");

inline cc::ClassWeights classWeights() {
  cc::ClassWeights w = {};
  for (uint8_t i = 0; i < cc::CLASS_COUNT; ++i) w.w[i] = AppConfig::CLASS_WEIGHTS[i];
  return w;
}

inline cc::CensusParams census(int8_t rssiGate) {
  cc::CensusParams p = {};
  p.windowS = AppConfig::WINDOW_S;
  p.rssiGate = rssiGate;
  p.backgroundAfterS = AppConfig::BACKGROUND_AFTER_S;
  p.weights = classWeights();
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
  l.gateMin = AppConfig::RSSI_GATE_MIN;
  l.gateMax = AppConfig::RSSI_GATE_MAX;
  l.gateStep = GATE_STEP;
  l.venueCount = AppConfig::VENUE_COUNT;
  l.manualStep = MANUAL_STEP;
  l.dimAfterMs = uint32_t(AppConfig::DIM_AFTER_S) * 1000;
  l.promptTimeoutMs = PROMPT_TIMEOUT_MS;
  l.settingsTimeoutMs = SETTINGS_TIMEOUT_MS;
  l.confirmTimeoutMs = CONFIRM_TIMEOUT_MS;
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
