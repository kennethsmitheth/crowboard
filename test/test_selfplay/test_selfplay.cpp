// Host self-play, Hard versus Medium (issue #18). Deterministic: both sides are depth-capped
// (no time budget), Hard with a transposition table cleared per game. Openings: 25 pseudo-random
// two-ply openings picked with AiRng, each played twice with colours swapped. Win = 1, draw = 0.5.
// Default run: Hard depth 6 + TT vs Medium depth 3. -DSELFPLAY_LONG: depth 7 + TT vs depth 5,
// 100 games.
#include <unity.h>

#include <cstdio>
#include <vector>

#include "game/AiSync.h"
#include "game/Difficulty.h"
#include "game/Game.h"
#include "game/TT.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

#ifdef SELFPLAY_LONG
constexpr int kHardDepth = 7;
constexpr int kMediumDepth = 5;
constexpr int kOpenings = 50;
#else
constexpr int kHardDepth = 6;
constexpr int kMediumDepth = 3;
constexpr int kOpenings = 25;
#endif

static AiSync g_hard;  // each holds a ~131 KB Searcher: keep off the stack
static AiSync g_medium;

// Plays one game; returns Hard's score in half points (2 win, 1 draw, 0 loss).
static int playGame(int opening, bool hardIsBlack, TranspositionTable& tt) {
  Game g;
  AiRng rng(1000u + static_cast<uint32_t>(opening));
  for (int p = 0; p < 2 && !g.isOver(); ++p) {
    const MoveList& legal = g.legalMoves();
    g.apply(legal[rng.below(static_cast<uint32_t>(legal.size()))]);
  }
  tt.clear();
  uint32_t id = 0;
  while (!g.isOver()) {
    const bool hardToMove = (g.sideToMove() == Side::Black) == hardIsBlack;
    AiRequest r = makeRequest(g.board(), hardToMove ? Difficulty::Hard : Difficulty::Medium, ++id);
    r.budgetMs = 0;
    r.maxDepth = hardToMove ? kHardDepth : kMediumDepth;
    AiResult res = (hardToMove ? g_hard : g_medium).run(r);
    if (res.move.pathLen == 0 || !g.apply(res.move)) break;  // history full: adjudicate draw
  }
  const Side hardSide = hardIsBlack ? Side::Black : Side::White;
  switch (g.result()) {
    case Result::P1Wins:
      return hardSide == Side::Black ? 2 : 0;
    case Result::P2Wins:
      return hardSide == Side::White ? 2 : 0;
    default:
      return 1;
  }
}

void test_hard_beats_medium() {
  std::vector<TTEntry> mem(1 << 16);
  TranspositionTable tt;
  TEST_ASSERT_TRUE(tt.init(mem.data(), mem.size() * sizeof(TTEntry)));
  g_hard.setTT(&tt);
  int halfPoints = 0, wins = 0, draws = 0, games = 0;
  for (int o = 0; o < kOpenings; ++o) {
    for (int c = 0; c < 2; ++c) {
      const int s = playGame(o, c == 0, tt);
      halfPoints += s;
      wins += s == 2;
      draws += s == 1;
      ++games;
    }
  }
  char msg[160];
  snprintf(msg, sizeof msg, "Hard d%d+TT vs Medium d%d: %d games, %d W %d D %d L, score %.1f%%",
           kHardDepth, kMediumDepth, games, wins, draws, games - wins - draws,
           50.0 * halfPoints / games);
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE_MESSAGE(halfPoints * 100 >= 70 * 2 * games, msg);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_hard_beats_medium);
  return UNITY_END();
}
