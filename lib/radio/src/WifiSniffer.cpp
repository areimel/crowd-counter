#include "WifiSniffer.h"

#include <esp_event.h>
#include <esp_wifi.h>

#include "Log.h"
#include "ProbeParse.h"

namespace cc {

namespace {

// UNCONFIRMED: the on-device spike has not yet shown that WIFI_MODE_NULL
// delivers promiscuous frames. If it does not, switch this to WIFI_MODE_STA
// and still never call connect or scan (STA without connect does not transmit).
constexpr wifi_mode_t SNIFF_MODE = WIFI_MODE_NULL;

constexpr uint8_t CHANNEL_NONE = 0xFF;  // out of range: counted nowhere
constexpr size_t FCS_LEN = 4;

ObservationSink* gSink = nullptr;
volatile uint8_t gChannelIdx = CHANNEL_NONE;

void wipe(uint8_t* p, size_t n) {
  volatile uint8_t* v = p;
  for (size_t i = 0; i < n; i++) v[i] = 0;
}

// Wi-Fi driver task (not an ISR). Not IRAM: hashing code lives in flash.
void onFrame(void* buf, wifi_promiscuous_pkt_type_t type) {
  ObservationSink* sink = gSink;
  if (sink == nullptr || type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pkt = static_cast<const wifi_promiscuous_pkt_t*>(buf);
  sink->countWifiFrame();
  const uint32_t sig = pkt->rx_ctrl.sig_len;
  if (sig < FCS_LEN) return;
  ProbeInfo info;
  if (!parseProbeRequest(pkt->payload, sig - FCS_LEN, info)) return;
  sink->countProbe(gChannelIdx);
  const int8_t rssi = static_cast<int8_t>(pkt->rx_ctrl.rssi);
  if (sink->aboveFloor(rssi)) {
    sink->submit(Signal::WIFI, info.mac, rssi,
                 info.randomized ? AddrKind::ROTATING : AddrKind::FIXED, 0);
  }
  wipe(info.mac, sizeof(info.mac));
}

bool check(esp_err_t err, const char* what) {
  if (err == ESP_OK) return true;
  LOG_E("wifi: %s failed (%d)", what, static_cast<int>(err));
  return false;
}

}  // namespace

bool WifiSniffer::begin(ObservationSink& sink) {
  if (ready_) return true;
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

void WifiSniffer::listen(uint8_t channel, uint8_t channelIdx) {
  if (!ready_) return;
  if (!check(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE), "channel")) return;
  gChannelIdx = channelIdx;  // before enabling, so no frame is misattributed
  if (!check(esp_wifi_set_promiscuous(true), "promiscuous on")) gChannelIdx = CHANNEL_NONE;
}

void WifiSniffer::pause() {
  if (!ready_) return;
  esp_wifi_set_promiscuous(false);
  gChannelIdx = CHANNEL_NONE;
}

}  // namespace cc
