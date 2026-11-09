#pragma once
// Game statistics: played/won/lost/drawn per difficulty, two-player count, win streaks and total
// moves. Persisted as one versioned blob in namespace "stats", key "v1".
//
// Blob payload: StatsData, packed, 34 bytes, little-endian, no padding.
// Blob version 1. Any change to StatsData's layout requires a new version (and key "v2").
// load() treats a missing, corrupt or wrong-size blob as all zeros and returns false.
// All counters saturate at their maximum instead of wrapping.
#include <stddef.h>
#include <stdint.h>

#include "GameTypes.h"
#include "IStorage.h"

namespace checkers {

#pragma pack(push, 1)
struct DifficultyStats {
  uint16_t played;
  uint16_t won;
  uint16_t lost;
  uint16_t drawn;
};

struct StatsData {
  DifficultyStats byDifficulty[kDifficultyCount];
  uint16_t twoPlayerGames;
  uint16_t currentStreak;  // consecutive vs-computer wins; reset by a loss, kept by a draw
  uint16_t bestStreak;
  uint32_t totalMoves;  // moves across vs-computer and two-player games
};
#pragma pack(pop)

static_assert(sizeof(StatsData) == 34, "StatsData layout is part of blob version 1");

class Stats {
 public:
  static constexpr const char* kNamespace = "stats";
  static constexpr const char* kKey = "v1";
  static constexpr uint32_t kMagic = 0x53544154;  // "STAT"
  static constexpr uint16_t kVersion = 1;

  explicit Stats(IStorage& storage);

  // Reads the blob. On failure, data() is all zeros and false is returned.
  bool load();
  // Writes the current data. Returns false if the storage write or commit fails.
  bool save();

  const StatsData& data() const { return data_; }

  // Updates one difficulty row, streaks and total moves. Does not persist.
  void recordVsComputer(Difficulty difficulty, Outcome outcome, uint16_t moves);
  // Counts a two-player game and its moves. Does not affect streaks. Does not persist.
  void recordTwoPlayer(uint16_t moves);
  // Clears all in-memory counters. Call save() to persist the cleared state.
  void reset();

 private:
  IStorage& storage_;
  StatsData data_;
};

}  // namespace checkers
