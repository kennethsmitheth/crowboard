// Settings chosen on the menus and read by the Play scene. Lives in SceneContext so that the
// Main Menu, Game Setup and Play scenes can share it without referring to each other.
#pragma once

#include <stdint.h>

#include "../game/Board.h"
#include "../services/GameTypes.h"

namespace checkers::app {

struct GameConfig {
  enum class Mode : uint8_t { VsComputer, TwoPlayers };

  Mode mode = Mode::VsComputer;
  Difficulty difficulty = Difficulty::Medium;
  Side humanSide = Side::Black;
  char p1Initials[4] = "P1 ";
  char p2Initials[4] = "P2 ";
  bool resume = false;  // Play loads the autosave instead of starting a new game
};

}  // namespace checkers::app
