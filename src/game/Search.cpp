#if defined(__XTENSA__)
#pragma GCC optimize("O2")
#endif
#include "game/Search.h"

#include "game/Apply.h"
#include "game/MoveGen.h"
#include "game/Zobrist.h"

namespace checkers {

namespace {

// Ordering key bands (higher is searched first). History values stay below kKeyKiller.
constexpr int32_t kKeyPv = int32_t{1} << 30;
constexpr int32_t kKeyHash = kKeyPv - 1;
constexpr int32_t kKeyCapture = int32_t{1} << 29;
constexpr int32_t kKeyPromotion = int32_t{1} << 28;
constexpr int32_t kKeyKiller = int32_t{1} << 27;
constexpr int32_t kHistoryMax = int32_t{1} << 24;

// Depth stored for terminal positions: their score holds at any depth.
constexpr uint8_t kTerminalDepth = 255;

// Mate scores are stored relative to the node ("mate in n from here") and converted back to
// "mate in n from the root" when probed at another ply.
int scoreToTT(int score, int ply) {
  if (score >= kMateScore - kMaxSearchPly) return score + ply;
  if (score <= -kMateScore + kMaxSearchPly) return score - ply;
  return score;
}

int scoreFromTT(int score, int ply) {
  if (score >= kMateScore - kMaxSearchPly) return score - ply;
  if (score <= -kMateScore + kMaxSearchPly) return score + ply;
  return score;
}

void storeTT(TranspositionTable* tt, uint64_t key, int depth, int score, int ply, TTFlag flag,
             const Move* best) {
  TTEntry e{};
  e.score = static_cast<int16_t>(scoreToTT(score, ply));
  e.depth = static_cast<uint8_t>(depth);
  e.flag = flag;
  if (best != nullptr) {
    e.from = best->from;
    e.to = best->to;
    e.pathLen = best->pathLen;
  }
  tt->store(key, e);
}

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

void Searcher::scoreMoves(const MoveList& list, int ply, Side side, bool onPv, HashMove hashMove) {
  const bool havePv = onPv && ply < prevPvLen_;
  bool haveHash = hashMove.pathLen != 0;
  const int s = sideIndex(side);
  for (size_t i = 0; i < list.size(); ++i) {
    const Move& m = list[i];
    int32_t key;
    if (havePv && m == prevPv_[ply]) {
      key = kKeyPv;
    } else if (haveHash && m.from == hashMove.from && m.to == hashMove.to &&
               m.pathLen == hashMove.pathLen) {
      key = kKeyHash;
      haveHash = false;  // the first match only
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

int Searcher::negamax(const Board& b, uint64_t key, int depth, int alpha, int beta, int ply,
                      int ext, bool onPv) {
  ++nodes_;
  if (pollStop()) stopped_ = true;
  if (stopped_) return 0;
  if (ply < kMaxPvLen) pvLen_[ply] = ply;
  if (ply >= kMaxSearchPly) return evaluate(b, weights_);

  HashMove hashMove;
  if (tt_ != nullptr) {
    TTEntry e;
    if (tt_->probe(key, e)) {
      hashMove = HashMove{e.from, e.to, e.pathLen};
      // Never cut at the root: search() needs a move from this iteration.
      if (ply > 0 && e.depth >= depth) {
        const int s = scoreFromTT(e.score, ply);
        if (e.flag == TTFlag::Exact) return s;
        if (e.flag == TTFlag::Lower && s > alpha) alpha = s;
        if (e.flag == TTFlag::Upper && s < beta) beta = s;
        if (alpha >= beta) return s;
      }
    }
  }

  // Leaf without a capture for the side to move: no forced-capture extension is possible, so
  // decide terminal / evaluate from bitboards without generating the moves (same result as the
  // general path below).
  if (depth == 0 && !hasCapture(b)) {
    if (countSimpleMoves(b) == 0) {
      const int s = -kMateScore + ply;
      if (tt_ != nullptr) storeTT(tt_, key, kTerminalDepth, s, ply, TTFlag::Exact, nullptr);
      return s;
    }
    const int s = evaluate(b, weights_);
    if (tt_ != nullptr) storeTT(tt_, key, 0, s, ply, TTFlag::Exact, nullptr);
    return s;
  }

  MoveList& list = moves_[ply];
  list.clear();
  legalMoves(b, list, forcedCapture_);
  if (list.empty()) {
    const int s = -kMateScore + ply;
    if (tt_ != nullptr) storeTT(tt_, key, kTerminalDepth, s, ply, TTFlag::Exact, nullptr);
    return s;
  }

  int childDepth = depth - 1;
  int childExt = ext;
  if (ext < kMaxCaptureExtension && allCaptures(list)) {
    childDepth = depth;  // forced sequence: do not reduce depth
    childExt = ext + 1;
  }
  if (childDepth < 0) {  // depth 0 and not inside a forced sequence
    const int s = evaluate(b, weights_);
    if (tt_ != nullptr) storeTT(tt_, key, 0, s, ply, TTFlag::Exact, nullptr);
    return s;
  }

  scoreMoves(list, ply, b.side, onPv, hashMove);
  int32_t* keys = keys_[ply];
  const size_t n = list.size();
  const bool pvSlot = onPv && ply < prevPvLen_;

  const int alphaStart = alpha;  // after any TT narrowing: the bound flag is relative to it
  int best = -kSearchInfinity;
  size_t bestIndex = 0;
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
    const uint64_t childKey = tt_ != nullptr ? zobristApply(key, b, m) : 0;
    const int score =
        -negamax(child, childKey, childDepth, -beta, -alpha, ply + 1, childExt, childOnPv);
    if (stopped_) return 0;

    if (score > best) {
      best = score;
      bestIndex = i;
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

  if (tt_ != nullptr) {
    const TTFlag flag = best <= alphaStart ? TTFlag::Upper
                        : best >= beta     ? TTFlag::Lower
                                           : TTFlag::Exact;
    storeTT(tt_, key, depth, best, ply, flag, flag == TTFlag::Upper ? nullptr : &list[bestIndex]);
  }
  return best;
}

void Searcher::begin(const SearchLimits& limits, const EvalWeights& weights, StopFn stop,
                     void* ctx) {
  weights_ = weights;
  stop_ = stop;
  ctx_ = ctx;
  checkInterval_ = limits.nodeCheckInterval == 0 ? 1 : limits.nodeCheckInterval;
  checkCountdown_ = checkInterval_;
  forcedCapture_ = limits.forcedCapture;
  nodes_ = 0;
  stopped_ = false;
  clearOrdering();
}

size_t Searcher::rankRootMoves(const Board& root, ScoredMove* out, size_t maxOut,
                               const SearchLimits& limits, const EvalWeights& weights, StopFn stop,
                               void* ctx) {
  begin(limits, weights, stop, ctx);
  TranspositionTable* const savedTT = tt_;
  tt_ = nullptr;

  int depth = limits.maxDepth;
  if (depth < 1) depth = 1;
  if (depth > kMaxSearchPly) depth = kMaxSearchPly;

  // moves_[0] is the root list; negamax() at ply >= 1 never touches it.
  MoveList& rootMoves = moves_[0];
  rootMoves.clear();
  legalMoves(root, rootMoves, forcedCapture_);
  ++nodes_;
  int childDepth = depth - 1;
  int childExt = 0;
  if (allCaptures(rootMoves)) {
    childDepth = depth;
    childExt = 1;
  }

  size_t count = 0;
  for (size_t i = 0; i < rootMoves.size(); ++i) {
    const Move m = rootMoves[i];
    const int score = -negamax(applyMove(root, m), 0, childDepth, -kSearchInfinity, kSearchInfinity,
                               1, childExt, false);
    if (stopped_) break;
    // Insertion into the sorted output; strict > keeps generation order among equal scores.
    size_t pos = count < maxOut ? count : maxOut;
    while (pos > 0 && score > out[pos - 1].score) --pos;
    if (pos >= maxOut) continue;
    const size_t last = count < maxOut ? count : maxOut - 1;
    for (size_t k = last; k > pos; --k) out[k] = out[k - 1];
    out[pos] = ScoredMove{m, score};
    if (count < maxOut) ++count;
  }
  tt_ = savedTT;
  return count;
}

SearchResult Searcher::search(const Board& root, const SearchLimits& limits,
                              const EvalWeights& weights, StopFn stop, void* ctx) {
  begin(limits, weights, stop, ctx);

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
    scoreMoves(rootMoves, 0, root.side, false, HashMove{});
    size_t bi = 0;
    for (size_t j = 1; j < rootMoves.size(); ++j) {
      if (keys_[0][j] > keys_[0][bi]) bi = j;
    }
    result.best = rootMoves[bi];
  }

  const uint64_t rootKey = tt_ != nullptr ? zobristHash(root) : 0;
  for (int depth = 1; depth <= maxDepth; ++depth) {
    const int score = negamax(root, rootKey, depth, -kSearchInfinity, kSearchInfinity, 0, 0, true);
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
