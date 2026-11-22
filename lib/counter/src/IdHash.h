#pragma once
// Salted device-id hash: first 8 bytes (big-endian) of
// SHA-256(salt[16] || signal byte || mac[6]), never 0.
#include <stdint.h>
#include "CounterTypes.h"
#include "SecureZero.h"
#include "Sha256.h"

namespace cc {

// 0 is reserved as "empty slot"; map it to 1.
inline uint64_t nonZeroId(uint64_t v) { return v == 0 ? 1 : v; }

// First 8 digest bytes, digest[0] most significant.
inline uint64_t digestToId(const uint8_t digest[32]) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v = (v << 8) | digest[i];
  return v;
}

inline uint64_t hashId(const uint8_t salt[16], Signal signal, const uint8_t mac[6]) {
  uint8_t msg[23];
  uint8_t digest[32];
  for (int i = 0; i < 16; i++) msg[i] = salt[i];
  msg[16] = static_cast<uint8_t>(signal);
  for (int i = 0; i < 6; i++) msg[17 + i] = mac[i];
  sha256(msg, sizeof msg, digest);
  const uint64_t id = nonZeroId(digestToId(digest));
  // sha256() wipes its own state; clear our copies of the address and digest.
  secureZero(msg, sizeof msg);
  secureZero(digest, sizeof digest);
  return id;
}

}  // namespace cc
