#pragma once
// Checkers board representation (ARCHITECTURE §6). Pure C++17: no Arduino/ESP-IDF headers,
// no heap, no exceptions, no iostream.
//
// Design decisions
// ----------------
// * Squares. Only the 32 dark squares are playable. Internally a square is a 0-based index
//   0..31 (type `Square`, an int). The standard 1-32 "notation number" (as printed in
//   ARCHITECTURE §6 and used in position strings) is always notation = index + 1; convert with
//   toNotation()/fromNotation() and never mix the two.
//
//        col: 0  1  2  3  4  5  6  7
//     row 0:     1     2     3     4      <- top of the screen, White's back rank
//     row 1:  5     6     7     8
//     row 2:     9    10    11    12
//     row 3: 13    14    15    16
//     row 4:    17    18    19    20
//     row 5: 21    22    23    24
//     row 6:    25    26    27    28
//     row 7: 29    30    31    32         <- bottom of the screen, Black's back rank
//
//   (row, col) is dark iff (row + col) is odd; index = row * 4 + col / 2.
//
// * Sides. Black is Player 1: dark pieces, moves first, starts on 21-32 (rows 5-7); its men move
//   toward row 0 (NW/NE) and promote there. White is Player 2: starts on 1-12 (rows 0-2); its
//   men move toward row 7 (SW/SE) and promote there.
//
// * Bitboards. Bit i of a mask is square index i. men[] and kings[] are indexed by
//   sideIndex(Side) and are kept disjoint: a square holds at most one piece.
//
// * Neighbors. kNeighbor[sq][dir] is the adjacent square in direction dir, or -1 off board.
//   Directions are NW, NE, SW, SE in that order; "N" means toward row 0 (up the screen). The
//   table is generated at compile time from the row/col mapping. A jump from a in direction d
//   lands on kNeighbor[kNeighbor[a][d]][d] (when both are not -1).
//
// * Position strings: `<side>:W<list>:B<list>`, e.g. `W:W18,24,27,28,K10,K15:B12,16,20,K22`.
//   <side> is the side to move ('W' or 'B'); lists are comma-separated 1-based notation numbers,
//   a `K` prefix marks a king, and a list may be empty (`B:W:B1`). toString() writes the White
//   list then the Black list, each in ascending square order with men and kings interleaved.
//   fromString() is strict: exactly that layout (White list first), no whitespace, numbers 1-32
//   without leading zeros, no duplicate squares, no man on its own promotion row (a Black man on
//   row 0 or a White man on row 7 cannot occur in play). It leaves `out` untouched on failure.
#include <cstddef>
#include <cstdint>

