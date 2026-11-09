// Game Over text and outcome derivation tests (host).
#include <string.h>
#include <unity.h>

#include "app/GameOverText.h"

using namespace checkers;
using namespace checkers::app;

void setUp(void) {}
void tearDown(void) {}

static void test_outcome_black_human(void) {
  TEST_ASSERT_EQUAL(Outcome::Win, humanOutcome(Result::P1Wins, Side::Black));
  TEST_ASSERT_EQUAL(Outcome::Loss, humanOutcome(Result::P2Wins, Side::Black));
  TEST_ASSERT_EQUAL(Outcome::Draw, humanOutcome(Result::Draw, Side::Black));
}

static void test_outcome_white_human(void) {
  TEST_ASSERT_EQUAL(Outcome::Loss, humanOutcome(Result::P1Wins, Side::White));
  TEST_ASSERT_EQUAL(Outcome::Win, humanOutcome(Result::P2Wins, Side::White));
  TEST_ASSERT_EQUAL(Outcome::Draw, humanOutcome(Result::Draw, Side::White));
}

static void test_headline_vs_computer(void) {
  GameConfig c;
  c.mode = GameConfig::Mode::VsComputer;
  char b[32];
  c.humanSide = Side::Black;
  headlineText(Result::P1Wins, c, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("You win!", b);
  headlineText(Result::P2Wins, c, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("Computer wins", b);
  c.humanSide = Side::White;
  headlineText(Result::P1Wins, c, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("Computer wins", b);
  headlineText(Result::P2Wins, c, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("You win!", b);
  headlineText(Result::Draw, c, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("Draw", b);
}

static void test_headline_two_players(void) {
  GameConfig c;
  c.mode = GameConfig::Mode::TwoPlayers;
  memcpy(c.p1Initials, "ABC", 4);
  memcpy(c.p2Initials, "XY ", 4);
  char b[32];
  headlineText(Result::P1Wins, c, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("ABC wins!", b);
  headlineText(Result::P2Wins, c, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("XY wins!", b);
  headlineText(Result::Draw, c, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("Draw", b);
}

static void test_reasons(void) {
  TEST_ASSERT_EQUAL_STRING("", reasonText(Reason::None));
  TEST_ASSERT_EQUAL_STRING("no pieces left", reasonText(Reason::NoPieces));
  TEST_ASSERT_EQUAL_STRING("no legal moves", reasonText(Reason::NoMoves));
  TEST_ASSERT_EQUAL_STRING("40-move rule", reasonText(Reason::FortyMoveRule));
  TEST_ASSERT_EQUAL_STRING("threefold repetition", reasonText(Reason::Repetition));
  TEST_ASSERT_EQUAL_STRING("resigned", reasonText(Reason::Resign));
  TEST_ASSERT_EQUAL_STRING("agreed", reasonText(Reason::Agreement));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_outcome_black_human);
  RUN_TEST(test_outcome_white_human);
  RUN_TEST(test_headline_vs_computer);
  RUN_TEST(test_headline_two_players);
  RUN_TEST(test_reasons);
  return UNITY_END();
}
