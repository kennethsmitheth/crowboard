// Scoring formula tests (host). Table-driven; see ARCHITECTURE §11.
#include <unity.h>

#include "game/Board.h"
#include "services/GameTypes.h"
#include "services/Scoring.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

// Board with `men` men and `kings` kings for Black on the first squares, none for White.
static Board boardWith(int men, int kings) {
  Board b = Board::emptyBoard();
  for (int i = 0; i < men; ++i) b.set(i, Piece::BlackMan);
  for (int i = 0; i < kings; ++i) b.set(men + i, Piece::BlackKing);
  return b;
}

static ScoreBreakdown score(Outcome o, Difficulty d, const Board& b, uint16_t moves, uint32_t sec) {
  return computeScore(o, d, b, Side::Black, moves, sec);
}

static void test_multiplier(void) {
  TEST_ASSERT_EQUAL_UINT8(1, difficultyMultiplier(Difficulty::Easy));
  TEST_ASSERT_EQUAL_UINT8(2, difficultyMultiplier(Difficulty::Medium));
  TEST_ASSERT_EQUAL_UINT8(3, difficultyMultiplier(Difficulty::Hard));
}

static void test_result_table(void) {
  struct Row {
    Outcome o;
    Difficulty d;
    uint32_t result;
    bool recorded;
  };
  const Row rows[] = {
      {Outcome::Win, Difficulty::Easy, 1000, true},
      {Outcome::Win, Difficulty::Medium, 2000, true},
      {Outcome::Win, Difficulty::Hard, 3000, true},
      {Outcome::Draw, Difficulty::Easy, 200, true},
      {Outcome::Draw, Difficulty::Medium, 400, true},
      {Outcome::Draw, Difficulty::Hard, 600, true},
      {Outcome::Loss, Difficulty::Easy, 0, false},
      {Outcome::Loss, Difficulty::Medium, 0, false},
      {Outcome::Loss, Difficulty::Hard, 0, false},
  };
  // Empty board, 60 moves, 600 s: no bonuses, so total == result.
  for (const Row& r : rows) {
    const ScoreBreakdown s = score(r.o, r.d, boardWith(0, 0), 60, 600);
    TEST_ASSERT_EQUAL_UINT32(r.result, s.result);
    TEST_ASSERT_EQUAL_UINT32(r.result, s.total);
    TEST_ASSERT_EQUAL(r.recorded, s.recorded);
  }
}

static void test_loss_is_all_zero_even_with_bonuses(void) {
  const ScoreBreakdown s = score(Outcome::Loss, Difficulty::Hard, boardWith(5, 2), 10, 30);
  TEST_ASSERT_FALSE(s.recorded);
  TEST_ASSERT_EQUAL_UINT32(0, s.total);
  TEST_ASSERT_EQUAL_UINT32(0, s.material);
  TEST_ASSERT_EQUAL_UINT32(0, s.efficiency);
  TEST_ASSERT_EQUAL_UINT32(0, s.speed);
}

static void test_material(void) {
  struct Row {
    int men, kings;
    uint32_t material;
  };
  const Row rows[] = {{0, 0, 0}, {1, 0, 50}, {12, 0, 600}, {0, 1, 100}, {3, 2, 350}, {0, 12, 1200}};
  for (const Row& r : rows) {
    const ScoreBreakdown s =
        score(Outcome::Win, Difficulty::Easy, boardWith(r.men, r.kings), 60, 600);
    TEST_ASSERT_EQUAL_UINT32(r.material, s.material);
  }
}

static void test_material_counts_human_side_only(void) {
  Board b = Board::initial();
  b.set(0, Piece::WhiteKing);
  const ScoreBreakdown black =
      computeScore(Outcome::Win, Difficulty::Easy, b, Side::Black, 60, 600);
  const ScoreBreakdown white =
      computeScore(Outcome::Win, Difficulty::Easy, b, Side::White, 60, 600);
  TEST_ASSERT_EQUAL_UINT32(600, black.material);
  TEST_ASSERT_EQUAL_UINT32(11 * 50 + 100, white.material);
}

static void test_efficiency(void) {
  struct Row {
    uint16_t moves;
    uint32_t eff;
  };
  const Row rows[] = {{0, 600}, {1, 590}, {59, 10}, {60, 0}, {61, 0}, {200, 0}, {65535, 0}};
  for (const Row& r : rows) {
    const ScoreBreakdown s = score(Outcome::Win, Difficulty::Easy, boardWith(0, 0), r.moves, 600);
    TEST_ASSERT_EQUAL_UINT32(r.eff, s.efficiency);
  }
}

static void test_speed(void) {
  struct Row {
    uint32_t sec;
    uint32_t speed;
  };
  const Row rows[] = {{0, 300}, {1, 299}, {598, 1},    {599, 0},
                      {600, 0}, {601, 0}, {100000, 0}, {0xFFFFFFFFu, 0}};
  for (const Row& r : rows) {
    const ScoreBreakdown s = score(Outcome::Win, Difficulty::Easy, boardWith(0, 0), 60, r.sec);
    TEST_ASSERT_EQUAL_UINT32(r.speed, s.speed);
  }
}

static void test_casual_not_recorded(void) {
  const ScoreBreakdown s =
      computeScore(Outcome::Win, Difficulty::Hard, boardWith(5, 1), Side::Black, 10, 30, false);
  TEST_ASSERT_FALSE(s.recorded);
  TEST_ASSERT_EQUAL_UINT32(0, s.total);
  TEST_ASSERT_EQUAL_UINT32(0, s.result);
}

static void test_worked_example(void) {
  // Hard win, initial board as final (12 men), 40 moves, 150 s:
  // 3000 + 600 + 10*20 + (450/2) = 4025.
  const ScoreBreakdown s = score(Outcome::Win, Difficulty::Hard, Board::initial(), 40, 150);
  TEST_ASSERT_TRUE(s.recorded);
  TEST_ASSERT_EQUAL_UINT32(3000, s.result);
  TEST_ASSERT_EQUAL_UINT32(600, s.material);
  TEST_ASSERT_EQUAL_UINT32(200, s.efficiency);
  TEST_ASSERT_EQUAL_UINT32(225, s.speed);
  TEST_ASSERT_EQUAL_UINT32(4025, s.total);
}

static void test_speed_integer_division(void) {
  // 600 - 3 = 597 -> 298 (floor).
  const ScoreBreakdown s = score(Outcome::Draw, Difficulty::Easy, boardWith(0, 0), 60, 3);
  TEST_ASSERT_EQUAL_UINT32(298, s.speed);
  TEST_ASSERT_EQUAL_UINT32(200 + 298, s.total);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_multiplier);
  RUN_TEST(test_result_table);
  RUN_TEST(test_loss_is_all_zero_even_with_bonuses);
  RUN_TEST(test_material);
  RUN_TEST(test_material_counts_human_side_only);
  RUN_TEST(test_efficiency);
  RUN_TEST(test_speed);
  RUN_TEST(test_casual_not_recorded);
  RUN_TEST(test_worked_example);
  RUN_TEST(test_speed_integer_division);
  return UNITY_END();
}
