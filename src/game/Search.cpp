#include "game/Search.h"

#include "game/Apply.h"
#include "game/MoveGen.h"

namespace checkers {

namespace {

// Ordering key bands (higher is searched first). History values stay below kKeyKiller.
constexpr int32_t kKeyPv = int32_t{1} << 30;
constexpr int32_t kKeyCapture = int32_t{1} << 29;
constexpr int32_t kKeyPromotion = int32_t{1} << 28;
constexpr int32_t kKeyKiller = int32_t{1} << 27;
constexpr int32_t kHistoryMax = int32_t{1} << 24;

bool allCaptures(const MoveList& list) {
  for (const Move& m : list) {
    if (!m.isCapture()) return false;
  }
  return true;
}

}  // namespace

void Searcher::clearOrdering() {
  for (int p = 0; p < kMaxSearchPly; ++p) {
    killers_[p][0] = Move{};
    killers_[p][1] = Move{};
  }
  for (auto& side : history_) {
    for (auto& from : side) {
      for (int32_t& h : from) h = 0;
    }
  }
  prevPvLen_ = 0;
}

bool Searcher::pollStop() {
  if (stop_ == nullptr) return false;
  if (--checkCountdown_ != 0) return false;
  checkCountdown_ = checkInterval_;
  return stop_(ctx_);
}

void Searcher::scoreMoves(const MoveList& list, int ply, Side side, bool onPv) {
  const bool havePv = onPv && ply < prevPvLen_;
  const int s = sideIndex(side);
  for (size_t i = 0; i < list.size(); ++i) {
    const Move& m = list[i];
    int32_t key;
    if (havePv && m == prevPv_[ply]) {
      key = kKeyPv;
    } else if (m.isCapture()) {
      key = kKeyCapture + m.captureCount() * 2 + (m.promotes ? 1 : 0);
    } else if (m.promotes) {
      key = kKeyPromotion;
    } else if (m == killers_[ply][0]) {
      key = kKeyKiller + 1;
    } else if (m == killers_[ply][1]) {
      key = kKeyKiller;
    } else {
      key = history_[s][m.from][m.to];
    }
    keys_[ply][i] = key;
  }
}

int Searcher::negamax(const Board& b, int depth, int alpha, int beta, int ply, int ext, bool onPv) {
  ++nodes_;
  if (pollStop()) stopped_ = true;
  if (stopped_) return 0;
  if (ply < kMaxPvLen) pvLen_[ply] = ply;
  if (ply >= kMaxSearchPly) return evaluate(b, weights_);

  // Hook for #16: probe the transposition table here (cutoff on a sufficient-depth entry, and
  // pass its move to scoreMoves() as the hash move).

  MoveList& list = moves_[ply];
  list.clear();
  legalMoves(b, list, forcedCapture_);
  if (list.empty()) return -kMateScore + ply;

  int childDepth = depth - 1;
  int childExt = ext;
  if (ext < kMaxCaptureExtension && allCaptures(list)) {
    childDepth = depth;  // forced sequence: do not reduce depth
    childExt = ext + 1;
  }
  if (childDepth < 0) return evaluate(b, weights_);  // depth 0 and not inside a forced sequence

  scoreMoves(list, ply, b.side, onPv);
  int32_t* keys = keys_[ply];
  const size_t n = list.size();
  const bool pvSlot = onPv && ply < prevPvLen_;

  int best = -kSearchInfinity;
  for (size_t i = 0; i < n; ++i) {
    // Selection step: bring the highest remaining key to i (first maximum wins: deterministic).
    size_t bi = i;
    for (size_t j = i + 1; j < n; ++j) {
      if (keys[j] > keys[bi]) bi = j;
    }
    if (bi != i) {
      const Move tm = list[i];
      list[i] = list[bi];
      list[bi] = tm;
      const int32_t tk = keys[i];
      keys[i] = keys[bi];
      keys[bi] = tk;
    }
    const Move& m = list[i];
    const bool childOnPv = pvSlot && keys[i] == kKeyPv;

    const Board child = applyMove(b, m);
    const int score = -negamax(child, childDepth, -beta, -alpha, ply + 1, childExt, childOnPv);
    if (stopped_) return 0;

    if (score > best) {
      best = score;
      if (score > alpha) {
        alpha = score;
        if (ply < kMaxPvLen) {
          pv_[ply][ply] = m;
          if (ply + 1 < kMaxPvLen) {
            const int len = pvLen_[ply + 1];
            for (int k = ply + 1; k < len; ++k) pv_[ply][k] = pv_[ply + 1][k];
            pvLen_[ply] = len;
          } else {
            pvLen_[ply] = ply + 1;
          }
        }
      }
      if (score >= beta) {
        if (!m.isCapture()) {
          if (m != killers_[ply][0]) {
            killers_[ply][1] = killers_[ply][0];
            killers_[ply][0] = m;
          }
          int32_t& h = history_[sideIndex(b.side)][m.from][m.to];
          h += (depth + 1) * (depth + 1);
          if (h > kHistoryMax) {
            for (auto& side : history_) {
              for (auto& from : side) {
                for (int32_t& v : from) v /= 2;
              }
            }
          }
        }
        break;
      }
    }
  }

  // Hook for #16: store (hash, depth, best, bound, best move) in the transposition table here.
  return best;
}

SearchResult Searcher::search(const Board& root, const SearchLimits& limits,
                              const EvalWeights& weights, StopFn stop, void* ctx) {
  weights_ = weights;
  stop_ = stop;
  ctx_ = ctx;
  checkInterval_ = limits.nodeCheckInterval == 0 ? 1 : limits.nodeCheckInterval;
  checkCountdown_ = checkInterval_;
  forcedCapture_ = limits.forcedCapture;
  nodes_ = 0;
  stopped_ = false;
  clearOrdering();

  int maxDepth = limits.maxDepth;
  if (maxDepth < 1) maxDepth = 1;
  if (maxDepth > kMaxSearchPly) maxDepth = kMaxSearchPly;

  SearchResult result;
  {
    MoveList& rootMoves = moves_[0];
    rootMoves.clear();
    legalMoves(root, rootMoves, forcedCapture_);
    if (rootMoves.empty()) {
      result.score = -kMateScore;
      result.completed = true;
      return result;
    }
    // Fallback if the very first iteration is aborted: the first move in search order.
    scoreMoves(rootMoves, 0, root.side, false);
    size_t bi = 0;
    for (size_t j = 1; j < rootMoves.size(); ++j) {
      if (keys_[0][j] > keys_[0][bi]) bi = j;
    }
    result.best = rootMoves[bi];
  }

  for (int depth = 1; depth <= maxDepth; ++depth) {
    const int score = negamax(root, depth, -kSearchInfinity, kSearchInfinity, 0, 0, true);
    if (stopped_) break;

    result.best = pv_[0][0];
    result.score = score;
    result.depth = depth;
    prevPvLen_ = pvLen_[0];
    for (int k = 0; k < prevPvLen_; ++k) prevPv_[k] = pv_[0][k];

    // A win or loss proven within `depth` plies cannot change with more depth.
    if (isMateScore(score)) {
      const int dist = kMateScore - (score < 0 ? -score : score);
      if (dist <= depth) break;
    }
  }
  result.nodes = nodes_;
  result.completed = !stopped_;
  return result;
}

SearchResult search(const Board& root, const SearchLimits& limits, const EvalWeights& weights,
                    StopFn stop, void* ctx) {
  static Searcher searcher;
  return searcher.search(root, limits, weights, stop, ctx);
}

}  // namespace checkers
