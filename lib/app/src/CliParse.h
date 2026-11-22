#pragma once
#include <stddef.h>
#include <stdint.h>

// Pure serial-CLI parsing. No Arduino includes.
namespace cc {

enum class CliCommand : uint8_t { EMPTY = 0, UNKNOWN, HELP, STATUS, NIGHTS, EXPORT, SUMMARY, TIME, SHOT };

struct CliRequest {
  CliCommand command;
  uint32_t arg;
  bool hasArg;
};

inline const char* cliCommandName(CliCommand c) {
  switch (c) {
    case CliCommand::HELP: return "help";
    case CliCommand::STATUS: return "status";
    case CliCommand::NIGHTS: return "nights";
    case CliCommand::EXPORT: return "export";
    case CliCommand::SUMMARY: return "summary";
    case CliCommand::TIME: return "time";
    case CliCommand::SHOT: return "shot";
    case CliCommand::EMPTY: return "";
    default: return "unknown";
  }
}

namespace cli_detail {

inline bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
inline char lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; }

// Advances p past one token; token is [start, end).
inline bool nextToken(const char*& p, const char*& start, const char*& end) {
  while (*p && isSpace(*p)) ++p;
  if (!*p) return false;
  start = p;
  while (*p && !isSpace(*p)) ++p;
  end = p;
  return true;
}

inline bool wordIs(const char* s, const char* e, const char* word) {
  while (s < e && *word) {
    if (lower(*s) != *word) return false;
    ++s;
    ++word;
  }
  return s == e && *word == '\0';
}

inline bool parseU32(const char* s, const char* e, uint32_t& out) {
  if (s == e) return false;
  uint64_t v = 0;
  for (; s < e; ++s) {
    if (*s < '0' || *s > '9') return false;
    v = v * 10 + uint64_t(*s - '0');
    if (v > 0xFFFFFFFFull) return false;
  }
  out = uint32_t(v);
  return true;
}

}  // namespace cli_detail

inline CliRequest parseCli(const char* line) {
  using namespace cli_detail;
  const CliRequest unknown{CliCommand::UNKNOWN, 0, false};
  if (!line) return CliRequest{CliCommand::EMPTY, 0, false};
  const char* p = line;
  const char *s, *e;
  if (!nextToken(p, s, e)) return CliRequest{CliCommand::EMPTY, 0, false};

  struct Entry { const char* word; CliCommand cmd; int arg; };  // arg: 0 none, 1 optional, 2 required
  static const Entry kTable[] = {
      {"help", CliCommand::HELP, 0},       {"status", CliCommand::STATUS, 0},
      {"nights", CliCommand::NIGHTS, 0},   {"shot", CliCommand::SHOT, 0},
      {"export", CliCommand::EXPORT, 1},   {"summary", CliCommand::SUMMARY, 1},
      {"time", CliCommand::TIME, 2},
  };
  for (const Entry& en : kTable) {
    if (!wordIs(s, e, en.word)) continue;
    const char *as, *ae;
    bool has = nextToken(p, as, ae);
    uint32_t v = 0;
    if (has) {
      if (en.arg == 0 || !parseU32(as, ae, v)) return unknown;
      const char *xs, *xe;
      if (nextToken(p, xs, xe)) return unknown;
    } else if (en.arg == 2) {
      return unknown;
    }
    return CliRequest{en.cmd, v, has};
  }
  return unknown;
}

// Collects bytes into lines. Lines longer than N-1 chars are dropped whole.
template <size_t N>
class LineAccumulator {
  static_assert(N >= 2, "buffer too small");

 public:
  // True when a non-empty complete line is ready in line().
  bool feed(char c) {
    if (c == '\0') return false;
    if (c == '\r' || c == '\n') {
      bool ready = !overflow_ && len_ > 0;
      if (ready) buf_[len_] = '\0';
      len_ = 0;
      overflow_ = false;
      return ready;
    }
    if (overflow_) return false;
    if (len_ >= N - 1) {
      overflow_ = true;
      len_ = 0;
      return false;
    }
    buf_[len_++] = c;
    return false;
  }

  const char* line() const { return buf_; }

 private:
  char buf_[N] = {0};
  size_t len_ = 0;
  bool overflow_ = false;
};

}  // namespace cc
