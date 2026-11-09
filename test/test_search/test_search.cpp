// Alpha-beta search (ARCHITECTURE §4, issue #15).
#include <unity.h>

#include <chrono>
#include <cstdio>

#include "game/Apply.h"
#include "game/Eval.h"
#include "game/MoveGen.h"
#include "game/Search.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

// One Searcher for the whole suite, outside any stack frame (as on the device).
static Searcher g_searcher;

static Board parse(const char* s) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE_MESSAGE(Board::fromString(s, b), s);
  return b;
}

static SearchResult run(const Board& b, int depth, bool forcedCapture = true) {
  SearchLimits limits;
  limits.maxDepth = depth;
  limits.forcedCapture = forcedCapture;
  return g_searcher.search(b, limits, EvalWeights{});
}

static bool isLegal(const Board& b, const Move& m, bool forcedCapture = true) {
  MoveList list;
  legalMoves(b, list, forcedCapture);
  for (const Move& x : list) {
    if (x == m) return true;
  }
  return false;
}

// Moves are given in notation (1..32) as from-to; for captures the first matching sequence.
static bool sameFromTo(const Move& m, int from, int to) {
  return m.from == fromNotation(from) && m.to == fromNotation(to);
}

// Plain negamax without pruning or ordering, same rules as the search (terminal scoring, forced
// capture extension, ply cap). Reference for the alpha-beta value.
static int reference(const Board& b, int depth, int ply, int ext) {
  if (ply >= kMaxSearchPly) return evaluate(b);
  MoveList list;
  legalMoves(b, list);
  if (list.empty()) return -kMateScore + ply;
  bool allCaptures = true;
  for (const Move& m : list) allCaptures = allCaptures && m.isCapture();
  int childDepth = depth - 1;
  int childExt = ext;
  if (allCaptures && ext < kMaxCaptureExtension) {
    childDepth = depth;
    childExt = ext + 1;
  }
  if (childDepth < 0) return evaluate(b);
  int best = -kSearchInfinity;
  for (const Move& m : list) {
    const int s = -reference(applyMove(b, m), childDepth, ply + 1, childExt);
    if (s > best) best = s;
  }
  return best;
}

static void test_one_ply_winning_capture(void) {
  // Black has several captures; exactly one of them removes both White pieces.
  const Board b = parse("B:WK23,24:B11,K20,26");
  const SearchResult r = run(b, 4);
  TEST_ASSERT_TRUE(r.completed);
  TEST_ASSERT_TRUE(isLegal(b, r.best));
  TEST_ASSERT_EQUAL_INT(kMateScore - 1, r.score);
  const Board after = applyMove(b, r.best);
  TEST_ASSERT_EQUAL_UINT32(0, after.pieces(Side::White));

  // Without forced capture Black may also step; capturing White's last man still wins at once.
  const Board q = parse("B:W18:B22,K1");
  const SearchResult rq = run(q, 3, /*forcedCapture=*/false);
  TEST_ASSERT_TRUE(rq.best.isCapture());
  TEST_ASSERT_EQUAL_INT(kMateScore - 1, rq.score);
  TEST_ASSERT_EQUAL_INT(1, rq.depth);  // proven win stops the deepening
}

static void test_three_ply_shot(void) {
  // Material is level. 27-23 gives a man away; White must take and Black's reply wins two.
  const Board b = parse("B:W9,10,17,19:B16,27,30,31");
  for (int depth = 3; depth <= 5; ++depth) {
    const SearchResult r = run(b, depth);
    TEST_ASSERT_TRUE(r.completed);
    TEST_ASSERT_TRUE_MESSAGE(sameFromTo(r.best, 27, 23), "expected the 27-23 shot");
    TEST_ASSERT_TRUE(r.score >= EvalWeights{}.man);
  }
  // A second shot: 26-22 sacrifices to win a man back with interest.
  const Board c = parse("B:W4,18,24:B8,25,26");
  const SearchResult rc = run(c, 3);
  TEST_ASSERT_TRUE_MESSAGE(sameFromTo(rc.best, 26, 22), "expected the 26-22 shot");
  TEST_ASSERT_TRUE(rc.score >= EvalWeights{}.man / 2);
}

static void test_prefers_faster_mate(void) {
  // K30-25 leaves the White king on 29 without a move (mate in 1); other moves mate in 3.
  const char* const positions[] = {"B:WK29:B22,K30", "B:W20:BK19,K27", "B:W13:BK21,22"};
  for (const char* s : positions) {
    const Board b = parse(s);
    const SearchResult r = run(b, 6);
    TEST_ASSERT_EQUAL_INT_MESSAGE(kMateScore - 1, r.score, s);
    const Board after = applyMove(b, r.best);
    MoveList replies;
    legalMoves(after, replies);
    TEST_ASSERT_TRUE_MESSAGE(replies.empty(), s);
  }
}

