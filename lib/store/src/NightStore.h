#pragma once
#include <Arduino.h>

#include <type_traits>

namespace cc {

// Night files on LittleFS (partition "spiffs"): one text file per night, named
// by a monotonic night id. Format-agnostic: it stores and returns lines only.
// Ids must be non-zero; 0 means "none". Every call fails cleanly (false/0)
// until begin() has succeeded.
class NightStore {
 public:
  static constexpr size_t LINE_BUF = 256;  // read buffer incl. NUL; longer lines are delivered truncated
  using LineFn = bool (*)(const char* line, void* ctx);  // return false to stop

  bool begin();                                           // mount (format on failure), ensure dir
  bool create(uint32_t nightId, const char* headerText);  // false if the file already exists
  bool append(uint32_t nightId, const char* line);        // open-append-close; false if no such night
  bool exists(uint32_t nightId) const;
  size_t list(uint32_t* ids, size_t max) const;  // newest (highest id) first; returns count written
  uint32_t newestId() const;                     // 0 if none
  bool forEachLine(uint32_t nightId, LineFn fn, void* ctx) const;  // strips \r\n
  template <typename F>
  bool forEachLine(uint32_t nightId, F&& f) const {
    using Fn = std::remove_reference_t<F>;
    auto tramp = [](const char* line, void* ctx) -> bool { return (*static_cast<Fn*>(ctx))(line); };
    return forEachLine(nightId, static_cast<LineFn>(tramp), const_cast<void*>(static_cast<const void*>(&f)));
  }
  // Delete oldest files beyond keepCount, then keep deleting the oldest (never
  // protectId) while free space < minFreeBytes. Returns files deleted.
  size_t prune(size_t keepCount, size_t minFreeBytes, uint32_t protectId);
  size_t usedBytes() const;
  size_t totalBytes() const;

 private:
  using IdFn = void (*)(uint32_t id, void* ctx);
  void scan(IdFn fn, void* ctx) const;
  bool removeOldest(uint32_t protectId);
  bool ready_ = false;
};

}  // namespace cc
