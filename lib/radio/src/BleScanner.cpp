#include "BleScanner.h"

#include <NimBLEDevice.h>

#include "BleFingerprint.h"
#include "Log.h"

namespace cc {

namespace {

// Scan window == interval: the receiver listens for the whole BLE slice. The
// scheduler's slicing, not the duty cycle, shares the radio with Wi-Fi.
constexpr uint16_t SCAN_INTERVAL_MS = 100;
constexpr uint16_t SCAN_WINDOW_MS = 100;

// Public and static-random addresses are stable; resolvable and
// non-resolvable private addresses rotate. NimBLE stores the address
// little-endian, so the two type bits are the top of byte 5.
AddrKind addrKindOf(const NimBLEAddress& a) {
  if (a.getType() != BLE_ADDR_RANDOM) return AddrKind::FIXED;
  const bool staticRandom = (a.getVal()[5] & 0xC0) == 0xC0;
  return staticRandom ? AddrKind::FIXED : AddrKind::ROTATING;
}

class Callbacks : public NimBLEScanCallbacks {
 public:
  ObservationSink* sink = nullptr;

  // NimBLE host task (5 KB stack). Must not call start/stop or block.
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (sink == nullptr) return;
    sink->countBleAdvert();
    const int8_t rssi = static_cast<int8_t>(d->getRSSI());
    if (!sink->aboveFloor(rssi)) return;
    const std::vector<uint8_t>& payload = d->getPayload();
    const uint8_t markers = bleMarkers(payload.data(), payload.size());
    const NimBLEAddress& addr = d->getAddress();
    sink->submit(Signal::BLE, addr.getVal(), rssi, addrKindOf(addr), markers);
  }
};

Callbacks callbacks;

}  // namespace

bool BleScanner::begin(ObservationSink& sink) {
  callbacks.sink = &sink;
  if (!NimBLEDevice::init("")) {
    LOG_E("ble: init failed");
    return false;
  }
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&callbacks, true);  // true = keep duplicates (no filter)
  scan->setActiveScan(false);                // passive: never sends a scan request
  scan->setInterval(SCAN_INTERVAL_MS);
  scan->setWindow(SCAN_WINDOW_MS);
  scan->setMaxResults(0);  // callback only; nothing retained
  ready_ = true;
  return true;
}

bool BleScanner::start() {
  if (!ready_) return false;
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan->isScanning()) return true;
  return scan->start(0, false, true);
}

void BleScanner::stop() {
  if (!ready_) return;
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan->isScanning()) scan->stop();
}

}  // namespace cc
