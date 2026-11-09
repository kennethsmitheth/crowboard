// Host tests for ui/BoardViewState: which squares a view-state or board change repaints.
#include <unity.h>

#include "ui/BoardViewState.h"

using namespace checkers;
using namespace checkers::ui;

void setUp(void) {}
void tearDown(void) {}

static uint32_t sq(int notation) {
  return uint32_t{1} << (notation - 1);
}

static void test_identical_is_empty(void) {
  Board b = Board::initial();
  BoardViewState s;
  s.selected = 22;
  s.legalTargets = sq(18) | sq(19);
  s.pendingPath[0] = 17;
  s.pendingLen = 1;
  TEST_ASSERT_EQUAL_HEX32(0, changedSquares(&b, s, &b, s));
}

static void test_selection_change_marks_old_and_new(void) {
  Board b = Board::initial();
  BoardViewState a, c;
  a.selected = 22;
  c.selected = 23;
  TEST_ASSERT_EQUAL_HEX32(sq(22) | sq(23), changedSquares(&b, a, &b, c));
  c.selected = 0;
  TEST_ASSERT_EQUAL_HEX32(sq(22), changedSquares(&b, a, &b, c));
}

static void test_targets_forced_and_last_move(void) {
  Board b = Board::initial();
  BoardViewState a, c;
  c.legalTargets = sq(18) | sq(19);
  c.forcedPieces = sq(22);
  c.lastFrom = 11;
  c.lastTo = 15;
  TEST_ASSERT_EQUAL_HEX32(sq(18) | sq(19) | sq(22) | sq(11) | sq(15), changedSquares(&b, a, &b, c));
  // Moving only the last-move marks leaves the shared square alone.
  a = c;
  c.lastFrom = 15;
  c.lastTo = 19;
  TEST_ASSERT_EQUAL_HEX32(sq(11) | sq(19), changedSquares(&b, a, &b, c));
}

static void test_pending_path(void) {
  Board b = Board::initial();
  BoardViewState a, c;
  c.pendingPath[0] = 17;  // 0-based
  c.pendingPath[1] = 9;
  c.pendingLen = 2;
  TEST_ASSERT_EQUAL_HEX32(sq(18) | sq(10), changedSquares(&b, a, &b, c));
  c.pendingLen = 1;
  a.pendingPath[0] = 17;
  a.pendingLen = 1;
  TEST_ASSERT_EQUAL_HEX32(0, changedSquares(&b, a, &b, c));
}

static void test_board_changes(void) {
  Board a = Board::initial();
  Board b = a;
  b.set(fromNotation(22), Piece::None);
  b.set(fromNotation(18), Piece::BlackMan);
  BoardViewState s;
  TEST_ASSERT_EQUAL_HEX32(sq(22) | sq(18), changedSquares(&a, s, &b, s));
  // Promotion changes only the one square.
  Board k = a;
  k.set(fromNotation(22), Piece::BlackKing);
  TEST_ASSERT_EQUAL_HEX32(sq(22), changedSquares(&a, s, &k, s));
}

static void test_null_board_is_empty(void) {
  Board b = Board::initial();
  BoardViewState s;
  TEST_ASSERT_EQUAL_HEX32(b.occupied(), changedSquares(nullptr, s, &b, s));
}

static void test_out_of_range_ignored(void) {
  Board b = Board::initial();
  BoardViewState a, c;
  c.selected = 40;
  c.lastFrom = -1;
  c.pendingPath[0] = 200;
  c.pendingLen = 1;
  TEST_ASSERT_EQUAL_HEX32(0, changedSquares(&b, a, &b, c));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_identical_is_empty);
  RUN_TEST(test_selection_change_marks_old_and_new);
  RUN_TEST(test_targets_forced_and_last_move);
  RUN_TEST(test_pending_path);
  RUN_TEST(test_board_changes);
  RUN_TEST(test_null_board_is_empty);
  RUN_TEST(test_out_of_range_ignored);
  return UNITY_END();
}
