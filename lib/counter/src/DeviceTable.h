#pragma once
#include <stddef.h>
#include <stdint.h>

#include "CounterTypes.h"

namespace cc {

// 64-bit finaliser (murmur3 fmix64). Ids are already hashes; this just makes the
// slot index independent of how they were produced.
inline uint64_t mixId(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}

// Open-addressing hash table of devices over caller-supplied storage (linear
// probing, backward-shift deletion, so no tombstones). Never allocates.
// DeviceEntry::state: 0 = free slot, 1 = live entry.
class DeviceTable {
 public:
  // slotCount must be a power of two (>= 2). A null table or fewer than 2 slots
  // is inert: size() is 0, observe() is ignored, expire() removes nothing.
  DeviceTable(DeviceEntry* slots, uint32_t slotCount, uint32_t maxEntries)
      : slots_(slots), mask_(0), slotCount_(slotCount), max_(maxEntries ? maxEntries : 1) {
    if (slots_ == nullptr || slotCount_ < 2) {
      slots_ = nullptr;
      slotCount_ = 0;
      max_ = 0;
      return;
    }
    mask_ = slotCount_ - 1;
    if (max_ >= slotCount_) max_ = slotCount_ - slotCount_ / 8;
    if (max_ >= slotCount_) max_ = slotCount_ - 1;  // always keep a free slot
    if (max_ == 0) max_ = 1;
    clear();
  }

  void clear() {
    for (uint32_t i = 0; i < slotCount_; ++i) {
      slots_[i] = DeviceEntry{};
    }
    size_ = 0;
    evictions_ = 0;
  }

  void observe(const Observation& o, uint32_t nowS) {
    if (o.id == 0 || slots_ == nullptr) return;
    uint32_t i = find(o.id);
    if (i != kNone) {
      DeviceEntry& e = slots_[i];
      e.lastSeenS = nowS;
      e.markers |= o.markers;
      const int fresh = o.rssi;
      // Floor, not truncate: with negative dBm, truncation rounds toward the
      // stronger value and the decay stalls up to 3 dB short of the real level.
      const int smoothed = (3 * static_cast<int>(e.rssi) + fresh) >> 2;
      e.rssi = static_cast<int8_t>(fresh > smoothed ? fresh : smoothed);
      return;
    }
    if (size_ >= max_) evictOldest();
    i = home(o.id);
    while (slots_[i].state != kFree) i = (i + 1) & mask_;
    DeviceEntry& e = slots_[i];
    e.id = o.id;
    e.firstSeenS = nowS;
    e.lastSeenS = nowS;
    e.rssi = o.rssi;
    e.signal = o.signal;
    e.addrKind = o.addrKind;
    e.markers = o.markers;
    e.state = kUsed;
    for (uint8_t k = 0; k < 3; ++k) e.reserved[k] = 0;
    ++size_;
  }

  // Removes entries with nowS - lastSeenS > maxAgeS. Returns how many.
  uint32_t expire(uint32_t nowS, uint32_t maxAgeS) {
    if (slots_ == nullptr) return 0;
    return sweep([&](const DeviceEntry& e) { return nowS - e.lastSeenS > maxAgeS; });
  }

  uint32_t size() const { return size_; }
  uint32_t evictions() const { return evictions_; }

  template <class F>
  void forEach(F f) const {
    for (uint32_t i = 0; i < slotCount_; ++i)
      if (slots_[i].state == kUsed) f(static_cast<const DeviceEntry&>(slots_[i]));
  }

 private:
  static constexpr uint8_t kFree = 0, kUsed = 1;
  static constexpr uint32_t kNone = 0xFFFFFFFFu;
  static constexpr uint32_t kBuckets = 64;

  uint32_t home(uint64_t id) const { return static_cast<uint32_t>(mixId(id)) & mask_; }

  uint32_t find(uint64_t id) const {
    for (uint32_t i = home(id); slots_[i].state != kFree; i = (i + 1) & mask_)
      if (slots_[i].id == id) return i;
    return kNone;
  }

  // Delete slot i, then pull later members of its probe cluster back so every
  // remaining entry stays reachable from its home slot.
  void removeAt(uint32_t i) {
    uint32_t j = i;
    for (;;) {
      j = (j + 1) & mask_;
      if (slots_[j].state == kFree) break;
      const uint32_t k = home(slots_[j].id);
      // Entry at j may move to i unless its home k lies cyclically in (i, j].
      const bool homeBetween = (i <= j) ? (i < k && k <= j) : (i < k || k <= j);
      if (homeBetween) continue;
      slots_[i] = slots_[j];
      i = j;
    }
    slots_[i].id = 0;
    slots_[i].state = kFree;
    --size_;
  }

  // Removes every entry for which pred(entry) is true (pred is called once per
  // distinct entry). Starts just after a free slot so no cluster straddles the
  // start: a backward shift then never moves an unvisited entry behind us.
  template <class P>
  uint32_t sweep(P pred) {
    uint32_t start = 0;
    while (slots_[start].state != kFree) ++start;  // exists: max_ < slotCount_
    uint32_t removed = 0;
    for (uint32_t n = 1; n <= slotCount_; ++n) {
      const uint32_t i = (start + n) & mask_;
      while (slots_[i].state == kUsed && pred(static_cast<const DeviceEntry&>(slots_[i]))) {
        removeAt(i);  // slot i now holds a different, unexamined entry (or is free)
        ++removed;
      }
    }
    return removed;
  }

  // Drop the batch (max/16, at least 1) least recently seen entries: min/max
  // pass, 64-bucket histogram over [min,max] to find the cutoff bucket, then one
  // deletion sweep (all older buckets, plus just enough of the cutoff bucket).
  void evictOldest() {
    uint32_t batch = max_ / 16;
    if (batch == 0) batch = 1;
    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    forEach([&](const DeviceEntry& e) {
      if (e.lastSeenS < lo) lo = e.lastSeenS;
      if (e.lastSeenS > hi) hi = e.lastSeenS;
    });
    const uint64_t width = static_cast<uint64_t>(hi) - lo + 1;
    auto bucketOf = [&](uint32_t t) { return static_cast<uint32_t>(static_cast<uint64_t>(t - lo) * kBuckets / width); };
    uint32_t hist[kBuckets] = {};
    forEach([&](const DeviceEntry& e) { ++hist[bucketOf(e.lastSeenS)]; });
    uint32_t below = 0, cut = 0;
    while (cut < kBuckets - 1 && below + hist[cut] < batch) below += hist[cut++];
    uint32_t quota = batch - below;  // taken from the cutoff bucket
    evictions_ += sweep([&](const DeviceEntry& e) {
      const uint32_t b = bucketOf(e.lastSeenS);
      if (b < cut) return true;
      if (b == cut && quota > 0) {
        --quota;
        return true;
      }
      return false;
    });
  }

  DeviceEntry* slots_;
  uint32_t mask_, slotCount_, max_;
  uint32_t size_ = 0, evictions_ = 0;
};

}  // namespace cc
