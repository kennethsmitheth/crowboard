// Selection and multi-jump continuation state machine of the Play scene (ARCHITECTURE §8).
// Header-only, pure C++17, no Arduino includes, so it is host-tested (test/test_playlogic).
//
// Squares: `tapped` is a 1-32 notation number (0 = not a playable square, as BoardGeometry's
// cellAt returns). Everything else is 0-based (Move::from / Move::path), and masks have bit i =
// 0-based square i, matching BoardViewState::legalTargets / forcedPieces.
//
// Rules:
// * Tapping an own piece that has a legal move selects it. legalMoves() already contains only
//   captures when a capture exists under forced capture, so only capturing pieces qualify.
// * Tapping a highlighted landing square extends the pending path. When a legal move's full
//   path equals the pending path the move resolves; otherwise the next landing squares of every
//   move sharing the prefix are highlighted (each landing of a multi-jump is one tap).
// * Tapping the selected piece again, or anything else, clears the selection and the path.
//   Tapping another selectable piece switches the selection to it (path cleared).
#pragma once

#include <stdint.h>

#include "../game/Move.h"
#include "../game/MoveList.h"

namespace checkers::app {

struct PlaySelection {
  int8_t from = -1;  // 0-based square of the selected piece, -1 for none
  uint8_t path[kMaxPathLen] = {};
  uint8_t pathLen = 0;  // landings entered so far

  bool active() const { return from >= 0; }
  void clear() {
    from = -1;
    pathLen = 0;
  }
};

struct PlayTapResult {
  PlaySelection sel;      // new selection (cleared when a move resolved)
  uint32_t targets = 0;   // landing squares to highlight for `sel`
  uint32_t forced = 0;    // pieces that must capture
  bool resolved = false;  // true: `move` is the legal move to play
  Move move{};
};

namespace playlogic {

inline uint32_t sqMask(int sq) {
  return (sq >= 0 && sq < 32) ? (uint32_t{1} << sq) : 0;
}

// True when m starts on sel.from and its path begins with sel.path[0..pathLen).
inline bool matches(const Move& m, const PlaySelection& sel) {
  if (!sel.active() || m.from != sel.from || m.pathLen < sel.pathLen) return false;
  for (int i = 0; i < sel.pathLen; ++i) {
    if (m.path[i] != sel.path[i]) return false;
  }
  return true;
}

}  // namespace playlogic

// Squares holding a piece with at least one legal move.
inline uint32_t movablePieces(const MoveList& moves) {
  uint32_t m = 0;
  for (const Move& mv : moves) m |= playlogic::sqMask(mv.from);
  return m;
}

// Pieces that must capture: the capturing pieces when every legal move is a capture (always
// the case under forced capture when one exists), else 0.
inline uint32_t forcedPieces(const MoveList& moves) {
  uint32_t m = 0;
  for (const Move& mv : moves) {
    if (!mv.isCapture()) return 0;
    m |= playlogic::sqMask(mv.from);
  }
  return m;
}

// Next landing squares of the moves that extend the selection's pending path.
inline uint32_t legalTargets(const MoveList& moves, const PlaySelection& sel) {
  uint32_t t = 0;
  for (const Move& mv : moves) {
    if (playlogic::matches(mv, sel) && mv.pathLen > sel.pathLen) {
      t |= playlogic::sqMask(mv.path[sel.pathLen]);
    }
  }
  return t;
}

// Applies one tap on square `tapped` (1-32, 0 = none) to the selection.
inline PlayTapResult playTap(const MoveList& moves, const PlaySelection& cur, int tapped) {
  PlayTapResult r;
  r.forced = forcedPieces(moves);
  const int sq = (tapped >= 1 && tapped <= 32) ? tapped - 1 : -1;
  const uint32_t b = playlogic::sqMask(sq);

  if (cur.active() && cur.pathLen < kMaxPathLen && (legalTargets(moves, cur) & b)) {
    PlaySelection next = cur;
    next.path[next.pathLen++] = static_cast<uint8_t>(sq);
    for (const Move& mv : moves) {
      if (playlogic::matches(mv, next) && mv.pathLen == next.pathLen) {
        r.resolved = true;
        r.move = mv;
        r.sel.clear();
        return r;
      }
    }
    r.sel = next;
    r.targets = legalTargets(moves, next);
    return r;
  }

  const bool reTapSelected = cur.active() && sq == cur.from;
  if (!reTapSelected && (movablePieces(moves) & b)) {
    r.sel.from = static_cast<int8_t>(sq);
    r.sel.pathLen = 0;
    r.targets = legalTargets(moves, r.sel);
  }
  return r;  // otherwise the selection is cleared
}

}  // namespace checkers::app
