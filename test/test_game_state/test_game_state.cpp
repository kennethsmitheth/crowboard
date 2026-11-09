// Game state: apply/undo, history, results, draws (ARCHITECTURE §5, §13).
#include <unity.h>

#include "game/Game.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

static Board parse(const char* s) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE_MESSAGE(Board::fromString(s, b), s);
  return b;
}

// Plays the legal move from -> to (notation numbers); fails the test if there is none.
static void play(Game& g, int fromN, int toN) {
  for (const Move& m : g.legalMoves()) {
    if (m.from == fromNotation(fromN) && m.to == fromNotation(toN)) {
      TEST_ASSERT_TRUE(g.apply(m));
      return;
    }
  }
  TEST_FAIL_MESSAGE("no such legal move");
}

static void test_reset(void) {
  Game g;
  TEST_ASSERT_TRUE(g.board() == Board::initial());
  TEST_ASSERT_EQUAL_UINT(7, g.legalMoves().size());
  TEST_ASSERT_EQUAL_UINT16(0, g.ply());
  TEST_ASSERT_FALSE(g.canUndo());
  TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
  TEST_ASSERT_EQUAL_UINT32(0, g.captured(Side::Black));
  TEST_ASSERT_EQUAL_UINT32(0, g.captured(Side::White));
}

static void test_apply_advances(void) {
  Game g;
  play(g, 22, 18);
  TEST_ASSERT_EQUAL_UINT16(1, g.ply());
  TEST_ASSERT_TRUE(g.sideToMove() == Side::White);
  TEST_ASSERT_EQUAL_UINT16(1, g.historySize());
  TEST_ASSERT_TRUE(g.canUndo());
}

static void test_illegal_move_rejected(void) {
  Game g;
  const Board before = g.board();
  TEST_ASSERT_FALSE(g.apply(Move::simple(0, 5, false)));
  TEST_ASSERT_TRUE(g.board() == before);
  TEST_ASSERT_EQUAL_UINT16(0, g.ply());
  TEST_ASSERT_EQUAL_UINT(7, g.legalMoves().size());
}

static void test_undo_restores(void) {
  Game g;
  play(g, 22, 18);
  play(g, 9, 13);
  const Board before = g.board();
  const uint16_t clk = g.halfmoveClock();
  const size_t n = g.legalMoves().size();
  play(g, 18, 14);
  TEST_ASSERT_TRUE(g.undo());
  TEST_ASSERT_TRUE(g.board() == before);
  TEST_ASSERT_EQUAL_UINT16(2, g.ply());
  TEST_ASSERT_EQUAL_UINT16(clk, g.halfmoveClock());
  TEST_ASSERT_EQUAL_UINT(n, g.legalMoves().size());
  TEST_ASSERT_TRUE(g.undo());
  TEST_ASSERT_TRUE(g.undo());
  TEST_ASSERT_TRUE(g.board() == Board::initial());
  TEST_ASSERT_FALSE(g.undo());
}

static void test_halfmove_clock(void) {
  Game g;
  // King moves do not reset the clock; a man move does.
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:WK1,12:BK32,22")));
  play(g, 32, 28);
  TEST_ASSERT_EQUAL_UINT16(1, g.halfmoveClock());
  play(g, 1, 6);
  TEST_ASSERT_EQUAL_UINT16(2, g.halfmoveClock());
  play(g, 22, 18);
  TEST_ASSERT_EQUAL_UINT16(0, g.halfmoveClock());
  play(g, 6, 1);
  TEST_ASSERT_EQUAL_UINT16(1, g.halfmoveClock());
  TEST_ASSERT_TRUE(g.undo());
  TEST_ASSERT_EQUAL_UINT16(0, g.halfmoveClock());

  // A king capture resets the clock.
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:W18,K1:BK22,K32")));
  play(g, 22, 15);
  TEST_ASSERT_EQUAL_UINT16(0, g.halfmoveClock());
  TEST_ASSERT_EQUAL_UINT32(11, g.captured(Side::White));
  TEST_ASSERT_EQUAL_UINT32(10, g.captured(Side::Black));
}

static void test_captured_count(void) {
  Game g;
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:W1,2,18:B22,26")));
  play(g, 22, 15);
  TEST_ASSERT_EQUAL_UINT32(10, g.captured(Side::White));
  TEST_ASSERT_EQUAL_UINT32(10, g.captured(Side::Black));
  g.undo();
  TEST_ASSERT_EQUAL_UINT32(9, g.captured(Side::White));
}

static void test_no_pieces(void) {
  Game g;
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:W18:B22,26")));
  play(g, 22, 15);
  TEST_ASSERT_TRUE(g.result() == Result::P1Wins);
  TEST_ASSERT_TRUE(g.reason() == Reason::NoPieces);
  TEST_ASSERT_FALSE(g.apply(Move::simple(25, 21, false)));
  TEST_ASSERT_TRUE(g.undo());
  TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
  TEST_ASSERT_TRUE(g.reason() == Reason::None);
}

