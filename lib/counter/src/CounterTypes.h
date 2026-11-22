#pragma once
#include <stddef.h>
#include <stdint.h>

// Shared data contract for the Crowd Counter. Every module builds against this
// header; change it only by agreement, because all workstreams depend on it.
//
// Rules for code that includes it:
//  - Pure modules (lib/counter, lib/input, lib/app) include no Arduino or
//    generated headers. Tunables arrive in the params structs below; time
//    arrives as an argument (seconds since boot unless stated otherwise).
//  - A device address exists only on the stack of a radio callback. Nothing in
//    this header can hold one: ids are salted hashes.
namespace cc {

// ---- Observations ----------------------------------------------------------

enum class Signal : uint8_t { BLE = 0, WIFI = 1 };

// FIXED: BLE public or static-random, Wi-Fi globally administered.
// ROTATING: BLE resolvable/non-resolvable private, Wi-Fi locally administered.
enum class AddrKind : uint8_t { FIXED = 0, ROTATING = 1 };

// Coarse device type. Never anything finer: a type, not a person.
enum class DeviceClass : uint8_t { PHONE = 0, WEARABLE, AUDIO, TRACKER, COMPUTER, BEACON, OTHER };
constexpr uint8_t CLASS_COUNT = 7;

// Evidence bits collected from advert payloads; one bit per DeviceClass except
// OTHER (no bits). Stored per device so classes can be re-derived later.
namespace marker {
constexpr uint8_t PHONE = 1 << 0, WEARABLE = 1 << 1, AUDIO = 1 << 2, TRACKER = 1 << 3,
                  COMPUTER = 1 << 4, BEACON = 1 << 5;
}

// What a radio callback hands to the main loop. 16 bytes, trivially copyable.
struct Observation {
  uint64_t id;  // first 8 bytes of SHA-256(salt || signal || address); never 0
  int8_t rssi;
  Signal signal;
  AddrKind addrKind;
  uint8_t markers;  // BLE only; 0 for Wi-Fi
  uint8_t reserved[4];
};
static_assert(sizeof(Observation) == 16, "Observation layout");

// One slot of the device table. 24 bytes; lives in PSRAM on the device.
struct DeviceEntry {
  uint64_t id;
  uint32_t firstSeenS;
  uint32_t lastSeenS;
  int8_t rssi;  // fast-attack, slow-decay: max(new, (3*old + new) / 4)
  Signal signal;
  AddrKind addrKind;
  uint8_t markers;  // OR of every marker seen for this id
  uint8_t state;    // owned by DeviceTable
  uint8_t reserved[3];
};
static_assert(sizeof(DeviceEntry) == 24, "DeviceEntry layout");

// ---- Census: table -> counts ----------------------------------------------

struct ClassWeights {
  float w[CLASS_COUNT];  // indexed by DeviceClass
};

struct CensusParams {
  uint32_t windowS;           // sliding window
  int8_t rssiGate;            // entries weaker than this are not counted
  uint32_t backgroundAfterS;  // fixed-address non-phone BLE seen this long = infrastructure
  ClassWeights weights;
};

constexpr uint16_t ALLOWLIST_MAX = 96;

// Ids of fixed-address devices captured in setup mode (band gear, venue kit).
struct Allowlist {
  uint64_t ids[ALLOWLIST_MAX];
  uint16_t count;
};

// Everything counted in one sweep. "Counted" means: seen inside the window, at
// or above the RSSI gate, not allowlisted and not background.
struct RawCounts {
  uint16_t ble[CLASS_COUNT];  // counted BLE devices by class
  uint16_t wifi;              // counted Wi-Fi devices
  uint16_t wifiRotating;      // of wifi, locally administered addresses
  uint16_t bleSeen;           // BLE devices in the window before any filtering
  uint16_t wifiSeen;
  uint16_t belowGate;
  uint16_t allowlisted;
  uint16_t background;
  float bleWeighted;          // sum of class weights over counted BLE devices
  float bleWeightedRotating;  // the same, rotating-address devices only
};

// Subtracted for the rest of the night. Rotating-address devices only; fixed
// ones are in the Allowlist, so nothing is subtracted twice.
struct Baseline {
  float ble;            // bleWeightedRotating at capture
  float wifi;           // wifiRotating at capture
  uint16_t fixedCount;  // how many ids went into the Allowlist
  bool valid;
};

// ---- Estimator -------------------------------------------------------------

struct Ratios {
  float ble;        // weighted BLE devices per person
  float wifi;       // Wi-Fi devices per person
  bool wifiFitted;  // false: Wi-Fi gets zero blend weight
};

struct EstimatorParams {
  float blend;          // BLE share of the estimate when Wi-Fi is fitted (0..1)
  uint16_t recomputeS;  // update() cadence
  uint16_t smoothingS;  // moving-average length
  uint16_t peakHoldS;   // a peak must be held this long to count
  uint16_t alertHoldS;  // smoothed >= threshold this long fires the alert
};

enum class ThresholdState : uint8_t { BELOW = 0, NEAR, ABOVE };  // <80%, 80-99%, >=100% of X

// The part of the estimator that must survive a crash reset.
struct EstimatorState {
  uint16_t peak;
  uint32_t secondsAbove;
  bool alertLatched;
};

struct EstimateSnapshot {
  float bleSignal;   // max(0, bleWeighted - baseline.ble)
  float wifiSignal;  // max(0, wifi - baseline.wifi)
  uint16_t instant;  // unsmoothed estimate
  uint16_t smoothed;
  uint16_t peak;     // highest smoothed value held for peakHoldS
  uint16_t minutesAbove;
  ThresholdState state;
  bool warmingUp;  // window still filling: excluded from peak, time-above and alert
  bool alertNow;   // true on exactly one update per night
};

// ---- Calibration -----------------------------------------------------------

constexpr uint8_t CAL_PAIRS_MAX = 16;

struct CalPair {
  float ble;   // bleSignal when the manual count was entered
  float wifi;  // wifiSignal at the same moment
  uint16_t truth;
};

// One per venue type; persisted as a blob.
struct CalProfile {
  CalPair pairs[CAL_PAIRS_MAX];  // ring
  uint8_t count;
  uint8_t next;
};

// ---- Night log -------------------------------------------------------------

namespace rowflag {
constexpr uint8_t WARMUP = 1 << 0;
}

// One row per minute. Aggregates only.
struct MinuteRecord {
  uint16_t minute;  // since night start
  uint32_t epoch;   // UTC seconds, 0 when the clock is unknown
  uint16_t ble[CLASS_COUNT];
  uint16_t wifi;
  uint16_t wifiRotating;
  uint16_t background;
  uint16_t allowlisted;
  float bleSignal;
  float wifiSignal;
  uint16_t estimate;  // instant
  uint16_t smoothed;
  uint16_t peak;
  uint16_t threshold;
  uint16_t batteryMv;
  uint8_t flags;  // rowflag::*
};

// Printed in the file header and again as an event row whenever it changes, so
// anyone reading a log can see how each number was made.
struct NightMethod {
  const char* venue;
  Ratios ratios;
  Baseline baseline;
  float blend;
  uint32_t windowS;
  int8_t rssiGate;
  ClassWeights weights;
};

struct NightSummary {
  uint16_t minutes;  // rows in the night
  uint16_t peak;
  uint16_t minutesAbove;
  uint16_t threshold;  // from the last row
  uint32_t startEpoch;  // 0 when unknown
  uint32_t endEpoch;
};

// ---- Persistent and reset-surviving state ----------------------------------

// NVS. Small and rarely written.
struct Settings {
  uint16_t threshold;
  uint8_t venue;  // index into the compile-time venue list
  int8_t rssiGate;
};

// RTC no-init memory: survives a crash or soft reset, lost at power-off. This
// is the only place the salt and the allowlist ever live.
struct NightState {
  uint8_t salt[16];
  uint32_t nightId;
  uint32_t elapsedS;  // night time accumulated before this boot
  uint32_t startEpoch;
  EstimatorState estimator;
  Baseline baseline;
  Allowlist allowlist;
};

// ---- Radio -----------------------------------------------------------------

constexpr uint8_t WIFI_CHANNELS_MAX = 3;

struct RadioParams {
  uint16_t bleSliceMs;
  uint16_t wifiDwellMs;
  uint8_t channels[WIFI_CHANNELS_MAX];
  uint8_t channelCount;
  int8_t rssiFloor;  // dropped in the callback, well below any usable gate
};

struct RadioStats {
  uint32_t bleAdverts;
  uint32_t wifiFrames;  // management frames seen
  uint32_t wifiProbes[WIFI_CHANNELS_MAX];
  uint32_t queueDrops;
};

// ---- UI --------------------------------------------------------------------

enum class Screen : uint8_t { COUNT = 0, DETAIL, SUMMARY, SETTINGS, NIGHT_PROMPT };

enum class SettingsItem : uint8_t { THRESHOLD = 0, VENUE, RSSI_GATE, MANUAL_COUNT, END_NIGHT };
constexpr uint8_t SETTINGS_ITEM_COUNT = 5;

// What the controller asks the app to do. `value` carries the new setting.
enum class UiAction : uint8_t {
  NONE = 0,
  SET_THRESHOLD,
  SET_VENUE,
  SET_RSSI_GATE,
  SAVE_MANUAL_COUNT,
  END_NIGHT,
  START_CAPTURE,  // setup mode: baseline + allowlist
  NEW_NIGHT,
  RESUME_NIGHT,
};

struct UiEvent {
  UiAction action;
  int32_t value;
};

struct UiLimits {
  uint16_t thresholdMin, thresholdMax, thresholdStep;
  int8_t gateMin, gateMax, gateStep;
  uint8_t venueCount;
  uint16_t manualStep;
  uint32_t dimAfterMs;
  uint32_t promptTimeoutMs;
};

// Everything the renderer needs. Redraw only when this changes.
struct UiModel {
  Screen screen;
  SettingsItem item;
  bool dimmed;
  // count screen
  uint16_t estimate;  // smoothed
  uint16_t threshold;
  uint16_t peak;
  ThresholdState state;
  bool warmingUp;
  bool capturing;   // setup capture waiting for a full window
  bool setupStale;  // resumed after power loss: allowlist gone, re-run setup
  bool flash;       // alert flash phase
  uint8_t pulse;    // scanning animation phase, 0..3
  int8_t batteryPct;  // -1 unknown
  bool charging;
  // detail screen
  uint16_t bleCount;
  uint16_t wifiCount;
  uint16_t background;
  uint16_t baseline;  // rounded baseline.ble + baseline.wifi
  uint16_t ratioX100;
  int8_t rssiGate;
  // summary screen
  uint16_t minutesAbove;
  uint16_t nightMinutes;
  uint32_t startEpoch;  // 0 unknown
  uint8_t venue;
  // settings / prompt
  uint16_t manualCount;
  uint8_t promptSeconds;
};

}  // namespace cc
