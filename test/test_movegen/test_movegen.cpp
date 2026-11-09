// Simple (non-capturing) move generation tests.
#include <unity.h>

#include "game/MoveGen.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

static Board parse(const char* s) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE_MESSAGE(Board::fromString(s, b), s);
  return b;
}

// Arguments are 1-32 notation numbers.
static bool hasMove(const MoveList& l, int fromN, int toN) {
  for (const Move& m : l) {
    if (m.from == fromN - 1 && m.to == toN - 1) return true;
  }
  return false;
}

static void test_initial_position_has_seven_moves_for_black(void) {
  MoveList l;
  simpleMoves(Board::initial(), l);
  TEST_ASSERT_EQUAL_UINT(7, l.size());
  for (const Move& m : l) {
    TEST_ASSERT_FALSE(m.isCapture());
    TEST_ASSERT_EQUAL_INT(0, m.captureCount());
    TEST_ASSERT_FALSE(m.promotes);
    TEST_ASSERT_EQUAL_UINT8(1, m.pathLen);
    TEST_ASSERT_EQUAL_UINT8(m.to, m.path[0]);
  }
  TEST_ASSERT_TRUE(hasMove(l, 21, 17));
  TEST_ASSERT_TRUE(hasMove(l, 24, 20));
}

static void test_initial_position_has_seven_moves_for_white(void) {
  Board b = parse("W:W1,2,3,4,5,6,7,8,9,10,11,12:B21,22,23,24,25,26,27,28,29,30,31,32");
  TEST_ASSERT_TRUE(b.side == Side::White);
  MoveList l;
  simpleMoves(b, l);
  TEST_ASSERT_EQUAL_UINT(7, l.size());
  TEST_ASSERT_TRUE(hasMove(l, 9, 13));
  TEST_ASSERT_TRUE(hasMove(l, 12, 16));
}

static void test_lone_king_in_centre_has_four_moves(void) {
  MoveList l;
  simpleMoves(parse("B:W:BK18"), l);  // 18 -> 14, 15, 22, 23
  TEST_ASSERT_EQUAL_UINT(4, l.size());
  TEST_ASSERT_TRUE(hasMove(l, 18, 14));
  TEST_ASSERT_TRUE(hasMove(l, 18, 15));
  TEST_ASSERT_TRUE(hasMove(l, 18, 22));
  TEST_ASSERT_TRUE(hasMove(l, 18, 23));
  for (const Move& m : l) TEST_ASSERT_FALSE(m.promotes);
}

static void test_lone_king_in_corner_has_one_move(void) {
  MoveList l;
  simpleMoves(parse("B:W:BK4"), l);  // top-right corner: only 4 -> 8
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(hasMove(l, 4, 8));
  l.clear();
  simpleMoves(parse("B:W:BK29"), l);  // bottom-left corner: only 29 -> 25
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(hasMove(l, 29, 25));
}

static void test_blocked_man_has_no_moves(void) {
  // Black men 25, 26, 30; White pieces on 21, 22, 23 block the front men, which block 30.
  MoveList l;
  simpleMoves(parse("B:W21,22,23:B25,26,30"), l);
  TEST_ASSERT_EQUAL_UINT(0, l.size());
  // Same without the White blockers: 30 is still blocked by its own men.
  l.clear();
  simpleMoves(parse("B:W:B25,26,30"), l);
  TEST_ASSERT_FALSE(hasMove(l, 30, 25));
  TEST_ASSERT_FALSE(hasMove(l, 30, 26));
}

static void test_men_never_move_backward(void) {
  MoveList l;
  simpleMoves(parse("B:W:B18"), l);
  TEST_ASSERT_EQUAL_UINT(2, l.size());
  TEST_ASSERT_TRUE(hasMove(l, 18, 14));
  TEST_ASSERT_TRUE(hasMove(l, 18, 15));
  l.clear();
  simpleMoves(parse("W:W18:B"), l);
  TEST_ASSERT_EQUAL_UINT(2, l.size());
  TEST_ASSERT_TRUE(hasMove(l, 18, 22));
  TEST_ASSERT_TRUE(hasMove(l, 18, 23));
}

static void test_promotion_flag(void) {
  MoveList l;
  simpleMoves(parse("B:W:B6"), l);  // 6 -> 1, 2 on row 0
  TEST_ASSERT_EQUAL_UINT(2, l.size());
  for (const Move& m : l) TEST_ASSERT_TRUE(m.promotes);
  l.clear();
  simpleMoves(parse("W:W26:B"), l);  // 26 -> 30, 31 on row 7
  TEST_ASSERT_EQUAL_UINT(2, l.size());
  for (const Move& m : l) TEST_ASSERT_TRUE(m.promotes);
  l.clear();
  simpleMoves(parse("B:W:BK6"), l);  // a king reaching the row is not a promotion
  TEST_ASSERT_EQUAL_UINT(4, l.size());
  for (const Move& m : l) TEST_ASSERT_FALSE(m.promotes);
}

static void test_no_moves_for_side_not_on_move(void) {
  MoveList l;
  simpleMoves(parse("B:W18:B22"), l);  // White man 18 must not move; Black 22 -> 17 only
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  for (const Move& m : l) TEST_ASSERT_EQUAL_UINT8(21, m.from);
}

static void test_movelist_overflow_flag(void) {
  MoveList l;
  TEST_ASSERT_FALSE(l.overflowed());
  for (size_t i = 0; i < MoveList::kCapacity; ++i) {
    TEST_ASSERT_FALSE(l.full());
    l.push(Move::simple(0, 4, false));
  }
  TEST_ASSERT_TRUE(l.full());
  TEST_ASSERT_FALSE(l.overflowed());
  l.push(Move::simple(1, 5, false));
  TEST_ASSERT_TRUE(l.overflowed());
  TEST_ASSERT_EQUAL_UINT(MoveList::kCapacity, l.size());
  TEST_ASSERT_EQUAL_UINT8(0, l[MoveList::kCapacity - 1].from);
  l.clear();
  TEST_ASSERT_EQUAL_UINT(0, l.size());
  TEST_ASSERT_FALSE(l.overflowed());
}

static void test_move_equality_and_helpers(void) {
  Move a = Move::simple(5, 1, true);
  Move b = Move::simple(5, 1, true);
  TEST_ASSERT_TRUE(a == b);
  TEST_ASSERT_TRUE(a != Move::simple(5, 0, true));
  TEST_ASSERT_TRUE(a != Move::simple(5, 1, false));
  b.captured = bit(2) | bit(9);
  TEST_ASSERT_TRUE(b.isCapture());
  TEST_ASSERT_EQUAL_INT(2, b.captureCount());
  TEST_ASSERT_TRUE(a != b);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_position_has_seven_moves_for_black);
  RUN_TEST(test_initial_position_has_seven_moves_for_white);
  RUN_TEST(test_lone_king_in_centre_has_four_moves);
  RUN_TEST(test_lone_king_in_corner_has_one_move);
  RUN_TEST(test_blocked_man_has_no_moves);
  RUN_TEST(test_men_never_move_backward);
  RUN_TEST(test_promotion_flag);
  RUN_TEST(test_no_moves_for_side_not_on_move);
  RUN_TEST(test_movelist_overflow_flag);
  RUN_TEST(test_move_equality_and_helpers);
  return UNITY_END();
}
