#pragma once
// Request / result types shared by the device AiWorker and the host AiSync stub, and the one
// function both use to turn a request into a search (runAiSearch()). Pure C++17.
#include <cstdint>

#include "game/Board.h"
#include "game/Difficulty.h"
#include "game/Eval.h"
#include "game/Move.h"
#include "game/MoveGen.h"
#include "game/MoveList.h"
#include "game/Search.h"
#include "game/TT.h"
#include "services/GameTypes.h"

namespace checkers {

struct AiRequest {
  Board board;
  Difficulty difficulty = Difficulty::Medium;
  uint32_t budgetMs = 500;  // wall-clock budget for the whole search; 0 = unlimited
  int maxDepth = 64;
  uint32_t requestId = 0;
  bool forcedCapture = true;
  bool useTT = false;      // search with the worker's transposition table (if it has one)
  bool newGame = false;    // clear the transposition table before this search
  uint8_t randomTopN = 1;  // > 1: pick one of the best N root moves at random (Easy)
  uint32_t seed = 0;       // AiRng seed for the random pick
  uint32_t nodeCheckInterval = kAiNodeCheckInterval;
};

struct AiResult {
  Move move{};  // pathLen == 0 if the side to move has no legal move
  int score = 0;
  int depth = 0;
  uint32_t nodes = 0;
  uint32_t requestId = 0;
  uint32_t elapsedMs = 0;
  bool completed = false;  // false if cancelled or out of budget mid-iteration
};

// Per-difficulty limits, kept for older callers; the source of truth is kDifficultyProfiles.
struct AiBudget {
  static constexpr uint32_t ms(Difficulty d) { return difficultyProfile(d).budgetMs; }
  static constexpr int depth(Difficulty d) { return difficultyProfile(d).maxDepth; }
};

// The one mapping from a request to search limits, used by AiWorker and AiSync.
inline SearchLimits toSearchLimits(const AiRequest& r) {
  SearchLimits l;
  l.maxDepth = r.maxDepth < 1 ? 1 : (r.maxDepth > kMaxSearchPly ? kMaxSearchPly : r.maxDepth);
  l.forcedCapture = r.forcedCapture;
  l.nodeCheckInterval = r.nodeCheckInterval;
  return l;
}

// Request for difficulty `d` with its profile's budget, depth, TT use and random pick. `seed`
// only matters for profiles with randomTopN > 1 (Easy); pass newGame = true for the first
// request of a game (or after the rules changed) so the worker clears its table.
inline AiRequest makeRequest(const Board& b, Difficulty d, uint32_t id, bool forcedCapture = true,
                             uint32_t seed = 0, bool newGame = false) {
  const DifficultyProfile& p = difficultyProfile(d);
  AiRequest r;
  r.board = b;
  r.difficulty = d;
  r.budgetMs = p.budgetMs;
  r.maxDepth = p.maxDepth;
  r.requestId = id;
  r.forcedCapture = forcedCapture;
  r.useTT = p.useTT;
  r.newGame = newGame;
  r.randomTopN = p.randomTopN;
  r.seed = seed;
  return r;
}

inline AiResult toAiResult(const SearchResult& s, const AiRequest& r, uint32_t elapsedMs) {
  AiResult o;
  o.move = s.best;
  o.score = s.score;
  o.depth = s.depth;
  o.nodes = s.nodes;
  o.requestId = r.requestId;
  o.elapsedMs = elapsedMs;
  o.completed = s.completed;
  return o;
}

// Owner-side transposition table policy shared by AiWorker and AiSync: the table is attached to
// the searcher only for requests with useTT, and cleared before a request with newGame or whose
// forcedCapture differs from the rules the table was filled under (entries do not record the
// variant). The eval weights are fixed, so they never invalidate it.
struct AiTable {
  TranspositionTable* tt = nullptr;  // not owned; nullptr = no table available
  bool filled = false;               // holds entries from an earlier search
  bool forcedCapture = true;         // rule variant of those entries

  void prepare(Searcher& s, const AiRequest& r) {
    if (tt == nullptr || !r.useTT) {
      s.setTT(nullptr);
      return;
    }
    if (filled && (r.newGame || r.forcedCapture != forcedCapture)) {
      tt->clear();
      filled = false;
    }
    s.setTT(tt);
    filled = true;
    forcedCapture = r.forcedCapture;
  }
};

// Runs `r` on `searcher` (whose TT the caller has already set or cleared for r.useTT). With
// randomTopN > 1 the root moves are ranked at r.maxDepth and one of the best N is picked with
// AiRng(r.seed); depth is r.maxDepth when every root move was scored, else 0 (and if none was,
// the first legal move is returned). Otherwise this is Searcher::search().
inline SearchResult runAiSearch(Searcher& searcher, const AiRequest& r, const EvalWeights& weights,
                                StopFn stop, void* ctx) {
  const SearchLimits limits = toSearchLimits(r);
  if (r.randomTopN <= 1) return searcher.search(r.board, limits, weights, stop, ctx);

  constexpr size_t kMaxTop = 8;
  ScoredMove top[kMaxTop];
  const size_t want = r.randomTopN < kMaxTop ? r.randomTopN : kMaxTop;
  const size_t n = searcher.rankRootMoves(r.board, top, want, limits, weights, stop, ctx);
  SearchResult s;
  s.nodes = searcher.lastNodes();
  s.completed = searcher.lastCompleted();
  if (n == 0) {
    MoveList list;
    legalMoves(r.board, list, r.forcedCapture);
    if (list.empty()) {
      s.score = -kMateScore;
      s.completed = true;
    } else {
      s.best = list[0];
    }
    return s;
  }
  AiRng rng(r.seed);
  const ScoredMove& pick = top[rng.below(static_cast<uint32_t>(n))];
  s.best = pick.move;
  s.score = pick.score;
  s.depth = s.completed ? limits.maxDepth : 0;
  return s;
}

}  // namespace checkers
