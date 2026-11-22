#include "WifiSniffer.h"

#include <esp_event.h>
#include <esp_wifi.h>

#include "Log.h"
#include "ProbeParse.h"
#include "SecureZero.h"

namespace cc {

namespace {

// STILL UNVERIFIED ON HARDWARE: that WIFI_MODE_NULL delivers promiscuous
// frames after esp_wifi_start(). Nothing has been run on a device yet. If the
// first spike shows zero management frames, change this one constant to
// WIFI_MODE_STA: station mode without ever calling connect or scan does not
// transmit, so the receive-only guarantee holds either way.
constexpr wifi_mode_t SNIFF_MODE = WIFI_MODE_NULL;

constexpr size_t FCS_LEN = 4;

ObservationSink* gSink = nullptr;
// Written once in begin(), before the rx callback is registered; read-only after.
uint8_t gChannels[WIFI_CHANNELS_MAX] = {};
uint8_t gChannelCount = 0;

// Index of `channel` in the configured list, or -1.
int channelIndex(uint8_t channel) {
  for (uint8_t i = 0; i < gChannelCount; i++)
    if (gChannels[i] == channel) return i;
  return -1;
}

// Wi-Fi driver task (not an ISR). Not IRAM: hashing code lives in flash.
void onFrame(void* buf, wifi_promiscuous_pkt_type_t type) {
  ObservationSink* sink = gSink;
  if (sink == nullptr || type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pkt = static_cast<const wifi_promiscuous_pkt_t*>(buf);
  if (pkt->rx_ctrl.rx_state != 0) return;  // receive error: payload may be garbage
  sink->countWifiFrame();
  const uint32_t sig = pkt->rx_ctrl.sig_len;
  if (sig < FCS_LEN) return;
  ProbeInfo info;
  if (!parseProbeRequest(pkt->payload, sig - FCS_LEN, info)) return;
  const int idx = channelIndex(static_cast<uint8_t>(pkt->rx_ctrl.channel));
  if (idx >= 0) {  // attributed by the frame's own channel, not by scheduler state
    sink->countProbe(static_cast<uint8_t>(idx));
    const int8_t rssi = static_cast<int8_t>(pkt->rx_ctrl.rssi);
    if (sink->aboveFloor(rssi)) {
      sink->submit(Signal::WIFI, info.mac, rssi,
                   info.randomized ? AddrKind::ROTATING : AddrKind::FIXED, 0);
    }
  }
  secureZero(info.mac, sizeof(info.mac));
}

bool check(esp_err_t err, const char* what) {
  if (err == ESP_OK) return true;
  LOG_E("wifi: %s failed (%d)", what, static_cast<int>(err));
  return false;
}

}  // namespace

bool WifiSniffer::begin(ObservationSink& sink, const uint8_t* channels, uint8_t count) {
  if (ready_) return true;
  if (count > WIFI_CHANNELS_MAX) count = WIFI_CHANNELS_MAX;
  for (uint8_t i = 0; i < count; i++) channels_[i] = gChannels[i] = channels[i];
  count_ = gChannelCount = count;
  gSink = &sink;
  const esp_err_t loop = esp_event_loop_create_default();
  if (loop != ESP_ERR_INVALID_STATE && !check(loop, "event loop")) return false;

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.nvs_enable = 0;
  if (!check(esp_wifi_init(&cfg), "init")) return false;
  if (!check(esp_wifi_set_storage(WIFI_STORAGE_RAM), "storage")) return false;
  if (!check(esp_wifi_set_mode(SNIFF_MODE), "mode")) return false;
  if (!check(esp_wifi_start(), "start")) return false;

  wifi_promiscuous_filter_t filter = {};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  if (!check(esp_wifi_set_promiscuous_filter(&filter), "filter")) return false;
  if (!check(esp_wifi_set_promiscuous_rx_cb(&onFrame), "rx cb")) return false;
  ready_ = true;
  return true;
}

void WifiSniffer::listen(uint8_t channelIdx) {
  if (!ready_ || channelIdx >= count_) return;
  // Usual sniffer order: promiscuous first, then tune.
  if (!promiscuous_) {
    if (!check(esp_wifi_set_promiscuous(true), "promiscuous on")) return;
    promiscuous_ = true;
  }
  const uint8_t channel = channels_[channelIdx];
  if (!check(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE), "channel")) return;
  static bool mismatchLogged = false;
  if (!mismatchLogged) {
    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&primary, &second) == ESP_OK && primary != channel) {
      mismatchLogged = true;
      LOG_E("wifi: asked for channel %u, driver reports %u", static_cast<unsigned>(channel),
            static_cast<unsigned>(primary));
    }
  }
}

void WifiSniffer::pause() {
  if (!ready_) return;
  esp_wifi_set_promiscuous(false);
  promiscuous_ = false;
}

}  // namespace cc
