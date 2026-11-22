#include "RtcState.h"

#include <esp_system.h>
#include <esp_random.h>
#include <esp_rom_crc.h>
#include <string.h>

#include "Log.h"

namespace cc {
namespace rtc {
namespace {

constexpr uint32_t MAGIC = 0x434e5354;  // "CNST"
constexpr uint16_t VERSION = 1;

struct Record {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  NightState state;
  uint32_t crc;
};

RTC_NOINIT_ATTR Record g_rec;

uint32_t crcOf(const Record& r) {
  return esp_rom_crc32_le(0, reinterpret_cast<const uint8_t*>(&r), offsetof(Record, crc));
}

// Volatile so the compiler cannot drop a wipe of a dead stack copy.
void wipeBytes(void* p, size_t n) {
  volatile uint8_t* v = static_cast<volatile uint8_t*>(p);
  for (size_t i = 0; i < n; i++) v[i] = 0;
}

void wipe() { wipeBytes(&g_rec, sizeof(g_rec)); }

}  // namespace

bool load(NightState& out) {
  if (esp_reset_reason() == ESP_RST_POWERON) {
    wipe();
    return false;
  }
  Record r;
  memcpy(&r, &g_rec, sizeof(r));
  if (r.magic != MAGIC || r.version != VERSION || r.size != sizeof(NightState) ||
      r.crc != crcOf(r)) {
    LOG_W("rtc: no valid night state");
    wipeBytes(&r, sizeof(r));
    return false;
  }
  out = r.state;
  wipeBytes(&r, sizeof(r));  // the salt stays only in RTC memory and the caller's copy
  return true;
}

void save(const NightState& s) {
  Record r;
  memset(&r, 0, sizeof(r));  // deterministic padding for the CRC
  r.magic = MAGIC;
  r.version = VERSION;
  r.size = sizeof(NightState);
  r.state = s;
  r.crc = crcOf(r);
  memcpy(&g_rec, &r, sizeof(r));
  wipeBytes(&r, sizeof(r));
}

void clear() { wipe(); }

void freshSalt(uint8_t out[16]) { esp_fill_random(out, 16); }

int resetReason() { return static_cast<int>(esp_reset_reason()); }

const char* resetReasonName() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    case ESP_RST_EFUSE: return "EFUSE";
    case ESP_RST_PWR_GLITCH: return "PWR_GLITCH";
    case ESP_RST_CPU_LOCKUP: return "CPU_LOCKUP";
    default: return "UNKNOWN";
  }
}

}  // namespace rtc
}  // namespace cc
