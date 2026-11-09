#include "game/Board.h"

namespace checkers {

namespace {

constexpr uint32_t kRows0to2 = 0x00000FFFu;  // squares 1-12
constexpr uint32_t kRows5to7 = 0xFFF00000u;  // squares 21-32

// Bounded writer for toString(); remembers overflow instead of failing at each call site.
struct Writer {
  char* out;
  size_t cap;
  size_t len;
  bool ok;

  void put(char c) {
    if (len + 1 < cap) {
      out[len++] = c;
    } else {
      ok = false;
    }
  }
  void putNumber(int n) {  // 1..32
    if (n >= 10) put(static_cast<char>('0' + n / 10));
    put(static_cast<char>('0' + n % 10));
  }
};

void writeList(Writer& w, const Board& b, Side s) {
  bool first = true;
  for (Square sq = 0; sq < kNumSquares; ++sq) {
    const uint32_t m = bit(sq);
    if ((b.pieces(s) & m) == 0) continue;
    if (!first) w.put(',');
    first = false;
    if (b.kings[sideIndex(s)] & m) w.put('K');
    w.putNumber(toNotation(sq));
  }
}

// Parses a (possibly empty) piece list starting at *p, stopping at ':' or NUL. Adds the pieces
// to b for side s, rejecting squares already occupied in b.
bool parseList(const char*& p, Board& b, Side s) {
  if (*p == ':' || *p == '\0') return true;  // empty list
  for (;;) {
    bool king = false;
    if (*p == 'K') {
      king = true;
      ++p;
    }
    if (*p < '1' || *p > '9') return false;  // requires a digit, no leading zero
    int n = *p++ - '0';
    if (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
    if (n < 1 || n > kNumSquares) return false;
    const Square sq = fromNotation(n);
    if (b.occupied() & bit(sq)) return false;  // duplicate square
    if (!king && rowOf(sq) == promotionRow(s)) return false;
    b.set(sq, makePiece(s, king));
    if (*p == ',') {
      ++p;
      continue;
    }
    return *p == ':' || *p == '\0';
  }
}

}  // namespace

Board Board::initial() {
  Board b = emptyBoard();
  b.men[sideIndex(Side::Black)] = kRows5to7;
  b.men[sideIndex(Side::White)] = kRows0to2;
  return b;
}

Board Board::emptyBoard() {
  Board b{};
  b.side = Side::Black;
  return b;
}

Piece Board::pieceAt(Square sq) const {
  const uint32_t m = bit(sq);
  if (men[0] & m) return Piece::BlackMan;
  if (kings[0] & m) return Piece::BlackKing;
  if (men[1] & m) return Piece::WhiteMan;
  if (kings[1] & m) return Piece::WhiteKing;
  return Piece::None;
}

void Board::clear(Square sq) {
  const uint32_t keep = ~bit(sq);
  men[0] &= keep;
  men[1] &= keep;
  kings[0] &= keep;
  kings[1] &= keep;
}

void Board::set(Square sq, Piece p) {
  clear(sq);
  if (p == Piece::None) return;
  const int s = sideIndex(sideOf(p));
  if (isKing(p)) {
    kings[s] |= bit(sq);
  } else {
    men[s] |= bit(sq);
  }
}

bool Board::toString(char* out, size_t cap) const {
  if (out == nullptr || cap == 0) return false;
  Writer w{out, cap, 0, true};
  w.put(side == Side::White ? 'W' : 'B');
  w.put(':');
  w.put('W');
  writeList(w, *this, Side::White);
  w.put(':');
  w.put('B');
  writeList(w, *this, Side::Black);
  out[w.ok ? w.len : 0] = '\0';
  return w.ok;
}

bool Board::fromString(const char* s, Board& out) {
  if (s == nullptr) return false;
  Board b = emptyBoard();
  const char* p = s;

  if (*p == 'W') {
    b.side = Side::White;
  } else if (*p == 'B') {
    b.side = Side::Black;
  } else {
    return false;
  }
  ++p;

  if (*p++ != ':' || *p++ != 'W') return false;
  if (!parseList(p, b, Side::White)) return false;
  if (*p++ != ':' || *p++ != 'B') return false;
  if (!parseList(p, b, Side::Black)) return false;
  if (*p != '\0') return false;

  out = b;
  return true;
}

}  // namespace checkers
