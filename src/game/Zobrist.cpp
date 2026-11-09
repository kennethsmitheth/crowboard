#include "game/Zobrist.h"

namespace checkers {

namespace {

uint64_t hashMask(uint64_t h, uint32_t mask, Piece p) {
  for (uint32_t x = mask; x != 0; x &= x - 1) h ^= zobristKey(p, __builtin_ctz(x));
  return h;
}

}  // namespace

uint64_t zobristHash(const Board& b) {
  uint64_t h = 0;
  h = hashMask(h, b.men[sideIndex(Side::Black)], Piece::BlackMan);
  h = hashMask(h, b.kings[sideIndex(Side::Black)], Piece::BlackKing);
  h = hashMask(h, b.men[sideIndex(Side::White)], Piece::WhiteMan);
  h = hashMask(h, b.kings[sideIndex(Side::White)], Piece::WhiteKing);
  if (b.side == Side::White) h ^= zobristSideKey();
  return h;
}

uint64_t zobristApply(uint64_t h, const Board& before, const Move& m) {
  const Side me = before.side;
  const Side enemy = opposite(me);
  const bool wasKing = (before.kings[sideIndex(me)] & bit(m.from)) != 0;
  // from == to (a king looping back) cancels out, as it should.
  h ^= zobristKey(makePiece(me, wasKing), m.from);
  h ^= zobristKey(makePiece(me, wasKing || m.promotes), m.to);
  const int e = sideIndex(enemy);
  h = hashMask(h, m.captured & before.men[e], makePiece(enemy, false));
  h = hashMask(h, m.captured & before.kings[e], makePiece(enemy, true));
  return h ^ zobristSideKey();
}

}  // namespace checkers
