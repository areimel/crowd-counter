#pragma once
#include "ObservationSink.h"

namespace cc {

// Receive-only 802.11 probe-request sniffer on the raw ESP-IDF Wi-Fi driver
// (no Arduino WiFi library). Never transmits, connects, scans actively or
// deauths. Promiscuous mode is off until listen().
//
// listen()/pause() are called by the RadioScheduler task only. The rx
// callback runs in the Wi-Fi driver task, hashes the sender on its own stack
// through the sink and keeps nothing.
//
// A probe is attributed to a channel from the frame's own receive channel, so
// frames still queued in the driver after a channel switch are counted on the
// channel they were heard on. Frames on a channel outside the list given to
// begin() are ignored.
class WifiSniffer {
 public:
  // Bring-up; promiscuous stays OFF. `channels` (count <= WIFI_CHANNELS_MAX) is
  // the list listen() indexes into and the per-channel probe counters follow.
  bool begin(ObservationSink& sink, const uint8_t* channels, uint8_t count);
  // Enable promiscuous (first call), then tune to channels[channelIdx].
  void listen(uint8_t channelIdx);
  void pause();  // promiscuous off
  bool ready() const { return ready_; }

 private:
  bool ready_ = false;
  bool promiscuous_ = false;
  uint8_t channels_[WIFI_CHANNELS_MAX] = {};
  uint8_t count_ = 0;
};

}  // namespace cc
