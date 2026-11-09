#include "game/MoveGen.h"

namespace checkers {

void simpleMoves(const Board& b, MoveList& out) {
  const Side me = b.side;
  const int s = sideIndex(me);
  const uint32_t empty = b.empty();
  // Forward directions: Black toward row 0 (NW, NE); White toward row 7 (SW, SE).
  const int firstFwd = (me == Side::Black) ? NW : SW;
  const int promoRow = promotionRow(me);

  for (uint32_t m = b.pieces(me); m != 0; m &= m - 1) {
    const Square from = __builtin_ctz(m);
    const bool king = (b.kings[s] & bit(from)) != 0;
    for (int d = 0; d < kNumDirections; ++d) {
      if (!king && d != firstFwd && d != firstFwd + 1) continue;
      const Square to = kNeighbor[from][d];
      if (to == kNoSquare || (empty & bit(to)) == 0) continue;
      out.push(Move::simple(from, to, !king && rowOf(to) == promoRow));
    }
  }
}

}  // namespace checkers
