#pragma once
// Request / result types shared by the device AiWorker and the host AiSync stub. Pure C++17.
#include <cstdint>

#include "game/Board.h"
#include "game/Eval.h"
#include "game/Move.h"
#include "game/Search.h"
#include "services/GameTypes.h"

namespace checkers {

struct AiRequest {
  Board board;
  Difficulty difficulty = Difficulty::Medium;
  uint32_t budgetMs = 500;  // wall-clock budget for the whole search
  int maxDepth = 64;
  uint32_t requestId = 0;
  bool forcedCapture = true;
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

// Placeholder per-difficulty limits; issue #18 refines them.
struct AiBudget {
  static constexpr uint32_t ms(Difficulty d) {
    return d == Difficulty::Easy ? 150u : d == Difficulty::Medium ? 500u : 2500u;
  }
  static constexpr int depth(Difficulty d) { return d == Difficulty::Easy ? 2 : 64; }
};

// The one mapping from a request to search limits, used by AiWorker and AiSync.
inline SearchLimits toSearchLimits(const AiRequest& r) {
  SearchLimits l;
  l.maxDepth = r.maxDepth < 1 ? 1 : (r.maxDepth > kMaxSearchPly ? kMaxSearchPly : r.maxDepth);
  l.forcedCapture = r.forcedCapture;
  return l;
}

// Request for `difficulty` with the AiBudget defaults.
inline AiRequest makeRequest(const Board& b, Difficulty d, uint32_t id, bool forcedCapture = true) {
  AiRequest r;
  r.board = b;
  r.difficulty = d;
  r.budgetMs = AiBudget::ms(d);
  r.maxDepth = AiBudget::depth(d);
  r.requestId = id;
  r.forcedCapture = forcedCapture;
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

}  // namespace checkers
