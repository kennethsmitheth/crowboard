// AiSync request/result mapping (issue #17).
#include <initializer_list>
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

void test_profiles_table() {
  TEST_ASSERT_EQUAL_UINT8(3, difficultyProfile(Difficulty::Easy).randomTopN);
  TEST_ASSERT_FALSE(difficultyProfile(Difficulty::Medium).useTT);
  TEST_ASSERT_TRUE(difficultyProfile(Difficulty::Hard).useTT);
  AiRequest r = makeRequest(Board::initial(), Difficulty::Hard, 1, true, 9, true);
  TEST_ASSERT_TRUE(r.useTT);
  TEST_ASSERT_TRUE(r.newGame);
  TEST_ASSERT_EQUAL_UINT32(9, r.seed);
}

void test_rank_root_moves_sorted_and_matches_search() {
  static Searcher s;
  ScoredMove top[8];
  SearchLimits l;
  l.maxDepth = 4;
  const size_t n = s.rankRootMoves(Board::initial(), top, 8, l);
  TEST_ASSERT_EQUAL_UINT32(7, n);  // seven legal opening moves
  for (size_t i = 1; i < n; ++i) TEST_ASSERT_TRUE(top[i - 1].score >= top[i].score);
  TEST_ASSERT_EQUAL_INT(s.search(Board::initial(), l).score, top[0].score);
}

void test_easy_pick_is_reproducible_and_from_top3() {
  static Searcher s;
  ScoredMove top[3];
  SearchLimits l;
  l.maxDepth = 2;
  const size_t n = s.rankRootMoves(Board::initial(), top, 3, l);
  TEST_ASSERT_EQUAL_UINT32(3, n);
  bool seen[3] = {false, false, false};
  for (uint32_t seed = 0; seed < 60; ++seed) {
    AiRequest r = makeRequest(Board::initial(), Difficulty::Easy, 1, true, seed);
    AiResult a = g_sync.run(r);
    TEST_ASSERT_TRUE(a.move == g_sync.run(r).move);  // same seed, same move
    int k = -1;
    for (int i = 0; i < 3; ++i) {
      if (top[i].move == a.move) k = i;
    }
    TEST_ASSERT_TRUE(k >= 0);
    seen[k] = true;
  }
  TEST_ASSERT_TRUE(seen[0] && seen[1] && seen[2]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_easy_returns_legal_move);
  RUN_TEST(test_request_id_echoed);
  RUN_TEST(test_completed_with_generous_budget);
  RUN_TEST(test_immediate_stop_not_completed_but_valid_move);
  RUN_TEST(test_limits_mapping);
  RUN_TEST(test_profiles_table);
  RUN_TEST(test_rank_root_moves_sorted_and_matches_search);
  RUN_TEST(test_easy_pick_is_reproducible_and_from_top3);
  return UNITY_END();
}
