// Parameters of the game about to be played, filled by the menu / setup scenes and read by Play.
#pragma once

#include <stdint.h>

#include "../services/GameTypes.h"
#include "../game/Board.h"

namespace checkers::app {

struct GameConfig {
  enum class Mode : uint8_t { VsComputer, TwoPlayers } mode = Mode::VsComputer;
  Difficulty difficulty = Difficulty::Medium;
  Side humanSide = Side::Black;
  char p1Initials[4] = "P1 ";
  char p2Initials[4] = "P2 ";
  bool resume = false;
};

}  // namespace checkers::app
