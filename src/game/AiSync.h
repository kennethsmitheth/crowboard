#pragma once
// Synchronous host stand-in for AiWorker: same request -> search mapping (runAiSearch()), the same
// transposition table policy (AiTable) and the same budget semantics, no FreeRTOS. Header-only,
// pure C++17.
#include <chrono>
#include <cstdint>

#include "game/AiRequest.h"
#include "game/Search.h"
#include "game/TT.h"

namespace checkers {

class AiSync {
 public:
  // Table used for requests with useTT (not owned, must outlive its use); nullptr disables it.
  void setTT(TranspositionTable* tt) {
    table_.tt = tt;
    table_.filled = false;
    if (tt != nullptr) tt->clear();
  }

  // Runs the search to completion, the time budget (request.budgetMs, 0 = unlimited) or `stop`.
  AiResult run(const AiRequest& req, StopFn stop = nullptr, void* ctx = nullptr,
               const EvalWeights& weights = EvalWeights{}) {
    using Clock = std::chrono::steady_clock;
    Ctx c{Clock::now(), req.budgetMs, stop, ctx};
    table_.prepare(searcher_, req);
    SearchResult s = runAiSearch(searcher_, req, weights, &stopThunk, &c);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - c.start);
    return toAiResult(s, req, static_cast<uint32_t>(ms.count()));
  }

 private:
  struct Ctx {
    std::chrono::steady_clock::time_point start;
    uint32_t budgetMs;
    StopFn user;
    void* userCtx;
  };
  static bool stopThunk(void* p) {
    const Ctx* c = static_cast<const Ctx*>(p);
    if (c->user && c->user(c->userCtx)) return true;
    if (c->budgetMs == 0) return false;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - c->start);
    return static_cast<uint32_t>(ms.count()) >= c->budgetMs;
  }

  Searcher searcher_;
  AiTable table_;
};

}  // namespace checkers