namespace checkers {

using Square = int;  // 0-based square index 0..31, or -1 for "none / off board"

constexpr int kNumSquares = 32;
constexpr Square kNoSquare = -1;

enum class Side : uint8_t { Black = 0, White = 1 };

constexpr Side opposite(Side s) {
  return s == Side::Black ? Side::White : Side::Black;
}
constexpr int sideIndex(Side s) {
  return static_cast<int>(s);
}

enum class Piece : uint8_t { None, BlackMan, BlackKing, WhiteMan, WhiteKing };

constexpr bool isKing(Piece p) {
  return p == Piece::BlackKing || p == Piece::WhiteKing;
}
// Side owning p; meaningless for Piece::None.
constexpr Side sideOf(Piece p) {
  return (p == Piece::WhiteMan || p == Piece::WhiteKing) ? Side::White : Side::Black;
}
constexpr Piece makePiece(Side s, bool king) {
  return s == Side::Black ? (king ? Piece::BlackKing : Piece::BlackMan)
                          : (king ? Piece::WhiteKing : Piece::WhiteMan);
}

enum Direction : int { NW = 0, NE = 1, SW = 2, SE = 3 };
constexpr int kNumDirections = 4;

// ---- Mapping ---------------------------------------------------------------------------------

constexpr bool isDarkSquare(int row, int col) {
  return row >= 0 && row < 8 && col >= 0 && col < 8 && ((row + col) & 1) == 1;
}

// Square index for (row, col), or -1 for light squares and off-board coordinates.
constexpr Square squareFromRowCol(int row, int col) {
  return isDarkSquare(row, col) ? row * 4 + col / 2 : kNoSquare;
}

constexpr int rowOf(Square sq) {
  return sq / 4;
}
// Even rows have their dark squares on odd columns (1,3,5,7); odd rows on even columns.
constexpr int colOf(Square sq) {
  return (sq % 4) * 2 + ((rowOf(sq) & 1) == 0 ? 1 : 0);
}

constexpr bool isValidSquare(Square sq) {
  return sq >= 0 && sq < kNumSquares;
}
constexpr int toNotation(Square sq) {
  return sq + 1;
}  // index 0..31 -> notation 1..32
constexpr Square fromNotation(int n) {
  return n - 1;
}  // notation 1..32 -> index 0..31
constexpr uint32_t bit(Square sq) {
  return uint32_t{1} << sq;
}

// Row on which a man of side s promotes.
constexpr int promotionRow(Side s) {
  return s == Side::Black ? 0 : 7;
}

// ---- Neighbor table (compile time) -----------------------------------------------------------

namespace detail {
struct NeighborTable {
  int8_t v[kNumSquares][kNumDirections];
};

constexpr NeighborTable makeNeighborTable() {
  NeighborTable t{};
  constexpr int dRow[kNumDirections] = {-1, -1, +1, +1};  // NW, NE, SW, SE
  constexpr int dCol[kNumDirections] = {-1, +1, -1, +1};
  for (int sq = 0; sq < kNumSquares; ++sq) {
    for (int d = 0; d < kNumDirections; ++d) {
      t.v[sq][d] = static_cast<int8_t>(squareFromRowCol(rowOf(sq) + dRow[d], colOf(sq) + dCol[d]));
    }
  }
  return t;
}

inline constexpr NeighborTable kNeighborTable = makeNeighborTable();
}  // namespace detail

// kNeighbor[sq][dir]: adjacent square index in Direction dir, or -1 off board.
inline constexpr const int8_t (&kNeighbor)[kNumSquares][kNumDirections] = detail::kNeighborTable.v;

// Spot checks against the diagram above (notation n = index n - 1).
static_assert(squareFromRowCol(0, 1) == 0 && squareFromRowCol(7, 6) == 31, "corner mapping");
static_assert(squareFromRowCol(0, 0) == -1 && squareFromRowCol(8, 1) == -1, "light/off board");
static_assert(kNeighbor[0][NW] == -1 && kNeighbor[0][NE] == -1, "square 1 is on the top edge");
static_assert(kNeighbor[0][SW] == 4 && kNeighbor[0][SE] == 5, "square 1 -> 5, 6");
static_assert(kNeighbor[4][NE] == 0 && kNeighbor[4][NW] == -1, "square 5 is on the left edge");

// ---- Board -----------------------------------------------------------------------------------

// Buffer size that always fits Board::toString() output (worst case is 32 kings, ~138 chars).
constexpr size_t kPositionStringMax = 160;

struct Board {
  uint32_t men[2];    // indexed by sideIndex(Side)
  uint32_t kings[2];  // indexed by sideIndex(Side)
  Side side;          // side to move

  static Board initial();     // Black men on 21-32, White men on 1-12, Black to move
  static Board emptyBoard();  // no pieces, Black to move

  uint32_t pieces(Side s) const { return men[sideIndex(s)] | kings[sideIndex(s)]; }
  uint32_t occupied() const { return pieces(Side::Black) | pieces(Side::White); }
  uint32_t empty() const { return ~occupied(); }  // all 32 bits are squares; no mask needed

  Piece pieceAt(Square sq) const;
  // Places p on sq, replacing whatever was there; Piece::None clears the square.
  void set(Square sq, Piece p);
  void clear(Square sq);

  // Writes a NUL-terminated position string. Returns false (and writes "" if cap > 0) when it
  // does not fit; a buffer of kPositionStringMax always fits.
  bool toString(char* out, size_t cap) const;
  // Parses a position string. On failure returns false and leaves `out` untouched.
  static bool fromString(const char* s, Board& out);

  bool operator==(const Board& o) const {
    return men[0] == o.men[0] && men[1] == o.men[1] && kings[0] == o.kings[0] &&
           kings[1] == o.kings[1] && side == o.side;
  }
  bool operator!=(const Board& o) const { return !(*this == o); }
};

}  // namespace checkers
