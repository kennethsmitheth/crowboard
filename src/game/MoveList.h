#pragma once
// Fixed-capacity move list (no heap). Pushing when full is ignored and sets the overflow flag.
#include <cstddef>

#include "game/Move.h"

namespace checkers {

class MoveList {
 public:
  static constexpr size_t kCapacity = 64;

  void push(const Move& m) {
    if (count_ >= kCapacity) {
      overflow_ = true;
      return;
    }
    moves_[count_++] = m;
  }
  size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }
  bool full() const { return count_ >= kCapacity; }
  bool overflowed() const { return overflow_; }
  void clear() {
    count_ = 0;
    overflow_ = false;
  }

  Move& operator[](size_t i) { return moves_[i]; }
  const Move& operator[](size_t i) const { return moves_[i]; }
  Move* begin() { return moves_; }
  Move* end() { return moves_ + count_; }
  const Move* begin() const { return moves_; }
  const Move* end() const { return moves_ + count_; }

 private:
  Move moves_[kCapacity];
  size_t count_ = 0;
  bool overflow_ = false;
};

}  // namespace checkers
