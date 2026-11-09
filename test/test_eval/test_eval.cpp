// Static evaluation (ARCHITECTURE §4).
#include <unity.h>

#include "game/Eval.h"
#include "game/MoveGen.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

static Board parse(const char* s) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE_MESSAGE(Board::fromString(s, b), s);
  return b;
}

static EvalWeights noMobility() {
  EvalWeights w;
  w.mobility = 0;
  return w;
}

static const char* const kPositions[] = {
    "B:W1,2,3,4,5,6,7,8,9,10,11,12:B21,22,23,24,25,26,27,28,29,30,31,32",
    "W:W18,24,27,28,K10,K15:B12,16,20,K22",
    "B:W18,24,27,28,K10,K15:B12,16,20,K22",
    "B:WK1:B12,K32",
    "W:W5,9,K14:B21,K26,30",
    "B:W:B20",
};

static void test_initial_is_zero(void) {
  TEST_ASSERT_EQUAL_INT(0, evaluate(Board::initial()));
}

static void test_mirror_roundtrip_and_symmetry(void) {
  for (const char* s : kPositions) {
    const Board b = parse(s);
    TEST_ASSERT_TRUE_MESSAGE(mirror(mirror(b)) == b, s);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-evaluate(mirror(b)), evaluate(b), s);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-evaluate(mirror(b), noMobility()), evaluate(b, noMobility()), s);
  }
}

static void test_mirror_squares(void) {
  // Notation n <-> 33 - n; colours swap, side to move is kept.
  TEST_ASSERT_TRUE(mirror(parse("B:W1:B32")) == parse("B:W1:B32"));
  TEST_ASSERT_TRUE(mirror(parse("B:WK2:B11")) == parse("B:W22:BK31"));
}

static void test_up_a_man_is_positive(void) {
  Board b = Board::initial();
  b.clear(fromNotation(1));  // remove a White man
  const int s = evaluate(b);
  TEST_ASSERT_TRUE(s > 0);
  TEST_ASSERT_INT_WITHIN(40, EvalWeights{}.man, s);
  b.side = Side::White;
  TEST_ASSERT_TRUE(evaluate(b) < 0);
}

static void test_king_worth_more_than_man(void) {
  const EvalWeights w = noMobility();
  TEST_ASSERT_TRUE(evaluate(parse("B:W1:BK18"), w) > evaluate(parse("B:W1:B18"), w));
}

static void test_advanced_man_beats_back_rank(void) {
  const EvalWeights w = noMobility();
  // Black man on 30 (back rank, row 7) vs on 22 (row 5); neither is central.
  const int back = evaluate(parse("B:W1:B30"), w);
  const int adv = evaluate(parse("B:W1:B21"), w);
  TEST_ASSERT_EQUAL_INT(2 * w.advance - w.backRank, adv - back);
}

static void test_trapped_king_penalised(void) {
  const EvalWeights w = noMobility();
  // Black king in the corner on 29, own men on 25 (row 6) and 22 block it; no jumps possible.
  const int trapped = evaluate(parse("B:W1:BK29,25,22"), w);
  const int freeK = evaluate(parse("B:W1:BK29,22"), w);
  // Extra man on 25 is one row advanced; the king loses trappedKing.
  TEST_ASSERT_EQUAL_INT(w.man + w.advance - w.trappedKing, trapped - freeK);
}

static void test_mobility_weight(void) {
  const Board b = parse("W:W18,24,27,28,K10,K15:B12,16,20,K22");
  EvalWeights w = noMobility();
  const int off = evaluate(b, w);
  w.mobility = 50;
  const int on = evaluate(b, w);
  MoveList mine, theirs;
  legalMoves(b, mine);
  Board f = b;
  f.side = opposite(b.side);
  legalMoves(f, theirs);
  const int diff = static_cast<int>(mine.size()) - static_cast<int>(theirs.size());
  TEST_ASSERT_TRUE(diff != 0);
  TEST_ASSERT_EQUAL_INT(off + 50 * diff, on);
}

static void test_empty_side_material_dominates(void) {
  TEST_ASSERT_TRUE(evaluate(parse("B:W:B20")) > 0);
  TEST_ASSERT_TRUE(evaluate(parse("W:W:B20")) < 0);
  TEST_ASSERT_EQUAL_INT(0, evaluate(Board::emptyBoard()));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_is_zero);
  RUN_TEST(test_mirror_roundtrip_and_symmetry);
  RUN_TEST(test_mirror_squares);
  RUN_TEST(test_up_a_man_is_positive);
  RUN_TEST(test_king_worth_more_than_man);
  RUN_TEST(test_advanced_man_beats_back_rank);
  RUN_TEST(test_trapped_king_penalised);
  RUN_TEST(test_mobility_weight);
  RUN_TEST(test_empty_side_material_dominates);
  return UNITY_END();
}
