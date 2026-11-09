#include "services/Scoring.h"

namespace checkers {

namespace {
constexpr uint32_t popcount32(uint32_t x) {
  uint32_t n = 0;
  for (; x != 0; x &= x - 1) ++n;
  return n;
}
}  // namespace

ScoreBreakdown computeScore(Outcome outcome, Difficulty difficulty, const Board& finalBoard,
                            Side humanSide, uint16_t humanMovesPlayed, uint32_t elapsedSeconds,
                            bool forcedCapture) {
  ScoreBreakdown s{0, 0, 0, 0, 0, false};
  if (outcome == Outcome::Loss || !forcedCapture) return s;

  const uint32_t m = difficultyMultiplier(difficulty);
  s.result = (outcome == Outcome::Win ? 1000u : 200u) * m;
  const int i = sideIndex(humanSide);
  s.material = 50u * popcount32(finalBoard.men[i]) + 100u * popcount32(finalBoard.kings[i]);
  s.efficiency = humanMovesPlayed < 60 ? 10u * (60u - humanMovesPlayed) : 0u;
  s.speed = elapsedSeconds < 600 ? (600u - elapsedSeconds) / 2u : 0u;
  s.total = s.result + s.material + s.efficiency + s.speed;
  s.recorded = true;
  return s;
}

}  // namespace checkers
