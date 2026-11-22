#pragma once
#include <stddef.h>
#include <stdint.h>

namespace cc {

// Zero memory that held a salt or a device address. The volatile pointer stops
// the compiler from dropping the writes as dead stores, which it otherwise does
// for a buffer that is about to go out of scope.
inline void secureZero(void* p, size_t n) {
  volatile uint8_t* b = static_cast<volatile uint8_t*>(p);
  while (n--) *b++ = 0;
}

}  // namespace cc
