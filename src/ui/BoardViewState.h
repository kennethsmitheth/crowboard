// View state of the board (what is highlighted) and the diff between two states. Plain C++17 with
// no Arduino includes so it compiles in the native test environment.
//
// Square conventions: `selected`, `lastFrom` and `lastTo` are 1-32 notation numbers with 0 for
// "none" (the same as BoardGeometry's cellAt). `legalTargets`, `forcedPieces` and `pendingPath`
// use 0-based square indices (bit i = square i; Move::path entries), as the game code does.
#pragma once

#include <stdint.h>

// Arduino.h defines a bit() macro that collides with checkers::bit() in Board.h.
#pragma push_macro("bit")
#undef bit
#include "../game/Board.h"
#pragma pop_macro("bit")

namespace checkers::ui {

struct BoardViewState {
  int selected = 0;           // 1-32 or 0
  uint32_t legalTargets = 0;  // bitmask of 0-based squares
  uint32_t forcedPieces = 0;  // 0-based squares of pieces that must capture
  int lastFrom = 0;           // 1-32 or 0
  int lastTo = 0;             // 1-32 or 0
  uint8_t pendingPath[10] = {};
  uint8_t pendingLen = 0;  // landings so far of a multi-jump in progress
  bool flipped = false;    // reserved; the view is drawn unflipped for now
};

namespace detail {
constexpr uint32_t notationMask(int n) {
  return (n >= 1 && n <= 32) ? (uint32_t{1} << (n - 1)) : 0;
}
inline uint32_t pendingMask(const BoardViewState& s) {
  uint32_t m = 0;
  for (int i = 0; i < s.pendingLen && i < 10; ++i) {
    if (s.pendingPath[i] < 32) m |= uint32_t{1} << s.pendingPath[i];
  }
  return m;
}
}  // namespace detail

// Squares (bit i = 0-based square i) whose appearance differs between the two (board, state)
// pairs. A null board counts as empty.
inline uint32_t changedSquares(const Board* oldBoard, const BoardViewState& oldState,
                               const Board* newBoard, const BoardViewState& newState) {
  uint32_t m = 0;
  const Board empty = Board::emptyBoard();
  const Board& a = oldBoard ? *oldBoard : empty;
  const Board& b = newBoard ? *newBoard : empty;
  for (int i = 0; i < 2; ++i) m |= (a.men[i] ^ b.men[i]) | (a.kings[i] ^ b.kings[i]);
  m |= detail::notationMask(oldState.selected) ^ detail::notationMask(newState.selected);
  m |= oldState.legalTargets ^ newState.legalTargets;
  m |= oldState.forcedPieces ^ newState.forcedPieces;
  m |= (detail::notationMask(oldState.lastFrom) | detail::notationMask(oldState.lastTo)) ^
       (detail::notationMask(newState.lastFrom) | detail::notationMask(newState.lastTo));
  m |= detail::pendingMask(oldState) ^ detail::pendingMask(newState);
  return m;
}

}  // namespace checkers::ui
