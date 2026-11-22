#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cc {

struct ProbeInfo {
  uint8_t mac[6];
  bool randomized;  // locally administered bit set: a rotating address
};

// Accepts only an 802.11 management probe request (frame control byte 0 ==
// 0x40) with a full 24-byte MAC header. The transmitter (addr2) is bytes
// 10..15. Leaves `out` untouched when it returns false.
inline bool parseProbeRequest(const uint8_t* frame, size_t len, ProbeInfo& out) {
  if (frame == nullptr || len < 24 || frame[0] != 0x40) return false;
  memcpy(out.mac, frame + 10, 6);
  out.randomized = (out.mac[0] & 0x02) != 0;
  return true;
}

}  // namespace cc