static void test_mated_root_and_slow_loss(void) {
  // No legal move at the root.
  const SearchResult r = run(parse("W:WK29:B22,25"), 4);
  TEST_ASSERT_FALSE(r.hasMove());
  TEST_ASSERT_EQUAL_INT(-kMateScore, r.score);
  TEST_ASSERT_TRUE(r.completed);
  // White to move in a lost position delays the loss as long as possible.
  const Board b = parse("W:WK29:B22,K30");
  const int ref = reference(b, 4, 0, 0);
  const SearchResult rl = run(b, 4);
  TEST_ASSERT_EQUAL_INT(ref, rl.score);
}

static int depthOneBest(const Board& b) {
  MoveList list;
  legalMoves(b, list);
  int best = -kSearchInfinity;
  for (const Move& m : list) {
    const Board child = applyMove(b, m);
    // Precondition: no forced capture for the opponent, so depth 1 evaluates every child.
    MoveList caps;
    captureMoves(child, caps);
    TEST_ASSERT_TRUE(caps.empty());
    const int s = -evaluate(child);
    if (s > best) best = s;
  }
  return best;
}

static void test_depth_one_matches_evaluate(void) {
  const char* const positions[] = {
      "B:W1,2,3,4,5,6,7,8,9,10,11,12:B21,22,23,24,25,26,27,28,29,30,31,32",
      "W:W1,2,3,4,5,6,7,8,9,10,11,12:B21,22,23,24,25,26,27,28,29,30,31,32",
      "B:W1,3,K10:B26,K28,32",
  };
  for (const char* s : positions) {
    const Board b = parse(s);
    const int expected = depthOneBest(b);
    const SearchResult r = run(b, 1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, r.depth, s);
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected, r.score, s);
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected, -evaluate(applyMove(b, r.best)), s);
  }
}

static void test_matches_plain_negamax(void) {
  const char* const positions[] = {
      "B:W1,2,3,4,5,6,7,8,9,10,11,12:B21,22,23,24,25,26,27,28,29,30,31,32",
      "W:W18,24,27,28,K10,K15:B12,16,20,K22",
      "B:W18,24,27,28,K10,K15:B12,16,20,K22",
      "B:W9,10,17,19:B16,27,30,31",
      "W:W5,9,K14:B21,K26,30",
  };
  for (const char* s : positions) {
    const Board b = parse(s);
    for (int depth = 1; depth <= 4; ++depth) {
      const SearchResult r = run(b, depth);
      TEST_ASSERT_EQUAL_INT_MESSAGE(reference(b, depth, 0, 0), r.score, s);
      // The chosen move is worth exactly the root score.
      MoveList list;
      legalMoves(b, list);
      const bool forced = list[0].isCapture();
      const int childScore =
          -reference(applyMove(b, r.best), forced ? depth : depth - 1, 1, forced ? 1 : 0);
      TEST_ASSERT_EQUAL_INT_MESSAGE(r.score, childScore, s);
      TEST_ASSERT_TRUE_MESSAGE(isLegal(b, r.best), s);
    }
  }
}

static void test_deterministic(void) {
  const Board b = parse("W:W18,24,27,28,K10,K15:B12,16,20,K22");
  const SearchResult a = run(b, 6);
  // Disturb the searcher's ordering state with an unrelated search in between.
  run(Board::initial(), 5);
  const SearchResult c = run(b, 6);
  const SearchResult d = search(b, SearchLimits{6, 1024, true}, EvalWeights{});
  TEST_ASSERT_TRUE(a.best == c.best);
  TEST_ASSERT_EQUAL_INT(a.score, c.score);
  TEST_ASSERT_EQUAL_UINT32(a.nodes, c.nodes);
  TEST_ASSERT_TRUE(a.best == d.best);
  TEST_ASSERT_EQUAL_INT(a.score, d.score);
  TEST_ASSERT_EQUAL_UINT32(a.nodes, d.nodes);
}

struct CounterStop {
  uint32_t calls = 0;
  uint32_t fireAfter = 0;  // returns true on call fireAfter + 1
};

static bool counterStop(void* ctx) {
  CounterStop* c = static_cast<CounterStop*>(ctx);
  return ++c->calls > c->fireAfter;
}

