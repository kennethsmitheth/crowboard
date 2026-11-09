// AiSync request/result mapping (issue #17).
#include <unity.h>

#include "game/AiRequest.h"
#include "game/AiSync.h"
#include "game/MoveGen.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

static AiSync g_sync;  // sizeof(Searcher) ~131 KB: keep off the stack

static bool isLegal(const Board& b, const Move& m) {
  MoveList list;
  legalMoves(b, list, true);
  for (int i = 0; i < list.size(); i++) {
    const Move& c = list[i];
    if (c.from == m.from && c.to == m.to && c.captured == m.captured) return true;
  }
  return false;
}

static bool stopNow(void*) {
  return true;
}

void test_easy_returns_legal_move() {
  AiRequest r = makeRequest(Board::initial(), Difficulty::Easy, 7);
  AiResult o = g_sync.run(r);
  TEST_ASSERT_TRUE(o.move.pathLen != 0);
  TEST_ASSERT_TRUE(isLegal(r.board, o.move));
  TEST_ASSERT_EQUAL_INT(2, o.depth);
  TEST_ASSERT_TRUE(o.nodes > 0);
}

void test_request_id_echoed() {
  AiRequest r = makeRequest(Board::initial(), Difficulty::Easy, 123456u);
  TEST_ASSERT_EQUAL_UINT32(123456u, g_sync.run(r).requestId);
}

void test_completed_with_generous_budget() {
  AiRequest r = makeRequest(Board::initial(), Difficulty::Easy, 1);
  r.budgetMs = 60000;
  r.maxDepth = 4;
  TEST_ASSERT_TRUE(g_sync.run(r).completed);
}

void test_immediate_stop_not_completed_but_valid_move() {
  AiRequest r = makeRequest(Board::initial(), Difficulty::Hard, 2);
  r.budgetMs = 60000;
  AiResult o = g_sync.run(r, &stopNow, nullptr);
  TEST_ASSERT_FALSE(o.completed);
  TEST_ASSERT_TRUE(o.move.pathLen != 0);
  TEST_ASSERT_TRUE(isLegal(r.board, o.move));
}

void test_limits_mapping() {
  TEST_ASSERT_EQUAL_UINT32(150, AiBudget::ms(Difficulty::Easy));
  TEST_ASSERT_EQUAL_UINT32(500, AiBudget::ms(Difficulty::Medium));
  TEST_ASSERT_EQUAL_UINT32(2500, AiBudget::ms(Difficulty::Hard));
  TEST_ASSERT_EQUAL_INT(2, AiBudget::depth(Difficulty::Easy));
  TEST_ASSERT_EQUAL_INT(64, AiBudget::depth(Difficulty::Medium));
  TEST_ASSERT_EQUAL_INT(64, AiBudget::depth(Difficulty::Hard));
  for (Difficulty d : {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard}) {
    AiRequest r = makeRequest(Board::initial(), d, 1, false);
    TEST_ASSERT_EQUAL_UINT32(AiBudget::ms(d), r.budgetMs);
    SearchLimits l = toSearchLimits(r);
    TEST_ASSERT_EQUAL_INT(AiBudget::depth(d), l.maxDepth);
    TEST_ASSERT_FALSE(l.forcedCapture);
  }
  AiRequest r = makeRequest(Board::initial(), Difficulty::Easy, 1);
  r.maxDepth = 0;
  TEST_ASSERT_EQUAL_INT(1, toSearchLimits(r).maxDepth);
  r.maxDepth = 500;
  TEST_ASSERT_EQUAL_INT(kMaxSearchPly, toSearchLimits(r).maxDepth);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_easy_returns_legal_move);
  RUN_TEST(test_request_id_echoed);
  RUN_TEST(test_completed_with_generous_budget);
  RUN_TEST(test_immediate_stop_not_completed_but_valid_move);
  RUN_TEST(test_limits_mapping);
  return UNITY_END();
}
