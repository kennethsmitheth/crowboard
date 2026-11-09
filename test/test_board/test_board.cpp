// Board representation tests: mapping, neighbor table, initial position, position strings.
#include <unity.h>

#include <cstring>

#include "game/Board.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

static int popcount(uint32_t x) {
  int n = 0;
  for (; x != 0; x &= x - 1) ++n;
  return n;
}

// ---- Mapping ---------------------------------------------------------------------------------

static void test_every_square_round_trips_through_row_col(void) {
  for (Square sq = 0; sq < kNumSquares; ++sq) {
    const int r = rowOf(sq);
    const int c = colOf(sq);
    TEST_ASSERT_TRUE(r >= 0 && r < 8);
    TEST_ASSERT_TRUE(c >= 0 && c < 8);
    TEST_ASSERT_TRUE(isDarkSquare(r, c));
    TEST_ASSERT_EQUAL_INT(sq, squareFromRowCol(r, c));
  }
}

static void test_every_dark_row_col_round_trips_and_light_is_rejected(void) {
  int dark = 0;
  for (int r = 0; r < 8; ++r) {
    for (int c = 0; c < 8; ++c) {
      const Square sq = squareFromRowCol(r, c);
      if (isDarkSquare(r, c)) {
        ++dark;
        TEST_ASSERT_TRUE(isValidSquare(sq));
        TEST_ASSERT_EQUAL_INT(r, rowOf(sq));
        TEST_ASSERT_EQUAL_INT(c, colOf(sq));
      } else {
        TEST_ASSERT_EQUAL_INT(-1, sq);
      }
    }
  }
  TEST_ASSERT_EQUAL_INT(32, dark);
  TEST_ASSERT_EQUAL_INT(-1, squareFromRowCol(-1, 0));
  TEST_ASSERT_EQUAL_INT(-1, squareFromRowCol(0, 8));
  TEST_ASSERT_EQUAL_INT(-1, squareFromRowCol(8, 1));
  TEST_ASSERT_EQUAL_INT(-1, squareFromRowCol(3, -1));
}

static void test_mapping_matches_architecture_diagram(void) {
  // Notation numbers from ARCHITECTURE §6.
  TEST_ASSERT_EQUAL_INT(1, toNotation(squareFromRowCol(0, 1)));
  TEST_ASSERT_EQUAL_INT(4, toNotation(squareFromRowCol(0, 7)));
  TEST_ASSERT_EQUAL_INT(5, toNotation(squareFromRowCol(1, 0)));
  TEST_ASSERT_EQUAL_INT(8, toNotation(squareFromRowCol(1, 6)));
  TEST_ASSERT_EQUAL_INT(13, toNotation(squareFromRowCol(3, 0)));
  TEST_ASSERT_EQUAL_INT(20, toNotation(squareFromRowCol(4, 7)));
  TEST_ASSERT_EQUAL_INT(29, toNotation(squareFromRowCol(7, 0)));
  TEST_ASSERT_EQUAL_INT(32, toNotation(squareFromRowCol(7, 6)));
  TEST_ASSERT_EQUAL_INT(0, fromNotation(1));
  TEST_ASSERT_EQUAL_INT(31, fromNotation(32));
}

// ---- Neighbors -------------------------------------------------------------------------------

static void test_neighbors_are_symmetric(void) {
  for (Square a = 0; a < kNumSquares; ++a) {
    const int pairs[4][2] = {{NE, SW}, {SW, NE}, {NW, SE}, {SE, NW}};
    for (const auto& pr : pairs) {
      const int b = kNeighbor[a][pr[0]];
      if (b == -1) continue;
      TEST_ASSERT_TRUE(isValidSquare(b));
      TEST_ASSERT_EQUAL_INT(a, kNeighbor[b][pr[1]]);
    }
  }
}