static void test_stop_callback(void) {
  const Board b = Board::initial();
  const SearchResult four = run(b, 4);
  TEST_ASSERT_TRUE(four.completed);

  // Poll every node; fire 10 nodes into the depth-5 iteration.
  SearchLimits limits;
  limits.maxDepth = 8;
  limits.nodeCheckInterval = 1;
  CounterStop c;
  c.fireAfter = four.nodes + 10;
  const SearchResult r = g_searcher.search(b, limits, EvalWeights{}, counterStop, &c);
  TEST_ASSERT_FALSE(r.completed);
  TEST_ASSERT_EQUAL_INT(4, r.depth);
  TEST_ASSERT_TRUE(r.best == four.best);
  TEST_ASSERT_EQUAL_INT(four.score, r.score);
  TEST_ASSERT_EQUAL_UINT32(four.nodes + 11, r.nodes);  // aborted on the very node it fired
  TEST_ASSERT_EQUAL_UINT32(four.nodes + 11, c.calls);

  // Default interval: the callback runs once per 1024 nodes.
  CounterStop c2;
  c2.fireAfter = 3;
  const SearchResult r2 = g_searcher.search(b, SearchLimits{}, EvalWeights{}, counterStop, &c2);
  TEST_ASSERT_FALSE(r2.completed);
  TEST_ASSERT_EQUAL_UINT32(4, c2.calls);
  TEST_ASSERT_EQUAL_UINT32(4 * 1024, r2.nodes);
  TEST_ASSERT_TRUE(isLegal(b, r2.best));

  // Stopped before depth 1 completes: still a legal move, depth 0.
  CounterStop c3;
  limits.nodeCheckInterval = 1;
  const SearchResult r3 = g_searcher.search(b, limits, EvalWeights{}, counterStop, &c3);
  TEST_ASSERT_FALSE(r3.completed);
  TEST_ASSERT_EQUAL_INT(0, r3.depth);
  TEST_ASSERT_TRUE(r3.hasMove());
  TEST_ASSERT_TRUE(isLegal(b, r3.best));
}

using Clock = std::chrono::steady_clock;

struct Deadline {
  Clock::time_point end;
};

static bool deadlineStop(void* ctx) {
  return Clock::now() >= static_cast<Deadline*>(ctx)->end;
}

static long long msSince(Clock::time_point t0) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
}

static void test_time_budget(void) {
  const Board b = parse("B:W1,2,3,4,6,7,8,9,10,12:B21,22,23,24,25,26,27,29,30,31");
  const int budgetMs = 50;
  const Clock::time_point t0 = Clock::now();
  Deadline d{t0 + std::chrono::milliseconds(budgetMs)};
  const SearchResult r = g_searcher.search(b, SearchLimits{}, EvalWeights{}, deadlineStop, &d);
  const long long elapsed = msSince(t0);
  char msg[96];
  snprintf(msg, sizeof msg, "budget %d ms: elapsed %lld ms, depth %d, nodes %u", budgetMs, elapsed,
           r.depth, static_cast<unsigned>(r.nodes));
  TEST_MESSAGE(msg);
  TEST_ASSERT_FALSE(r.completed);
  TEST_ASSERT_TRUE(r.depth >= 4);
  TEST_ASSERT_TRUE(isLegal(b, r.best));
  TEST_ASSERT_TRUE_MESSAGE(elapsed <= budgetMs + 10, msg);
}

static void test_initial_depth6_nodes_per_second(void) {
  const Board b = Board::initial();
  const Clock::time_point t0 = Clock::now();
  const SearchResult r = run(b, 6);
  const long long us =
      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
  char msg[128];
  snprintf(msg, sizeof msg,
           "initial position depth 6: %u nodes in %lld us = %.0f nodes/s; "
           "sizeof(Searcher) = %u bytes",
           static_cast<unsigned>(r.nodes), us, us > 0 ? 1e6 * r.nodes / us : 0.0,
           static_cast<unsigned>(sizeof(Searcher)));
  printf("%s\n", msg);
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE(r.completed);
  TEST_ASSERT_EQUAL_INT(6, r.depth);
  TEST_ASSERT_TRUE(isLegal(b, r.best));
  TEST_ASSERT_TRUE(r.nodes > 100);
  TEST_ASSERT_TRUE(r.nodes < 200000);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_one_ply_winning_capture);
  RUN_TEST(test_three_ply_shot);
  RUN_TEST(test_prefers_faster_mate);
  RUN_TEST(test_mated_root_and_slow_loss);
  RUN_TEST(test_depth_one_matches_evaluate);
  RUN_TEST(test_matches_plain_negamax);
  RUN_TEST(test_deterministic);
  RUN_TEST(test_stop_callback);
  RUN_TEST(test_time_budget);
  RUN_TEST(test_initial_depth6_nodes_per_second);
  return UNITY_END();
}
