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
class WifiSniffer {
 public:
  bool begin(ObservationSink& sink);  // bring-up; promiscuous stays OFF
  // Tune to `channel`, then enable promiscuous. `channelIdx` (0..WIFI_CHANNELS_MAX-1)
  // selects the per-channel probe counter in RadioStats::wifiProbes.
  void listen(uint8_t channel, uint8_t channelIdx);
  void pause();  // promiscuous off; later frames are not attributed to a channel
  bool ready() const { return ready_; }

 private:
  bool ready_ = false;
};

}  // namespace cc
