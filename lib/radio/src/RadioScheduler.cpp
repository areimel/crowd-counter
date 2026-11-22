#include "RadioScheduler.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Log.h"

namespace cc {

namespace {
constexpr uint32_t IDLE_MS = 1000;
constexpr uint32_t STACK_BYTES = 4096;
constexpr UBaseType_t PRIORITY = 2;
constexpr BaseType_t CORE = 0;  // Arduino loop runs on core 1
}  // namespace

bool RadioScheduler::begin(const RadioParams& params) {
  if (started_) return false;
  params_ = params;
  if (params_.channelCount > WIFI_CHANNELS_MAX) params_.channelCount = WIFI_CHANNELS_MAX;
  uint8_t n = 0;
  for (uint8_t i = 0; i < params_.channelCount; i++) {
    const uint8_t ch = params_.channels[i];
    if (ch >= 1 && ch <= 13) params_.channels[n++] = ch;
  }
  params_.channelCount = n;

  if (!sink_.begin(params_.rssiFloor)) {
    LOG_E("radio: queue alloc failed");
    return false;
  }
  const bool ble = ble_.begin(sink_);
  const bool wifi = wifi_.begin(sink_);
  LOG_I("radio: ble=%d wifi=%d channels=%u", ble, wifi, static_cast<unsigned>(n));
  if (!ble && !wifi) return false;

  if (xTaskCreatePinnedToCore(&taskEntry, "radio", STACK_BYTES, this, PRIORITY, nullptr, CORE) !=
      pdPASS) {
    LOG_E("radio: task create failed");
    return false;
  }
  started_ = true;
  return true;
}

void RadioScheduler::run() {
  uint8_t idx = 0;
  for (;;) {
    bool ran = false;
    if (ble_.ready() && params_.bleSliceMs > 0) {
      ble_.start();
      vTaskDelay(pdMS_TO_TICKS(params_.bleSliceMs));
      ble_.stop();
      ran = true;
    }
    if (wifi_.ready() && params_.wifiDwellMs > 0 && params_.channelCount > 0) {
      if (idx >= params_.channelCount) idx = 0;
      wifi_.listen(params_.channels[idx], idx);
      vTaskDelay(pdMS_TO_TICKS(params_.wifiDwellMs));
      wifi_.pause();
      idx++;
      ran = true;
    }
    if (!ran) vTaskDelay(pdMS_TO_TICKS(IDLE_MS));
  }
}

}  // namespace cc
