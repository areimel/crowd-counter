#include "SettingsStore.h"

#include <stdio.h>

#include "Log.h"

namespace cc {
namespace {
constexpr const char* NS = "cc";
constexpr const char* KEY_SETTINGS = "settings";
constexpr const char* KEY_NIGHT = "night";

// "cal0".."cal15"; false if venue is out of range.
bool calKey(uint8_t venue, char (&key)[8]) {
  if (venue >= SettingsStore::VENUE_MAX) {
    LOG_W("store: venue %u out of range", venue);
    return false;
  }
  snprintf(key, sizeof(key), "cal%u", venue);
  return true;
}
}  // namespace

void SettingsStore::logWriteFail(const char* key) { LOG_E("store: write failed (%s)", key); }

bool SettingsStore::begin() {
  if (!prefs_.begin(NS, false)) {
    LOG_E("store: nvs open failed");
    return false;
  }
  return true;
}

Settings SettingsStore::loadSettings(const Settings& defaults) {
  Settings s = {};
  return getBlob(KEY_SETTINGS, s) ? s : defaults;
}

void SettingsStore::saveSettings(const Settings& s) { putBlob(KEY_SETTINGS, s); }

bool SettingsStore::loadCal(uint8_t venue, CalProfile& out) {
  char key[8];
  if (calKey(venue, key) && getBlob(key, out)) return true;
  memset(&out, 0, sizeof(out));
  return false;
}

void SettingsStore::saveCal(uint8_t venue, const CalProfile& p) {
  char key[8];
  if (calKey(venue, key)) putBlob(key, p);
}

NightMeta SettingsStore::loadNightMeta() {
  NightMeta m;
  return getBlob(KEY_NIGHT, m) ? m : NightMeta();
}

void SettingsStore::saveNightMeta(const NightMeta& m) { putBlob(KEY_NIGHT, m); }

}  // namespace cc
