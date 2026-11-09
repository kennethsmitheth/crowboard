#pragma once
// High scores: top-10 tables per difficulty plus Overall, and a persistent game counter (there is
// no RTC, so entries carry a game sequence number instead of a date). Persisted as one versioned
// blob in namespace "scores", key "v1".
//
// Blob payload: HighScoresData, packed, 564 bytes (4 * 10 * 14 + 4), little-endian, no padding.
// ScoreEntry is 14 bytes. Blob version 1; any layout change requires a new version (key "v2").
// load() treats a missing, corrupt or wrong-size blob as empty tables and returns false.
// Empty slots have score 0 and empty initials. A score of 0 never qualifies.
// Ties: the earlier entry stays ahead (a new equal score goes below existing equal scores).
#include <stddef.h>
#include <stdint.h>

#include "Blob.h"
#include "GameTypes.h"
#include "IStorage.h"

namespace checkers {

constexpr int kScoreEntries = 10;
constexpr int kScoreTables = 4;

enum class ScoreTable : uint8_t { Easy, Medium, Hard, Overall };

#pragma pack(push, 1)
struct ScoreEntry {
  char initials[4];  // exactly 3 printable ASCII chars plus NUL; "" for an empty slot
  uint32_t score;
  uint16_t moves;
  uint32_t gameNo;
};

struct HighScoresData {
  ScoreEntry tables[kScoreTables][kScoreEntries];
  uint32_t gameCounter;
};
#pragma pack(pop)

static_assert(sizeof(ScoreEntry) == 14, "ScoreEntry layout is part of blob version 1");
static_assert(sizeof(HighScoresData) == 564, "HighScoresData layout is part of blob version 1");
static_assert(sizeof(HighScoresData) <= Blob::kMaxPayload, "must fit one blob");

class HighScores {
 public:
  static constexpr const char* kNamespace = "scores";
  static constexpr const char* kKey = "v1";
  static constexpr uint32_t kMagic = 0x53434F52;  // "SCOR"
  static constexpr uint16_t kVersion = 1;

  explicit HighScores(IStorage& storage);

  // Reads the blob. On failure the tables are empty and false is returned.
  bool load();
  // Writes tables and counter. Returns false if the storage write or commit fails.
  bool save();

  // Increments and returns the game counter (first call returns 1). Persisted on save().
  uint32_t nextGameNo();

  // True if the score would enter the difficulty table or Overall.
  bool qualifies(Difficulty difficulty, uint32_t score) const;

  // Inserts into the difficulty table and Overall. Returns the 1-based rank in the difficulty
  // table, or 0 if not placed. overallRank (optional) receives the Overall rank or 0.
  // initials: first 3 chars, space-padded if shorter; non-printable chars become spaces.
  // Does not persist.
  int insert(Difficulty difficulty, uint32_t score, uint16_t moves, const char* initials,
             uint32_t gameNo, int* overallRank = nullptr);

  // index in [0, kScoreEntries); out-of-range returns an empty entry.
  const ScoreEntry& entry(ScoreTable table, int index) const;
  // Number of occupied slots in the table.
  int count(ScoreTable table) const;

  // Clears tables and the counter. Call save() to persist.
  void reset();

 private:
  IStorage& storage_;
  HighScoresData data_;
};

}  // namespace checkers
