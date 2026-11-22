#pragma once
#include <stddef.h>
#include <stdint.h>

#include "CounterTypes.h"

namespace cc {

// Coarse class from accumulated marker bits.
// Priority: TRACKER > AUDIO > WEARABLE > COMPUTER > BEACON > PHONE > OTHER.
inline DeviceClass classOf(uint8_t markers) {
  if (markers & marker::TRACKER) return DeviceClass::TRACKER;
  if (markers & marker::AUDIO) return DeviceClass::AUDIO;
  if (markers & marker::WEARABLE) return DeviceClass::WEARABLE;
  if (markers & marker::COMPUTER) return DeviceClass::COMPUTER;
  if (markers & marker::BEACON) return DeviceClass::BEACON;
  if (markers & marker::PHONE) return DeviceClass::PHONE;
  return DeviceClass::OTHER;
}

// ---- BLE advert -> marker bits ----------------------------------------------
//
// Rules (AD structure = [len][type][len-1 data bytes]):
//
//   AD 0xFF manufacturer data, company id little-endian in the first 2 bytes
//     Apple 0x004C: walk every [type][len][value] Continuity entry
//       0x10 Nearby Info, 0x0C Handoff, 0x0F Nearby Action, 0x05 AirDrop -> PHONE
//       0x07 Proximity Pairing                                          -> AUDIO
//       0x12 Find My                                                    -> TRACKER
//       0x0B Magic Switch                                               -> WEARABLE
//       0x02 iBeacon, 0x09 / 0x0A AirPlay                               -> BEACON
//       any other type                                                  -> nothing
//     Microsoft 0x0006, first data byte 0x01 (Connected Devices)        -> COMPUTER
//     Samsung 0x0075 and every other company                            -> nothing
//   AD 0x02 / 0x03 16-bit service UUID lists (little-endian, several allowed)
//     0xFEED, 0xFEEC (Tile), 0xFD5A (Samsung SmartTag)                  -> TRACKER
//     0xFEAA (Eddystone)                                                -> BEACON
//   AD 0x16 service data (16-bit UUID little-endian first)
//     0xFE2C (Google Fast Pair)                                         -> AUDIO
//     0xFEAA                                                            -> BEACON
//     tracker UUIDs above                                               -> TRACKER
//
// CAVEAT: the design review could only partly verify these identifiers against
// public sources. Treat them as best-effort heuristics, not a specification.
// Ambiguous vendors (Samsung, unrecognised Apple entry types) deliberately
// produce no marker: a wrong class is worse than OTHER. Nothing is logged and
// no part of the payload is stored; only the OR'd bits leave this function.

namespace detail {

inline uint8_t trackerOrBeaconUuid(uint16_t uuid) {
  switch (uuid) {
    case 0xFEED:
    case 0xFEEC:
    case 0xFD5A: return marker::TRACKER;
    case 0xFEAA: return marker::BEACON;
    default: return 0;
  }
}

inline uint8_t appleEntryMarker(uint8_t type) {
  switch (type) {
    case 0x10:
    case 0x0C:
    case 0x0F:
    case 0x05: return marker::PHONE;
    case 0x07: return marker::AUDIO;
    case 0x12: return marker::TRACKER;
    case 0x0B: return marker::WEARABLE;
    case 0x02:
    case 0x09:
    case 0x0A: return marker::BEACON;
    default: return 0;
  }
}

// `d` points at the AD data (after the type byte), `n` bytes long.
inline uint8_t manufacturerMarkers(const uint8_t* d, size_t n) {
  if (n < 2) return 0;
  uint16_t company = (uint16_t)(d[0] | (d[1] << 8));
  d += 2;
  n -= 2;
  if (company == 0x0006) return (n >= 1 && d[0] == 0x01) ? marker::COMPUTER : 0;
  if (company != 0x004C) return 0;
  uint8_t bits = 0;
  size_t i = 0;
  while (i + 2 <= n) {
    size_t entryLen = d[i + 1];
    if (i + 2 + entryLen > n) break;
    bits |= appleEntryMarker(d[i]);
    i += 2 + entryLen;
  }
  return bits;
}

}  // namespace detail

// Safe on any input; never reads at or past payload[len].
inline uint8_t bleMarkers(const uint8_t* payload, size_t len) {
  if (payload == nullptr) return 0;
  uint8_t bits = 0;
  size_t pos = 0;
  while (pos < len) {
    size_t adLen = payload[pos];
    if (adLen == 0 || pos + 1 + adLen > len) break;
    uint8_t type = payload[pos + 1];
    const uint8_t* d = payload + pos + 2;
    size_t n = adLen - 1;
    if (type == 0xFF) {
      bits |= detail::manufacturerMarkers(d, n);
    } else if (type == 0x02 || type == 0x03) {
      for (size_t i = 0; i + 2 <= n; i += 2)
        bits |= detail::trackerOrBeaconUuid((uint16_t)(d[i] | (d[i + 1] << 8)));
    } else if (type == 0x16 && n >= 2) {
      uint16_t uuid = (uint16_t)(d[0] | (d[1] << 8));
      bits |= uuid == 0xFE2C ? marker::AUDIO : detail::trackerOrBeaconUuid(uuid);
    }
    pos += 1 + adLen;
  }
  return bits;
}

}  // namespace cc