static void test_neighbors_are_diagonally_adjacent(void) {
  const int dRow[4] = {-1, -1, 1, 1};
  const int dCol[4] = {-1, 1, -1, 1};
  for (Square a = 0; a < kNumSquares; ++a) {
    for (int d = 0; d < kNumDirections; ++d) {
      const int r = rowOf(a) + dRow[d];
      const int c = colOf(a) + dCol[d];
      const bool onBoard = r >= 0 && r < 8 && c >= 0 && c < 8;
      if (onBoard) {
        TEST_ASSERT_EQUAL_INT(squareFromRowCol(r, c), kNeighbor[a][d]);
      } else {
        TEST_ASSERT_EQUAL_INT(-1, kNeighbor[a][d]);
      }
    }
  }
}

static void test_edge_squares_have_off_board_neighbors(void) {
  for (Square sq = 0; sq < kNumSquares; ++sq) {
    const int r = rowOf(sq);
    const int c = colOf(sq);
    TEST_ASSERT_EQUAL_INT(r == 0 || c == 0, kNeighbor[sq][NW] == -1);
    TEST_ASSERT_EQUAL_INT(r == 0 || c == 7, kNeighbor[sq][NE] == -1);
    TEST_ASSERT_EQUAL_INT(r == 7 || c == 0, kNeighbor[sq][SW] == -1);
    TEST_ASSERT_EQUAL_INT(r == 7 || c == 7, kNeighbor[sq][SE] == -1);
  }
  // Concrete spot checks in notation (n - 1 = index).
  TEST_ASSERT_EQUAL_INT(-1, kNeighbor[fromNotation(1)][NW]);
  TEST_ASSERT_EQUAL_INT(-1, kNeighbor[fromNotation(1)][NE]);
  TEST_ASSERT_EQUAL_INT(fromNotation(5), kNeighbor[fromNotation(1)][SW]);
  TEST_ASSERT_EQUAL_INT(fromNotation(6), kNeighbor[fromNotation(1)][SE]);
  TEST_ASSERT_EQUAL_INT(fromNotation(8), kNeighbor[fromNotation(4)][SW]);
  TEST_ASSERT_EQUAL_INT(-1, kNeighbor[fromNotation(4)][SE]);
  TEST_ASSERT_EQUAL_INT(-1, kNeighbor[fromNotation(5)][NW]);
  TEST_ASSERT_EQUAL_INT(-1, kNeighbor[fromNotation(5)][SW]);
  TEST_ASSERT_EQUAL_INT(fromNotation(1), kNeighbor[fromNotation(5)][NE]);
  TEST_ASSERT_EQUAL_INT(fromNotation(9), kNeighbor[fromNotation(5)][SE]);
  TEST_ASSERT_EQUAL_INT(fromNotation(25), kNeighbor[fromNotation(29)][NE]);
  TEST_ASSERT_EQUAL_INT(-1, kNeighbor[fromNotation(29)][NW]);
  TEST_ASSERT_EQUAL_INT(-1, kNeighbor[fromNotation(32)][SE]);
  TEST_ASSERT_EQUAL_INT(fromNotation(27), kNeighbor[fromNotation(32)][NW]);
  TEST_ASSERT_EQUAL_INT(fromNotation(28), kNeighbor[fromNotation(32)][NE]);
  // Interior square 18 (row 4, col 3): 14, 15 above; 22, 23 below.
  TEST_ASSERT_EQUAL_INT(fromNotation(14), kNeighbor[fromNotation(18)][NW]);
  TEST_ASSERT_EQUAL_INT(fromNotation(15), kNeighbor[fromNotation(18)][NE]);
  TEST_ASSERT_EQUAL_INT(fromNotation(22), kNeighbor[fromNotation(18)][SW]);
  TEST_ASSERT_EQUAL_INT(fromNotation(23), kNeighbor[fromNotation(18)][SE]);
}

// ---- Board -----------------------------------------------------------------------------------

