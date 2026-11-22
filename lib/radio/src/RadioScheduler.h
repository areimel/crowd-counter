#pragma once
#include "BleScanner.h"
#include "ObservationSink.h"
#include "WifiSniffer.h"

namespace cc {

// Time-slices BLE and Wi-Fi on the shared radio (Wi-Fi promiscuous RX during
// a BLE scan is rated unstable): BLE passive scan for bleSliceMs, stop, then
// a Wi-Fi dwell of wifiDwellMs on the next channel, rotate, repeat. If only one
// radio came up it runs continuously instead (Wi-Fi alone still rotates
// channels every dwell). Only the
// internal task starts/stops radios; callbacks never do.
//
// RadioStats::wifiProbes[i] counts probes heard on params.channels[i] (the
// position in the sanitised list, not the channel number).
class RadioScheduler {
 public:
  // True if at least one radio came up and the task started; false on a second call.
  bool begin(const RadioParams& params);
  void setSalt(const uint8_t salt[16]) { sink_.setSalt(salt); }  // safe while running
  bool poll(Observation& out) { return sink_.poll(out); }        // non-blocking
  void flush() { sink_.flush(); }                                // drop queued observations (after a salt change)
  RadioStats stats() const { return sink_.stats(); }
  bool bleReady() const { return ble_.ready(); }
  bool wifiReady() const { return wifi_.ready(); }
  uint32_t bleStartFailures() const { return bleStartFailures_; }  // approximate; written by the radio task

 private:
  static void taskEntry(void* self) { static_cast<RadioScheduler*>(self)->run(); }
  void run();
  void startBle();

  RadioParams params_ = {};
  ObservationSink sink_;
  BleScanner ble_;
  WifiSniffer wifi_;
  bool started_ = false;
  uint32_t bleStartFailures_ = 0;  // radio task only
};

}  // namespace cc
