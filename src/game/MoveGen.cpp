#if defined(__XTENSA__)
#pragma GCC optimize("O2")
#endif
#include "game/MoveGen.h"

namespace checkers {

namespace {

// Forward directions: Black toward row 0 (NW, NE); White toward row 7 (SW, SE). The two forward
// directions are firstForward() and firstForward() + 1.
constexpr int firstForward(Side s) {
  return s == Side::Black ? NW : SW;
}

// Builds the jump sequences of one piece. `m` is the sequence so far; each step pushes a
// landing, recurses, then pops it again.
struct JumpBuilder {
  uint32_t enemies;  // all enemy pieces, including those already captured in this sequence
  uint32_t empty;    // empty squares, with the moving piece's origin treated as empty
  bool king;
  int firstFwd;
  int promoRow;
  MoveList* out;
  Move m;

  void extend(Square at) {
    bool jumped = false;
    if (m.pathLen < kMaxPathLen) {
      for (int d = 0; d < kNumDirections; ++d) {
        if (!king && d != firstFwd && d != firstFwd + 1) continue;
        const Square over = kNeighbor[at][d];
        if (over == kNoSquare) continue;
        const uint32_t overBit = bit(over);
        if ((enemies & overBit) == 0 || (m.captured & overBit) != 0) continue;
        const Square land = kNeighbor[over][d];
        if (land == kNoSquare || (empty & bit(land)) == 0) continue;

        jumped = true;
        const uint32_t savedCaptured = m.captured;
        m.path[m.pathLen++] = static_cast<uint8_t>(land);
        m.captured |= overBit;
        if (!king && rowOf(land) == promoRow) {
          // A man reaching its promotion row is crowned and the turn ends immediately.
          m.to = static_cast<uint8_t>(land);
          m.promotes = true;
          out->push(m);
          m.promotes = false;
        } else {
          extend(land);
        }
        --m.pathLen;
        m.captured = savedCaptured;
      }
    }
    if (!jumped && m.pathLen > 0) {
      m.to = static_cast<uint8_t>(at);
      out->push(m);
    }
  }
};

// One diagonal step of a whole bitboard. Within a direction the index delta depends on the row
// parity, so each direction has two (mask, delta) pairs; squares without a neighbour in that
// direction are in neither mask.
struct StepTable {
  uint32_t mask[kNumDirections][2];
  int delta[kNumDirections][2];
};

constexpr StepTable makeStepTable() {
  StepTable t{};
  for (int d = 0; d < kNumDirections; ++d) {
    int deltas[2] = {0, 0};
    int nd = 0;
    for (int sq = 0; sq < kNumSquares; ++sq) {
      const int n = kNeighbor[sq][d];
      if (n < 0) continue;
      const int k = n - sq;
      int slot = 0;
      while (slot < nd && deltas[slot] != k) ++slot;
      if (slot == nd) deltas[nd++] = k;  // at most two distinct deltas per direction
      t.mask[d][slot] |= uint32_t{1} << sq;
    }
    t.delta[d][0] = deltas[0];
    t.delta[d][1] = deltas[1];
  }
  return t;
}

constexpr StepTable kStep = makeStepTable();

constexpr uint32_t shiftBy(uint32_t x, int k) {
  return k >= 0 ? x << k : x >> -k;
}

// Squares reached by moving every piece of `from` one step in direction d.
constexpr uint32_t step(uint32_t from, int d) {
  return shiftBy(from & kStep.mask[d][0], kStep.delta[d][0]) |
         shiftBy(from & kStep.mask[d][1], kStep.delta[d][1]);
}

static_assert(step(bit(0), SW) == bit(4) && step(bit(0), SE) == bit(5), "1 -> 5, 6");
static_assert(step(bit(4), NE) == bit(0) && step(bit(4), NW) == 0, "5 -> 1, edge");
static_assert(step(bit(31), NW) == bit(26) && step(bit(31), NE) == bit(27), "32 -> 27, 28");

}  // namespace

bool hasCapture(const Board& b) {
  const int s = sideIndex(b.side);
  const uint32_t enemy = b.pieces(opposite(b.side));
  const uint32_t empty = b.empty();
  const int fwd = firstForward(b.side);
  for (int d = 0; d < kNumDirections; ++d) {
    const uint32_t movers = (d == fwd || d == fwd + 1) ? (b.men[s] | b.kings[s]) : b.kings[s];
    if (step(step(movers, d) & enemy, d) & empty) return true;
  }
  return false;
}

int countSimpleMoves(const Board& b) {
  const int s = sideIndex(b.side);
  const uint32_t empty = b.empty();
  const int fwd = firstForward(b.side);
  int n = 0;
  for (int d = 0; d < kNumDirections; ++d) {
    const uint32_t movers = (d == fwd || d == fwd + 1) ? (b.men[s] | b.kings[s]) : b.kings[s];
    n += __builtin_popcount(step(movers, d) & empty);
  }
  return n;
}

int countLegalMoves(const Board& b) {
  if (!hasCapture(b)) return countSimpleMoves(b);
  MoveList list;
  captureMoves(b, list);
  return static_cast<int>(list.size());
}

void simpleMoves(const Board& b, MoveList& out) {
  const Side me = b.side;
  const int s = sideIndex(me);
  const uint32_t empty = b.empty();
  const int firstFwd = firstForward(me);
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

void captureMoves(const Board& b, MoveList& out) {
  const Side me = b.side;
  const int s = sideIndex(me);

  JumpBuilder jb{};
  jb.enemies = b.pieces(opposite(me));
  jb.firstFwd = firstForward(me);
  jb.promoRow = promotionRow(me);
  jb.out = &out;

  for (uint32_t p = b.pieces(me); p != 0; p &= p - 1) {
    const Square from = __builtin_ctz(p);
    jb.king = (b.kings[s] & bit(from)) != 0;
    jb.empty = b.empty() | bit(from);
    jb.m = Move{};
    jb.m.from = static_cast<uint8_t>(from);
    jb.extend(from);
  }
}

void legalMoves(const Board& b, MoveList& out, bool forcedCapture) {
  const size_t before = out.size();
  captureMoves(b, out);
  if (forcedCapture && (out.size() > before || out.overflowed())) return;
  simpleMoves(b, out);
}

}  // namespace checkers
