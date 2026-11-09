#pragma once
// A single move: a simple step or a (multi-)jump. Trivially copyable POD, no heap
// (ARCHITECTURE §6: the full landing path is kept so the UI can animate and so jump sequences
// with equal from/to but different paths stay distinct).
//
// * from, to:  0-based Square indices (see Board.h).
// * captured:  bitboard of the enemy squares jumped; 0 for a simple move.
// * path:      landing squares in order; path[pathLen - 1] == to. A simple move has pathLen == 1
//              and path[0] == to. Generators must not exceed kMaxPathLen landings.
// * promotes:  a man ends the move on its promotion row (and becomes a king).
#include <cstdint>

namespace checkers {

constexpr int kMaxPathLen = 10;

struct Move {
  uint8_t from;
  uint8_t to;
  uint32_t captured;
  uint8_t path[kMaxPathLen];
  uint8_t pathLen;
  bool promotes;

  static Move simple(int from, int to, bool promotes) {
    Move m{};
    m.from = static_cast<uint8_t>(from);
    m.to = static_cast<uint8_t>(to);
    m.captured = 0;
    m.path[0] = static_cast<uint8_t>(to);
    m.pathLen = 1;
    m.promotes = promotes;
    return m;
  }

  bool isCapture() const { return captured != 0; }
  int captureCount() const {
    int n = 0;
    for (uint32_t x = captured; x != 0; x &= x - 1) ++n;
    return n;
  }

  bool operator==(const Move& o) const {
    if (from != o.from || to != o.to || captured != o.captured || pathLen != o.pathLen ||
        promotes != o.promotes) {
      return false;
    }
    for (int i = 0; i < pathLen && i < kMaxPathLen; ++i) {
      if (path[i] != o.path[i]) return false;
    }
    return true;
  }
  bool operator!=(const Move& o) const { return !(*this == o); }
};

}  // namespace checkers
