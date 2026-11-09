#pragma once
// Difficulty profiles (ARCHITECTURE §4, issue #18) and the small PRNG used for Easy's random pick.
// Pure C++17, no heap, no Arduino/ESP-IDF headers.
//
//   Easy    fixed depth 2; every root move is scored and one of the best 3 is picked at random
//           (AiRng, seeded by the caller: the Play scene seeds from millis(), tests use constants).
//   Medium  iterative deepening within 500 ms, no transposition table.
//   Hard    iterative deepening within 2500 ms with the worker's transposition table.
//
// Measured on the CrowPanel (docs/ai-notes.md): Medium reaches depth 6 (initial position) / 5
// (mid-game) and Hard depth 8 / 7.
#include <cstdint>

#include "services/GameTypes.h"

namespace checkers {

struct DifficultyProfile {
  Difficulty difficulty;
  const char* name;    // lower case, as typed in the serial "ai" command
  uint32_t budgetMs;   // wall-clock budget for one search; 0 = unlimited
  int maxDepth;        // iterative-deepening cap (clamped to [1, kMaxSearchPly])
  uint8_t randomTopN;  // > 1: rank the root moves and pick one of the best N at random
  bool useTT;          // search with the worker's transposition table
};

// Polling period of the stop function in nodes (see docs/ai-notes.md, experiment d).
constexpr uint32_t kAiNodeCheckInterval = 1024;

inline constexpr DifficultyProfile kDifficultyProfiles[kDifficultyCount] = {
    {Difficulty::Easy, "easy", 150, 2, 3, false},
    {Difficulty::Medium, "medium", 500, 64, 1, false},
    {Difficulty::Hard, "hard", 2500, 64, 1, true},
};

constexpr const DifficultyProfile& difficultyProfile(Difficulty d) {
  return kDifficultyProfiles[static_cast<uint8_t>(d) < kDifficultyCount ? static_cast<uint8_t>(d)
                                                                        : 1];
}

static_assert(kDifficultyProfiles[0].difficulty == Difficulty::Easy &&
                  kDifficultyProfiles[1].difficulty == Difficulty::Medium &&
                  kDifficultyProfiles[2].difficulty == Difficulty::Hard,
              "profile table is indexed by Difficulty");

// Small deterministic PRNG (xorshift32 over a splitmix-style scrambled seed). Same sequence on
// every platform for the same seed; any seed, including 0, is valid.
class AiRng {
 public:
  explicit constexpr AiRng(uint32_t seed) : state_(scramble(seed)) {}

  constexpr uint32_t next() {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
  }
  // Uniform-enough value in [0, n); 0 when n == 0.
  constexpr uint32_t below(uint32_t n) {
    return n == 0 ? 0 : static_cast<uint32_t>((static_cast<uint64_t>(next()) * n) >> 32);
  }

 private:
  static constexpr uint32_t scramble(uint32_t s) {
    s += 0x9E3779B9u;
    s = (s ^ (s >> 16)) * 0x85EBCA6Bu;
    s = (s ^ (s >> 13)) * 0xC2B2AE35u;
    s ^= s >> 16;
    return s == 0 ? 0x6D2B79F5u : s;  // xorshift must not start at 0
  }

  uint32_t state_;
};

}  // namespace checkers
