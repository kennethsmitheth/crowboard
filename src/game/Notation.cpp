#include "game/Notation.h"

#include "game/MoveGen.h"
#include "game/MoveList.h"

namespace checkers {

namespace {

// Bounded output cursor. put() keeps one byte free for the terminator.
struct Writer {
  char* out;
  size_t cap;
  size_t len = 0;

  bool put(char c) {
    if (len + 1 >= cap) return false;
    out[len++] = c;
    return true;
  }
  bool putSquare(Square sq) {
    const int n = toNotation(sq);
    if (n >= 10 && !put(static_cast<char>('0' + n / 10))) return false;
    return put(static_cast<char>('0' + n % 10));
  }
};

bool wellFormed(const Move& m) {
  if (m.pathLen < 1 || m.pathLen > kMaxPathLen) return false;
  if (!isValidSquare(m.from)) return false;
  if (!m.isCapture()) return m.pathLen == 1 && isValidSquare(m.to);
  for (int i = 0; i < m.pathLen; ++i) {
    if (!isValidSquare(m.path[i])) return false;
  }
  return true;
}

// Reads one notation number (1..32, at most two digits, no leading zero) and advances p.
bool readNumber(const char*& p, Square& sq) {
  if (*p < '1' || *p > '9') return false;
  int n = *p++ - '0';
  if (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
  if (n < 1 || n > kNumSquares) return false;
  sq = fromNotation(n);
  return true;
}

}  // namespace

bool moveToString(const Move& m, char* out, size_t cap) {
  if (out == nullptr || cap == 0) return false;
  out[0] = '\0';
  if (!wellFormed(m)) return false;

  Writer w{out, cap};
  bool ok = w.putSquare(m.from);
  if (m.isCapture()) {
    for (int i = 0; ok && i < m.pathLen; ++i) {
      ok = w.put('x') && w.putSquare(m.path[i]);
    }
  } else {
    ok = w.put('-') && w.putSquare(m.to);
  }
  if (!ok) {
    out[0] = '\0';
    return false;
  }
  out[w.len] = '\0';
  return true;
}

bool parseMove(const char* text, const Board& board, Move& out) {
  if (text == nullptr) return false;

  // sq[0] is the start square, sq[1..count-1] the landings in order.
  Square sq[kMaxPathLen + 1];
  int count = 0;
  char sep = '\0';
  const char* p = text;

  if (!readNumber(p, sq[count])) return false;
  ++count;
  while (*p != '\0') {
    const char c = *p;
    if (c != '-' && c != 'x') return false;
    if (sep == '\0') {
      sep = c;
    } else if (c != sep) {
      return false;  // mixed separators
    }
    ++p;
    if (count == kMaxPathLen + 1) return false;  // more landings than any move can have
    if (!readNumber(p, sq[count])) return false;
    ++count;
  }
  if (count < 2) return false;
  // `-` joins exactly two numbers: start and destination of a simple move.
  if (sep == '-' && count != 2) return false;

  const bool capture = sep == 'x';
  const int landings = count - 1;
  MoveList moves;
  legalMoves(board, moves);
  for (const Move& m : moves) {
    if (m.from != sq[0] || m.isCapture() != capture || m.pathLen != landings) continue;
    bool same = true;
    for (int i = 0; same && i < landings; ++i) same = m.path[i] == sq[i + 1];
    if (same) {
      out = m;
      return true;
    }
  }
  return false;
}

}  // namespace checkers