static void test_initial_position(void) {
  const Board b = Board::initial();
  TEST_ASSERT_TRUE(b.side == Side::Black);
  TEST_ASSERT_EQUAL_INT(12, popcount(b.men[sideIndex(Side::Black)]));
  TEST_ASSERT_EQUAL_INT(12, popcount(b.men[sideIndex(Side::White)]));
  TEST_ASSERT_EQUAL_UINT32(0, b.kings[0]);
  TEST_ASSERT_EQUAL_UINT32(0, b.kings[1]);
  TEST_ASSERT_EQUAL_INT(24, popcount(b.occupied()));
  TEST_ASSERT_EQUAL_INT(8, popcount(b.empty()));
  for (Square sq = 0; sq < kNumSquares; ++sq) {
    const int r = rowOf(sq);
    const Piece expect = r <= 2 ? Piece::WhiteMan : (r >= 5 ? Piece::BlackMan : Piece::None);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expect), static_cast<int>(b.pieceAt(sq)));
  }
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(1)) == Piece::WhiteMan);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(12)) == Piece::WhiteMan);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(13)) == Piece::None);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(20)) == Piece::None);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(21)) == Piece::BlackMan);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(32)) == Piece::BlackMan);
}

static void test_set_clear_and_equality(void) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_EQUAL_UINT32(0, b.occupied());
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, b.empty());
  b.set(fromNotation(10), Piece::WhiteKing);
  b.set(fromNotation(22), Piece::BlackMan);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(10)) == Piece::WhiteKing);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(22)) == Piece::BlackMan);
  TEST_ASSERT_EQUAL_UINT32(bit(fromNotation(10)), b.pieces(Side::White));
  TEST_ASSERT_EQUAL_UINT32(bit(fromNotation(22)), b.pieces(Side::Black));
  b.set(fromNotation(22), Piece::BlackKing);  // replaces, never duplicates
  TEST_ASSERT_EQUAL_UINT32(0, b.men[0]);
  TEST_ASSERT_EQUAL_UINT32(bit(fromNotation(22)), b.kings[0]);
  Board c = b;
  TEST_ASSERT_TRUE(b == c);
  c.side = opposite(c.side);
  TEST_ASSERT_TRUE(b != c);
  b.clear(fromNotation(10));
  b.set(fromNotation(22), Piece::None);
  TEST_ASSERT_TRUE(b == Board::emptyBoard());
  TEST_ASSERT_TRUE(opposite(Side::Black) == Side::White);
  TEST_ASSERT_TRUE(opposite(Side::White) == Side::Black);
}

// ---- Position strings ------------------------------------------------------------------------

static void test_initial_position_string_round_trips(void) {
  char buf[kPositionStringMax];
  TEST_ASSERT_TRUE(Board::initial().toString(buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING(
      "B:W1,2,3,4,5,6,7,8,9,10,11,12:B21,22,23,24,25,26,27,28,29,30,31,32", buf);
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE(Board::fromString(buf, b));
  TEST_ASSERT_TRUE(b == Board::initial());
}

static void test_mixed_position_with_kings_round_trips(void) {
  const char* s = "W:W18,24,27,28,K10,K15:B12,16,20,K22,K25,K29";  // ARCHITECTURE §6
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE(Board::fromString(s, b));
  TEST_ASSERT_TRUE(b.side == Side::White);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(18)) == Piece::WhiteMan);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(10)) == Piece::WhiteKing);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(15)) == Piece::WhiteKing);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(12)) == Piece::BlackMan);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(22)) == Piece::BlackKing);
  TEST_ASSERT_TRUE(b.pieceAt(fromNotation(29)) == Piece::BlackKing);
  TEST_ASSERT_EQUAL_INT(6, popcount(b.pieces(Side::White)));
  TEST_ASSERT_EQUAL_INT(6, popcount(b.pieces(Side::Black)));

  char buf[kPositionStringMax];
  TEST_ASSERT_TRUE(b.toString(buf, sizeof buf));
  // Canonical output is ascending square order.
  TEST_ASSERT_EQUAL_STRING("W:WK10,K15,18,24,27,28:B12,16,20,K22,K25,K29", buf);
  Board c = Board::emptyBoard();
  TEST_ASSERT_TRUE(Board::fromString(buf, c));
  TEST_ASSERT_TRUE(b == c);
}

