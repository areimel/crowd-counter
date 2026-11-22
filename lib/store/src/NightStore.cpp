#include "NightStore.h"

#include <FS.h>
#include <LittleFS.h>

#include "Log.h"

namespace cc {

namespace {

constexpr const char* kMountPoint = "/littlefs";
constexpr const char* kPartition = "spiffs";
constexpr const char* kDir = "/nights";
constexpr const char* kExt = ".log";
constexpr uint8_t kMaxOpenFiles = 5;

// The one place that builds a night file path.
void pathFor(uint32_t id, char* out, size_t n) {
  snprintf(out, n, "%s/%08lu%s", kDir, static_cast<unsigned long>(id), kExt);
}

// The one place that parses a file name (with or without directory) to an id.
// Returns 0 when the name is not a night file.
uint32_t parseId(const char* name) {
  if (const char* slash = strrchr(name, '/')) name = slash + 1;
  uint64_t v = 0;
  size_t digits = 0;
  while (name[digits] >= '0' && name[digits] <= '9') {
    v = v * 10 + static_cast<uint64_t>(name[digits] - '0');
    if (++digits > 10 || v > UINT32_MAX) return 0;
  }
  if (digits == 0 || strcmp(name + digits, kExt) != 0) return 0;
  return static_cast<uint32_t>(v);
}

bool writeWithNewline(File& f, const char* text) {
  size_t len = strlen(text);
  if (len && f.write(reinterpret_cast<const uint8_t*>(text), len) != len) return false;
  if (len && text[len - 1] == '\n') return true;
  return f.write('\n') == 1;
}

}  // namespace

bool NightStore::begin() {
  ready_ = false;
  if (!LittleFS.begin(true, kMountPoint, kMaxOpenFiles, kPartition)) {
    LOG_E("night store: mount/format failed");
    return false;
  }
  if (!LittleFS.exists(kDir) && !LittleFS.mkdir(kDir)) {
    LOG_E("night store: cannot create %s", kDir);
    return false;
  }
  ready_ = true;
  LOG_I("night store: mounted, %u of %u bytes used", static_cast<unsigned>(usedBytes()),
        static_cast<unsigned>(totalBytes()));
  return true;
}

bool NightStore::exists(uint32_t nightId) const {
  if (!ready_ || nightId == 0) return false;
  char path[40];
  pathFor(nightId, path, sizeof path);
  return LittleFS.exists(path);
}

bool NightStore::create(uint32_t nightId, const char* headerText) {
  if (!ready_ || nightId == 0 || !headerText || exists(nightId)) return false;
  char path[40];
  pathFor(nightId, path, sizeof path);
  File f = LittleFS.open(path, FILE_WRITE);
  if (!f) {
    LOG_E("night store: create failed");
    return false;
  }
  bool ok = writeWithNewline(f, headerText);
  f.close();
  if (!ok) {
    LOG_E("night store: header write failed");
    LittleFS.remove(path);
  }
  return ok;
}

bool NightStore::append(uint32_t nightId, const char* line) {
  if (!ready_ || nightId == 0 || !line || !exists(nightId)) return false;
  char path[40];
  pathFor(nightId, path, sizeof path);
  File f = LittleFS.open(path, FILE_APPEND);
  if (!f) {
    LOG_E("night store: append open failed");
    return false;
  }
  bool ok = writeWithNewline(f, line);
  f.close();
  if (!ok) LOG_W("night store: append write failed (full?)");
  return ok;
}

void NightStore::scan(IdFn fn, void* ctx) const {
  if (!ready_) return;
  File dir = LittleFS.open(kDir);
  if (!dir || !dir.isDirectory()) return;
  for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    uint32_t id = e.isDirectory() ? 0 : parseId(e.name());
    if (id) fn(id, ctx);
  }
}

size_t NightStore::list(uint32_t* ids, size_t max) const {
  if (!ids || max == 0) return 0;
  struct Acc {
    uint32_t* ids;
    size_t max, n;
  } acc{ids, max, 0};
  scan(
      [](uint32_t id, void* c) {
        auto& a = *static_cast<Acc*>(c);
        if (a.n == a.max && id <= a.ids[a.n - 1]) return;  // full and not newer than the smallest kept
        size_t i = (a.n < a.max) ? a.n++ : a.n - 1;        // slot to fill, dropping the smallest if full
        while (i > 0 && a.ids[i - 1] < id) {
          a.ids[i] = a.ids[i - 1];
          --i;
        }
        a.ids[i] = id;
      },
      &acc);
  return acc.n;
}

uint32_t NightStore::newestId() const {
  uint32_t id = 0;
  list(&id, 1);
  return id;
}

bool NightStore::forEachLine(uint32_t nightId, LineFn fn, void* ctx) const {
  if (!ready_ || nightId == 0 || !fn) return false;
  char path[40];
  pathFor(nightId, path, sizeof path);
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return false;
  char line[LINE_BUF];
  size_t len = 0;
  uint8_t chunk[64];
  int got;
  while ((got = f.read(chunk, sizeof chunk)) > 0) {
    for (int i = 0; i < got; ++i) {
      char c = static_cast<char>(chunk[i]);
      if (c == '\n') {
        line[len] = '\0';
        if (!fn(line, ctx)) return true;
        len = 0;
      } else if (c != '\r' && len < LINE_BUF - 1) {
        line[len++] = c;  // excess characters are dropped (truncated line)
      }
    }
  }
  if (len) {  // final line without a newline
    line[len] = '\0';
    fn(line, ctx);
  }
  return true;
}

bool NightStore::removeOldest(uint32_t protectId) {
  struct Acc {
    uint32_t protectId, oldest;
  } acc{protectId, 0};
  scan(
      [](uint32_t id, void* c) {
        auto& a = *static_cast<Acc*>(c);
        if (id != a.protectId && (a.oldest == 0 || id < a.oldest)) a.oldest = id;
      },
      &acc);
  if (acc.oldest == 0) return false;
  char path[40];
  pathFor(acc.oldest, path, sizeof path);
  if (!LittleFS.remove(path)) {
    LOG_W("night store: delete failed");
    return false;
  }
  return true;
}

size_t NightStore::prune(size_t keepCount, size_t minFreeBytes, uint32_t protectId) {
  if (!ready_) return 0;
  size_t deleted = 0;
  for (;;) {
    size_t count = 0;
    scan([](uint32_t, void* c) { ++*static_cast<size_t*>(c); }, &count);
    bool tooMany = count > keepCount;
    bool tooFull = totalBytes() - usedBytes() < minFreeBytes;
    if ((!tooMany && !tooFull) || !removeOldest(protectId)) break;
    ++deleted;
  }
  if (deleted) LOG_I("night store: pruned %u night file(s)", static_cast<unsigned>(deleted));
  return deleted;
}

size_t NightStore::usedBytes() const { return ready_ ? LittleFS.usedBytes() : 0; }
size_t NightStore::totalBytes() const { return ready_ ? LittleFS.totalBytes() : 0; }

}  // namespace cc
