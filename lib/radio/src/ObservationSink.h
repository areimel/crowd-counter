#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stddef.h>
#include <stdint.h>

#include "CounterTypes.h"

namespace cc {

// The one place where a radio callback turns an address into an Observation:
// RSSI floor, salted hash, enqueue, counters. Both radio callbacks call it;
// neither hashes or enqueues on its own.
//
// Privacy: `submit` reads the address once, hashes it on the caller's stack
// and keeps nothing. Until a salt has been set, observations are discarded
// rather than hashed with an empty salt.
class ObservationSink {
 public:
  static constexpr size_t QUEUE_LEN = 1024;

  bool begin(int8_t rssiFloor);

  // Safe at any time, from any task; a callback never sees a half-written salt.
  void setSalt(const uint8_t salt[16]);

  bool aboveFloor(int8_t rssi) const { return rssi >= rssiFloor_; }

  // From a radio callback. Never blocks; a full queue counts a drop.
  void submit(Signal signal, const uint8_t mac[6], int8_t rssi, AddrKind kind, uint8_t markers);

  // Counters bumped by the radios (callback-safe).
  void countBleAdvert() { bump(stats_.bleAdverts); }
  void countWifiFrame() { bump(stats_.wifiFrames); }
  void countProbe(uint8_t channelIdx) {
    if (channelIdx < WIFI_CHANNELS_MAX) bump(stats_.wifiProbes[channelIdx]);
  }

  // Main loop: non-blocking.
  bool poll(Observation& out);
  RadioStats stats() const;

 private:
  static void bump(uint32_t& v) { __atomic_fetch_add(&v, 1, __ATOMIC_RELAXED); }
  bool copySalt(uint8_t out[16]);

  QueueHandle_t queue_ = nullptr;
  int8_t rssiFloor_ = -127;
  mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  uint8_t salt_[16] = {};
  bool saltSet_ = false;
  RadioStats stats_ = {};
};

}  // namespace cc
