#include "game/Apply.h"

namespace checkers {

void apply(Board& b, const Move& m) {
  const Side me = b.side;
  const int s = sideIndex(me);
  const int e = sideIndex(opposite(me));

  const uint32_t fromBit = bit(m.from);
  const bool king = (b.kings[s] & fromBit) != 0 || m.promotes;
  b.men[s] &= ~fromBit;
  b.kings[s] &= ~fromBit;

  b.men[e] &= ~m.captured;
  b.kings[e] &= ~m.captured;

  const uint32_t toBit = bit(m.to);
  if (king) {
    b.kings[s] |= toBit;
  } else {
    b.men[s] |= toBit;
  }
  b.side = opposite(me);
}

}  // namespace checkers
