#include "RadioScheduler.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Log.h"

namespace cc {

namespace {
constexpr uint32_t IDLE_MS = 1000;
constexpr uint32_t STACK_BYTES = 6144;
constexpr UBaseType_t PRIORITY = 2;
constexpr BaseType_t CORE = 0;  // Arduino loop runs on core 1
constexpr uint32_t BLE_FAIL_LOG_EVERY = 100;

// Verify the invariant documented at the BLE stop in run(): this task must be
// on the NimBLE host task's core and below its priority. The host task is
// created by NimBLEDevice::init() as "nimble_host".
void checkHostTaskInvariant() {
  TaskHandle_t host = xTaskGetHandle("nimble_host");
  if (host == nullptr) {
    LOG_E("radio: cannot find nimble_host task; BLE stop() safety unverified");
    return;
  }
  const UBaseType_t hostPrio = uxTaskPriorityGet(host);
  const BaseType_t hostCore = xTaskGetCoreID(host);
  if (hostCore != CORE || PRIORITY >= hostPrio) {
    LOG_E("radio: UNSAFE task layout: radio core %d prio %u, nimble_host core %d prio %u",
          static_cast<int>(CORE), static_cast<unsigned>(PRIORITY), static_cast<int>(hostCore),
          static_cast<unsigned>(hostPrio));
  }
}
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
  const bool wifi = wifi_.begin(sink_, params_.channels, params_.channelCount);
  LOG_I("radio: ble=%d wifi=%d channels=%u", ble, wifi, static_cast<unsigned>(n));
  if (!ble && !wifi) return false;

  if (xTaskCreatePinnedToCore(&taskEntry, "radio", STACK_BYTES, this, PRIORITY, nullptr, CORE) !=
      pdPASS) {
    LOG_E("radio: task create failed");
    return false;
  }
  if (ble) checkHostTaskInvariant();
  started_ = true;
  return true;
}

// Start the BLE scan; count and rate-limit-log failures (first, then every 100th).
void RadioScheduler::startBle() {
  if (ble_.start()) return;
  if (bleStartFailures_++ % BLE_FAIL_LOG_EVERY == 0)
    LOG_E("radio: BLE scan start failed (%lu so far)", static_cast<unsigned long>(bleStartFailures_));
}

void RadioScheduler::run() {
  uint8_t idx = 0;
  for (;;) {
    const bool bleOn = ble_.ready() && params_.bleSliceMs > 0;
    const bool wifiOn = wifi_.ready() && params_.wifiDwellMs > 0 && params_.channelCount > 0;

    if (bleOn && wifiOn) {  // both radios: time-share
      startBle();
      vTaskDelay(pdMS_TO_TICKS(params_.bleSliceMs));
      // SAFETY INVARIANT: stopping the scan from this task is only safe because
      // this task is pinned to core 0 at a priority below the NimBLE host task
      // (also core 0, see checkHostTaskInvariant). So it cannot run while a
      // host-task onResult callback is in flight, and NimBLEScan::stop() --
      // which clears the result vector that callback is using -- cannot race
      // it. Moving either task to another core, or raising this task's
      // priority to or above the host task's, makes this a use-after-free.
      ble_.stop();
      if (idx >= params_.channelCount) idx = 0;
      wifi_.listen(idx);
      vTaskDelay(pdMS_TO_TICKS(params_.wifiDwellMs));
      wifi_.pause();
      idx++;
    } else if (bleOn) {  // BLE only: scan continuously; start() is a no-op while scanning
      startBle();
      vTaskDelay(pdMS_TO_TICKS(params_.bleSliceMs));
    } else if (wifiOn) {  // Wi-Fi only: stay in promiscuous mode, just rotate channels
      if (idx >= params_.channelCount) idx = 0;
      wifi_.listen(idx);
      vTaskDelay(pdMS_TO_TICKS(params_.wifiDwellMs));
      if (params_.channelCount > 1) idx++;
    } else {
      vTaskDelay(pdMS_TO_TICKS(IDLE_MS));
    }
  }
}

}  // namespace cc
