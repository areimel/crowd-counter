#pragma once
#include <Preferences.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

#include "CounterTypes.h"

namespace cc {

// Needed for a cold resume (power lost mid-night).
struct NightMeta {
  uint32_t nextNightId = 1;
  uint32_t openNightId = 0;  // 0 = none open
  Baseline baseline = {};    // valid=false, zeros
  uint32_t startEpoch = 0;   // 0 = unknown
};

// NVS-backed settings, one blob per struct. Writes are skipped when the stored
// bytes are identical (flash wear). Callers should zero-initialise structs
// before filling them (e.g. `CalProfile p = {};` or memset) so padding bytes
// are deterministic and do not defeat the unchanged-write check. A stored blob
// whose size differs from the struct (layout changed) counts as "not stored".
class SettingsStore {
 public:
  static constexpr uint8_t VENUE_MAX = 16;

  bool begin();
  Settings loadSettings(const Settings& defaults);
  void saveSettings(const Settings& s);
  bool loadCal(uint8_t venue, CalProfile& out);  // false (out zeroed) if none stored
  void saveCal(uint8_t venue, const CalProfile& p);
  NightMeta loadNightMeta();
  void saveNightMeta(const NightMeta& m);

 private:
  template <typename T>
  bool getBlob(const char* key, T& out) {
    static_assert(std::is_trivially_copyable<T>::value, "blob types must be trivially copyable");
    if (prefs_.getBytesLength(key) != sizeof(T)) return false;
    return prefs_.getBytes(key, &out, sizeof(T)) == sizeof(T);
  }

  template <typename T>
  void putBlob(const char* key, const T& v) {
    static_assert(std::is_trivially_copyable<T>::value, "blob types must be trivially copyable");
    if (prefs_.getBytesLength(key) == sizeof(T)) {
      T cur;
      if (prefs_.getBytes(key, &cur, sizeof(T)) == sizeof(T) && memcmp(&cur, &v, sizeof(T)) == 0)
        return;  // unchanged
    }
    if (prefs_.putBytes(key, &v, sizeof(T)) != sizeof(T)) logWriteFail(key);
  }

  static void logWriteFail(const char* key);

  Preferences prefs_;
};

}  // namespace cc