static void test_no_moves(void) {
  Game g;
  // White man on 1 is blocked by Black men on 5 and 6 (backed by 9 and 10): no moves.
  TEST_ASSERT_TRUE(g.resetFrom(parse("W:W1:B5,6,9,10")));
  TEST_ASSERT_TRUE(g.result() == Result::P1Wins);
  TEST_ASSERT_TRUE(g.reason() == Reason::NoMoves);

  // Reached by a move: Black plays 14-10 and leaves White's man on 1 blocked.
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:W1:B5,6,9,14")));
  TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
  play(g, 14, 10);
  TEST_ASSERT_TRUE(g.result() == Result::P1Wins);
  TEST_ASSERT_TRUE(g.reason() == Reason::NoMoves);
  TEST_ASSERT_TRUE(g.undo());
  TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
}

static void test_forty_move_rule(void) {
  Game g;
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:WK2:BK17")));
  // Black's king walks a 6-cycle and White's a 10-cycle (far apart, so no captures). The joint
  // position repeats only every 60 plies, so no position occurs three times within 80 plies and
  // only the 80-ply rule can end the game.
  const int bc[6] = {17, 22, 26, 30, 25, 21};
  const int wc[10] = {2, 7, 11, 16, 20, 24, 19, 15, 10, 6};
  for (int i = 0; i < 80; ++i) {
    TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
    TEST_ASSERT_EQUAL_UINT16(i, g.halfmoveClock());
    if (i % 2 == 0) {
      play(g, bc[(i / 2) % 6], bc[(i / 2 + 1) % 6]);
    } else {
      play(g, wc[(i / 2) % 10], wc[(i / 2 + 1) % 10]);
    }
  }
  TEST_ASSERT_TRUE(g.result() == Result::Draw);
  TEST_ASSERT_TRUE(g.reason() == Reason::FortyMoveRule);
  TEST_ASSERT_TRUE(g.undo());
  TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
  TEST_ASSERT_EQUAL_UINT16(79, g.halfmoveClock());
}

static void test_repetition(void) {
  Game g;
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:WK1:BK32")));
  // 4-ply cycle: the start position occurs at plies 0, 4 and 8 -> draw at ply 8.
  for (int cycle = 0; cycle < 2; ++cycle) {
    play(g, 32, 28);
    play(g, 1, 6);
    play(g, 28, 32);
    TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
    play(g, 6, 1);
    TEST_ASSERT_TRUE(g.result() == (cycle == 1 ? Result::Draw : Result::Ongoing));
  }
  TEST_ASSERT_EQUAL_UINT16(8, g.ply());
  TEST_ASSERT_TRUE(g.reason() == Reason::Repetition);
  TEST_ASSERT_TRUE(g.undo());
  TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
  TEST_ASSERT_TRUE(g.reason() == Reason::None);
}

static void test_resign_and_draw(void) {
  Game g;
  g.resign(Side::Black);
  TEST_ASSERT_TRUE(g.result() == Result::P2Wins);
  TEST_ASSERT_TRUE(g.reason() == Reason::Resign);
  g.reset();
  g.resign(Side::White);
  TEST_ASSERT_TRUE(g.result() == Result::P1Wins);
  g.reset();
  g.agreeDraw();
  TEST_ASSERT_TRUE(g.result() == Result::Draw);
  TEST_ASSERT_TRUE(g.reason() == Reason::Agreement);
}

static void test_replay(void) {
  Game g;
  for (int i = 0; i < 20 && g.result() == Result::Ongoing; ++i) {
    const MoveList& l = g.legalMoves();
    TEST_ASSERT_FALSE(l.empty());
    Move m = l[static_cast<size_t>(i * 3) % l.size()];
    TEST_ASSERT_TRUE(g.apply(m));
  }
  Game h;
  for (uint16_t i = 0; i < g.historySize(); ++i) TEST_ASSERT_TRUE(h.apply(g.historyAt(i)));
  TEST_ASSERT_TRUE(h.board() == g.board());
  TEST_ASSERT_EQUAL_UINT16(g.ply(), h.ply());
  TEST_ASSERT_EQUAL_UINT16(g.halfmoveClock(), h.halfmoveClock());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_reset);
  RUN_TEST(test_apply_advances);
  RUN_TEST(test_illegal_move_rejected);
  RUN_TEST(test_undo_restores);
  RUN_TEST(test_halfmove_clock);
  RUN_TEST(test_captured_count);
  RUN_TEST(test_no_pieces);
  RUN_TEST(test_no_moves);
  RUN_TEST(test_forty_move_rule);
  RUN_TEST(test_repetition);
  RUN_TEST(test_resign_and_draw);
  RUN_TEST(test_replay);
  return UNITY_END();
}
