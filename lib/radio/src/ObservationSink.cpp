#include "ObservationSink.h"

#include <string.h>

#include "IdHash.h"

namespace cc {

namespace {
void wipe(uint8_t* p, size_t n) {
  volatile uint8_t* v = p;
  for (size_t i = 0; i < n; i++) v[i] = 0;
}
}  // namespace

bool ObservationSink::begin(int8_t rssiFloor) {
  rssiFloor_ = rssiFloor;
  if (queue_ == nullptr) queue_ = xQueueCreate(QUEUE_LEN, sizeof(Observation));
  return queue_ != nullptr;
}

void ObservationSink::setSalt(const uint8_t salt[16]) {
  portENTER_CRITICAL(&lock_);
  memcpy(salt_, salt, sizeof salt_);
  saltSet_ = true;
  portEXIT_CRITICAL(&lock_);
}

bool ObservationSink::copySalt(uint8_t out[16]) {
  portENTER_CRITICAL(&lock_);
  const bool ok = saltSet_;
  if (ok) memcpy(out, salt_, sizeof salt_);
  portEXIT_CRITICAL(&lock_);
  return ok;
}

void ObservationSink::submit(Signal signal, const uint8_t mac[6], int8_t rssi, AddrKind kind, uint8_t markers) {
  if (queue_ == nullptr || !aboveFloor(rssi)) return;
  uint8_t salt[16];
  if (!copySalt(salt)) return;
  Observation o = {};
  o.id = hashId(salt, signal, mac);
  wipe(salt, sizeof salt);
  o.rssi = rssi;
  o.signal = signal;
  o.addrKind = kind;
  o.markers = signal == Signal::BLE ? markers : 0;
  if (xQueueSend(queue_, &o, 0) != pdTRUE) bump(stats_.queueDrops);
}

bool ObservationSink::poll(Observation& out) {
  return queue_ != nullptr && xQueueReceive(queue_, &out, 0) == pdTRUE;
}

RadioStats ObservationSink::stats() const {
  RadioStats s;
  s.bleAdverts = __atomic_load_n(&stats_.bleAdverts, __ATOMIC_RELAXED);
  s.wifiFrames = __atomic_load_n(&stats_.wifiFrames, __ATOMIC_RELAXED);
  for (uint8_t i = 0; i < WIFI_CHANNELS_MAX; i++) s.wifiProbes[i] = __atomic_load_n(&stats_.wifiProbes[i], __ATOMIC_RELAXED);
  s.queueDrops = __atomic_load_n(&stats_.queueDrops, __ATOMIC_RELAXED);
  return s;
}

}  // namespace cc
