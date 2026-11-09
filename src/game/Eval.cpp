#include "game/Eval.h"

#include "game/MoveGen.h"

namespace checkers {

namespace {

constexpr uint32_t makeCentreMask() {
  uint32_t m = 0;
  for (int sq = 0; sq < kNumSquares; ++sq) {
    const int r = rowOf(sq);
    const int c = colOf(sq);
    if (r >= 2 && r <= 5 && c >= 2 && c <= 5) m |= bit(sq);
  }
  return m;
}

constexpr uint32_t makeRowMask(int row) {
  uint32_t m = 0;
  for (int sq = 0; sq < kNumSquares; ++sq) {
    if (rowOf(sq) == row) m |= bit(sq);
  }
  return m;
}

constexpr uint32_t kCentre = makeCentreMask();
// Back rank of Black is row 7, of White row 0.
constexpr uint32_t kBackRank[2] = {makeRowMask(7), makeRowMask(0)};

int popcount(uint32_t x) {
  return __builtin_popcount(x);
}

// True when the king on sq has neither a step nor a jump.
bool kingTrapped(const Board& b, Square sq, Side s) {
  const uint32_t occ = b.occupied();
  const uint32_t enemy = b.pieces(opposite(s));
  for (int d = 0; d < kNumDirections; ++d) {
    const int n = kNeighbor[sq][d];
    if (n < 0) continue;
    if (!(occ & bit(n))) return false;
    if (enemy & bit(n)) {
      const int j = kNeighbor[n][d];
      if (j >= 0 && !(occ & bit(j))) return false;
    }
  }
  return true;
}

// Non-mobility score for one side.
int sideScore(const Board& b, Side s, const EvalWeights& w) {
  const uint32_t men = b.men[sideIndex(s)];
  const uint32_t kings = b.kings[sideIndex(s)];
  int score = popcount(men) * w.man + popcount(kings) * w.king;
  score += popcount((men | kings) & kCentre) * w.centre;
  score += popcount(men & kBackRank[sideIndex(s)]) * w.backRank;

  for (uint32_t m = men; m; m &= m - 1) {
    const int sq = __builtin_ctz(m);
    const int advanced = s == Side::Black ? 7 - rowOf(sq) : rowOf(sq);
    score += advanced * w.advance;
  }
  for (uint32_t k = kings; k; k &= k - 1) {
    if (kingTrapped(b, __builtin_ctz(k), s)) score -= w.trappedKing;
  }
  return score;
}

uint32_t reverseBits(uint32_t x) {
  uint32_t r = 0;
  for (int i = 0; i < 32; ++i) {
    if (x & (uint32_t{1} << i)) r |= uint32_t{1} << (31 - i);
  }
  return r;
}

}  // namespace

int evaluate(const Board& b, const EvalWeights& w) {
  const Side me = b.side;
  const Side them = opposite(me);
  int score = sideScore(b, me, w) - sideScore(b, them, w);

  if (w.mobility != 0) {
    MoveList mine;
    legalMoves(b, mine);
    Board flipped = b;
    flipped.side = them;
    MoveList theirs;
    legalMoves(flipped, theirs);
    score += (static_cast<int>(mine.size()) - static_cast<int>(theirs.size())) * w.mobility;
  }
  return score;
}

Board mirror(const Board& b) {
  Board out = b;
  for (int i = 0; i < 2; ++i) {
    out.men[i] = reverseBits(b.men[1 - i]);
    out.kings[i] = reverseBits(b.kings[1 - i]);
  }
  // out.side stays b.side: evaluate() is from the mover's view, so swapping colours while
  // keeping the mover negates the score (evaluate(b) == -evaluate(mirror(b))).
  return out;
}

}  // namespace checkers
