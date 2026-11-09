// Pure text and outcome helpers for the Game Over scene (issue #36). No Arduino includes so the
// host tests can use them.
#pragma once

#include <stddef.h>
#include <stdio.h>

#include "../game/Game.h"
#include "../services/GameTypes.h"
#include "GameConfig.h"

namespace checkers::app {

// Human outcome of a vs-computer game. P1 is Black. An unfinished game counts as a draw.
inline Outcome humanOutcome(Result r, Side humanSide) {
  if (r == Result::Draw || r == Result::Ongoing) return Outcome::Draw;
  const Side winner = (r == Result::P1Wins) ? Side::Black : Side::White;
  return winner == humanSide ? Outcome::Win : Outcome::Loss;
}

inline const char* reasonText(Reason r) {
  switch (r) {
    case Reason::NoPieces:
      return "no pieces left";
    case Reason::NoMoves:
      return "no legal moves";
    case Reason::FortyMoveRule:
      return "40-move rule";
    case Reason::Repetition:
      return "threefold repetition";
    case Reason::Resign:
      return "resigned";
    case Reason::Agreement:
      return "agreed";
    case Reason::None:
      break;
  }
  return "";
}

// Writes the headline into out ("You win!", "Computer wins", "Draw", "<initials> wins!").
inline void headlineText(Result r, const GameConfig& cfg, char* out, size_t n) {
  if (cfg.mode == GameConfig::Mode::VsComputer) {
    switch (humanOutcome(r, cfg.humanSide)) {
      case Outcome::Win:
        snprintf(out, n, "You win!");
        return;
      case Outcome::Loss:
        snprintf(out, n, "Computer wins");
        return;
      case Outcome::Draw:
        snprintf(out, n, "Draw");
        return;
    }
  }
  if (r == Result::P1Wins || r == Result::P2Wins) {
    const char* in = (r == Result::P1Wins) ? cfg.p1Initials : cfg.p2Initials;
    char t[4] = {in[0], in[1], in[2], '\0'};
    for (int i = 2; i >= 0 && t[i] == ' '; --i) t[i] = '\0';
    snprintf(out, n, "%s wins!", t);
    return;
  }
  snprintf(out, n, "Draw");
}

}  // namespace checkers::app
