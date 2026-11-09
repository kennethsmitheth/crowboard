#pragma once
// Static evaluation (ARCHITECTURE §4). Pure C++17, no heap.
//
// evaluate() scores a position from the point of view of the side to move (positive = good for
// board.side). Both sides are scored with the same terms and subtracted, so
// evaluate(b) == -evaluate(mirror(b)) holds by construction.
//
// Terminal positions (no pieces / no moves) are the search's job; with no pieces on a side the
// material term still dominates, so the score stays sensible.
#include "game/Board.h"

namespace checkers {

struct EvalWeights {
  int man = 100;         // per man
  int king = 170;        // per king
  int advance = 4;       // per row advanced toward promotion, men only
  int backRank = 12;     // per own man still on its back rank (guards promotion)
  int centre = 6;        // per man or king on the 8 central squares
  int mobility = 2;      // per legal move of difference (side to move minus opponent)
  int trappedKing = 20;  // penalty per king that has no step and no jump

  static constexpr EvalWeights defaults() { return EvalWeights{}; }
};

// Score for board.side, in units where a man is `weights.man`.
// Mobility counts legalMoves() (forced capture on) for both sides with countLegalMoves(): a few
// bitboard operations per side, plus capture generation for a side that has a capture. With
// mobility == 0 no move counting happens at all.
int evaluate(const Board& b, const EvalWeights& weights = EvalWeights{});

// Colour-swapped, board-rotated copy: every piece changes colour and moves to the point-symmetric
// square (index s -> 31 - s, i.e. row 7 - r, col 7 - c); kings stay kings; the side to move is
// kept (the same side is to move, now with the other colour), which negates evaluate().
// Test/tuning helper.
Board mirror(const Board& b);

}  // namespace checkers
