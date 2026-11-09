#pragma once
// Game-tree search (ARCHITECTURE §4, §9). Pure C++17: no heap, no exceptions, no randomness.
//
// * Negamax alpha-beta, fail-soft, scores from the point of view of the side to move.
// * Iterative deepening from depth 1 to SearchLimits::maxDepth. Each iteration searches the
//   previous iteration's principal variation first (root best move, then the PV move at each
//   inner ply while still on the PV).
// * Move ordering: PV move, captures by captured count (descending, promotions breaking ties),
//   quiet promotions, two killer moves per ply, then a history heuristic [side][from][to].
// * Terminal: the side to move with no legal moves (including no pieces) loses with
//   -kMateScore + ply, so faster wins score higher and slower losses are preferred.
// * Forced sequences: when every legal move of the side to move is a capture, depth is not
//   reduced (also at depth 0, so a leaf is never evaluated in the middle of a forced exchange).
//   At most kMaxCaptureExtension extra plies along one line, and never beyond kMaxSearchPly.
// * Budget: the StopFn is polled every nodeCheckInterval nodes. When it returns true the current
//   iteration is abandoned and the last completed iteration's result is returned with
//   completed == false.
// * No repetition / 40-move draw detection inside the tree (Game handles those at the root).
// * Transposition table (optional, Searcher::setTT()): probed at every node with the incrementally
//   updated Zobrist key. Below the root an entry searched at least as deep as the node returns its
//   score when exact or when its bound already causes a cutoff, and otherwise narrows the window;
//   at the root it is only used for move ordering, so search() always ends with a move. Its best
//   move is ordered right after the PV move. Every node that completes stores its score (mate
//   scores converted to distance from that node), depth and bound, and the best move unless it
//   failed low. Terminal positions are stored as exact at the maximum depth. Entries do not
//   record the capture-extension budget or rule variant: clear the table when forcedCapture or
//   the eval weights change. search() does not clear the table, so results with a TT depend on
//   its contents; clear it first when reproducibility matters.
//
// Memory: a Searcher holds all per-ply state (move lists, ordering keys, PV, killers, history),
// about 130 KB (131416 bytes on the host); see sizeof(Searcher). The caller decides where it
// lives (a static or PSRAM object on the device). Recursion is bounded by kMaxSearchPly and each
// frame is small (a Board copy and a few scalars, roughly 300 bytes on the host), plus about
// 2.7 KB once at a leaf for evaluate()'s two mobility MoveLists.
#include <cstdint>

#include "game/Board.h"
#include "game/Eval.h"
#include "game/Move.h"
#include "game/MoveList.h"
#include "game/TT.h"

namespace checkers {

constexpr int kMaxSearchPly = 64;        // hard cap on recursion depth (plies from the root)
constexpr int kMaxCaptureExtension = 8;  // extra plies per line for forced captures
constexpr int kMateScore = 30000;        // win at ply p scores kMateScore - p
constexpr int kSearchInfinity = 32000;   // above any reachable score
constexpr int kMaxPvLen = 32;            // PV is tracked for the first kMaxPvLen plies

// True for scores that encode a forced win or loss.
constexpr bool isMateScore(int score) {
  return score >= kMateScore - kMaxSearchPly || score <= -kMateScore + kMaxSearchPly;
}

struct SearchLimits {
  int maxDepth = 64;                  // clamped to [1, kMaxSearchPly]
  uint32_t nodeCheckInterval = 1024;  // StopFn polling period in nodes (0 is treated as 1)
  bool forcedCapture = true;          // rule variant, passed to legalMoves()
};

struct SearchResult {
  Move best{};             // best move of the last completed iteration; pathLen == 0 if none
  int score = 0;           // score of `best` for the side to move at the root
  int depth = 0;           // nominal depth of the last completed iteration (0 if none completed)
  uint32_t nodes = 0;      // nodes visited over all iterations, including an aborted one
  bool completed = false;  // false when the StopFn aborted an iteration

  bool hasMove() const { return best.pathLen != 0; }
};

// Returns true to stop the search. Polled with the ctx passed to search().
using StopFn = bool (*)(void*);

class Searcher {
 public:
  // Searches `root` (b.side to move). Resets all ordering state first, so the result depends
  // only on the arguments (deterministic, and identical on repeated calls) unless stop() fires.
  // If the root has no legal moves: best.pathLen == 0, score == -kMateScore, completed == true.
  // If stop() fires before depth 1 completes, best is the first move in search order, depth 0.
  // Iterative deepening ends early once a forced win or loss is proven within the depth searched.
  SearchResult search(const Board& root, const SearchLimits& limits = SearchLimits{},
                      const EvalWeights& weights = EvalWeights{}, StopFn stop = nullptr,
                      void* ctx = nullptr);

  // Transposition table to use from the next search() on; nullptr (the default) disables it and
  // the search behaves exactly as without one. The table is not owned and must outlive its use.
  void setTT(TranspositionTable* tt) { tt_ = tt; }
  TranspositionTable* tt() const { return tt_; }

 private:
  struct HashMove {
    uint8_t from = 0, to = 0, pathLen = 0;  // pathLen == 0: none
  };

  int negamax(const Board& b, uint64_t key, int depth, int alpha, int beta, int ply, int ext,
              bool onPv);
  void scoreMoves(const MoveList& list, int ply, Side side, bool onPv, HashMove hashMove);
  bool pollStop();
  void clearOrdering();

  // Settings for the current search.
  EvalWeights weights_{};
  StopFn stop_ = nullptr;
  void* ctx_ = nullptr;
  uint32_t checkInterval_ = 1024;
  uint32_t checkCountdown_ = 1024;
  bool forcedCapture_ = true;
  TranspositionTable* tt_ = nullptr;

  uint32_t nodes_ = 0;
  bool stopped_ = false;

  // Per-ply state; index = ply from the root.
  MoveList moves_[kMaxSearchPly];
  int32_t keys_[kMaxSearchPly][MoveList::kCapacity];
  Move killers_[kMaxSearchPly][2];
  int32_t history_[2][kNumSquares][kNumSquares];

  // Triangular PV of the current iteration and the PV of the last completed one.
  Move pv_[kMaxPvLen][kMaxPvLen];
  int pvLen_[kMaxPvLen + 1];
  Move prevPv_[kMaxPvLen];
  int prevPvLen_ = 0;
};

// Convenience wrapper over Searcher::search() using one function-local static Searcher. Not
// reentrant and it reserves sizeof(Searcher) of static memory once linked; on the device give the
// AI worker its own Searcher (static or PSRAM) and call Searcher::search() instead.
SearchResult search(const Board& root, const SearchLimits& limits, const EvalWeights& weights,
                    StopFn stop = nullptr, void* ctx = nullptr);

}  // namespace checkers