static void test_empty_lists_and_full_king_board(void) {
  Board b = Board::initial();
  TEST_ASSERT_TRUE(Board::fromString("B:W:B", b));
  TEST_ASSERT_TRUE(b == Board::emptyBoard());
  TEST_ASSERT_TRUE(Board::fromString("W:W:BK1", b));
  TEST_ASSERT_TRUE(b.side == Side::White);
  TEST_ASSERT_TRUE(b.pieceAt(0) == Piece::BlackKing);

  // Worst-case length: every square a king.
  Board full = Board::emptyBoard();
  for (Square sq = 0; sq < kNumSquares; ++sq) {
    full.set(sq, sq < 16 ? Piece::WhiteKing : Piece::BlackKing);
  }
  char buf[kPositionStringMax];
  TEST_ASSERT_TRUE(full.toString(buf, sizeof buf));
  Board back = Board::emptyBoard();
  TEST_ASSERT_TRUE(Board::fromString(buf, back));
  TEST_ASSERT_TRUE(back == full);
}

static void test_to_string_reports_overflow(void) {
  char small[10];
  std::memset(small, 'x', sizeof small);
  TEST_ASSERT_FALSE(Board::initial().toString(small, sizeof small));
  TEST_ASSERT_EQUAL_STRING("", small);
  char exact[8];  // "B:W:B" needs 6 bytes including NUL
  TEST_ASSERT_TRUE(Board::emptyBoard().toString(exact, 6));
  TEST_ASSERT_EQUAL_STRING("B:W:B", exact);
  TEST_ASSERT_FALSE(Board::emptyBoard().toString(exact, 5));
}

static void test_from_string_rejects_malformed_input(void) {
  const char* bad[] = {
      "",
      "garbage",
      "X:W1:B32",          // bad side to move
      "W",                 // truncated
      "W:",                // truncated
      "W:W1",              // missing black list
      "W:W1:",             // missing B tag
      "W:B32:W1",          // lists in the wrong order
      "W:W1:B32:",         // trailing separator
      "W:W1:B32x",         // trailing garbage
      " W:W1:B32",         // whitespace
      "W:W1, 2:B32",       // whitespace inside list
      "W:W0:B32",          // square 0
      "W:W33:B32",         // square 33
      "W:W01:B32",         // leading zero
      "W:W123:B32",        // too many digits
      "W:W1,:B32",         // empty item
      "W:W,1:B32",         // leading comma
      "W:W1,,2:B32",       // double comma
      "W:WK:B32",          // K without number
      "W:WKK1:B32",        // double K
      "W:Wk1:B32",         // lowercase k
      "W:W1,1:B32",        // duplicate within a list
      "W:W5:B5",           // same square on both sides
      "W:W5:BK5",          // same square, king
      "W:W29:B12",         // white man on its promotion row
      "W:W12:B1",          // black man on its promotion row
      "w:W1:B32",          // lowercase side
  };
  for (const char* s : bad) {
    Board b = Board::initial();
    TEST_ASSERT_FALSE_MESSAGE(Board::fromString(s, b), s);
    TEST_ASSERT_TRUE_MESSAGE(b == Board::initial(), s);  // untouched on failure
  }
  Board b = Board::initial();
  TEST_ASSERT_FALSE(Board::fromString(nullptr, b));
  // Kings on the far rows are fine.
  TEST_ASSERT_TRUE(Board::fromString("W:WK29:BK1", b));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_every_square_round_trips_through_row_col);
  RUN_TEST(test_every_dark_row_col_round_trips_and_light_is_rejected);
  RUN_TEST(test_mapping_matches_architecture_diagram);
  RUN_TEST(test_neighbors_are_symmetric);
  RUN_TEST(test_neighbors_are_diagonally_adjacent);
  RUN_TEST(test_edge_squares_have_off_board_neighbors);
  RUN_TEST(test_initial_position);
  RUN_TEST(test_set_clear_and_equality);
  RUN_TEST(test_initial_position_string_round_trips);
  RUN_TEST(test_mixed_position_with_kings_round_trips);
  RUN_TEST(test_empty_lists_and_full_king_board);
  RUN_TEST(test_to_string_reports_overflow);
  RUN_TEST(test_from_string_rejects_malformed_input);
  return UNITY_END();
}
