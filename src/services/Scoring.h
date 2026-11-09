#pragma once
// Scoring formula (ARCHITECTURE §11). One pure function owns the formula; the Game Over scene
// displays the breakdown. Pure C++17: no Arduino/ESP-IDF headers.
#include <stdint.h>

#include "game/Board.h"
#include "services/GameTypes.h"

namespace checkers {

// Difficulty multiplier M: Easy 1, Medium 2, Hard 3.
constexpr uint8_t difficultyMultiplier(Difficulty d) {
  return d == Difficulty::Easy ? 1 : (d == Difficulty::Medium ? 2 : 3);
}

struct ScoreBreakdown {
  uint32_t result;      // Win 1000 x M, Draw 200 x M, Loss 0
  uint32_t material;    // +50 per own man, +100 per own king left
  uint32_t efficiency;  // +10 x max(0, 60 - moves played by the human)
  uint32_t speed;       // +max(0, 600 - elapsed seconds) / 2
  uint32_t total;       // sum of the parts when recorded, else 0
  bool recorded;        // false for losses and for games without forced capture
};

// Components are all 0 when !recorded (loss or casual game).
ScoreBreakdown computeScore(Outcome outcome, Difficulty difficulty, const Board& finalBoard,
                            Side humanSide, uint16_t humanMovesPlayed, uint32_t elapsedSeconds,
                            bool forcedCapture = true);

}  // namespace checkers
