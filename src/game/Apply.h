#pragma once
// Applying a move to a board. Pure C++17, no heap. No legality check: `m` must come from
// legalMoves() (or captureMoves()/simpleMoves()) for the same board. Issue #11 (Game) layers
// history and undo on top of this.
#include "game/Board.h"
#include "game/Move.h"

namespace checkers {

// In place: removes the enemy pieces (men or kings) on m.captured, moves the piece from m.from
// to m.to (from == to is fine for a king that loops back), crowns it if m.promotes, and hands
// the turn to the other side.
void apply(Board& b, const Move& m);

// Returns a copy of b with m applied.
inline Board applyMove(const Board& b, const Move& m) {
  Board r = b;
  apply(r, m);
  return r;
}

}  // namespace checkers
