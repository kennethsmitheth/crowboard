#include "HighScores.h"

#include <string.h>

namespace checkers {

namespace {

const ScoreEntry kEmptyEntry = {{0, 0, 0, 0}, 0, 0, 0};

int tableIndex(Difficulty d) {
  const auto i = static_cast<uint8_t>(d);
  return i < kDifficultyCount ? i : -1;
}

// 0-based position for a new score, or kScoreEntries if it does not make the table.
// Strictly greater: an equal score goes below existing equal ones.
int position(const ScoreEntry* t, uint32_t score) {
  if (score == 0) return kScoreEntries;
  for (int i = 0; i < kScoreEntries; ++i) {
    if (score > t[i].score) return i;
  }
  return kScoreEntries;
}

int place(ScoreEntry* t, const ScoreEntry& e) {
  const int pos = position(t, e.score);
  if (pos >= kScoreEntries) return 0;
  for (int i = kScoreEntries - 1; i > pos; --i) t[i] = t[i - 1];
  t[pos] = e;
  return pos + 1;
}

constexpr int kOverall = static_cast<int>(ScoreTable::Overall);

}  // namespace

HighScores::HighScores(IStorage& storage) : storage_(storage) {
  memset(&data_, 0, sizeof(data_));
}

bool HighScores::load() {
  HighScoresData in;
  size_t len = 0;
  if (!Blob::load(storage_, kNamespace, kKey, kMagic, kVersion, &in, sizeof(in), &len) ||
      len != sizeof(in)) {
    reset();
    return false;
  }
  for (auto& table : in.tables) {
    for (auto& e : table) e.initials[3] = '\0';
  }
  data_ = in;
  return true;
}

bool HighScores::save() {
  return Blob::save(storage_, kNamespace, kKey, kMagic, kVersion, &data_, sizeof(data_));
}

uint32_t HighScores::nextGameNo() {
  if (data_.gameCounter != 0xFFFFFFFFu) ++data_.gameCounter;
  return data_.gameCounter;
}

bool HighScores::qualifies(Difficulty difficulty, uint32_t score) const {
  const int t = tableIndex(difficulty);
  if (t < 0) return false;
  return position(data_.tables[t], score) < kScoreEntries ||
         position(data_.tables[kOverall], score) < kScoreEntries;
}

int HighScores::insert(Difficulty difficulty, uint32_t score, uint16_t moves, const char* initials,
                       uint32_t gameNo, int* overallRank) {
  if (overallRank) *overallRank = 0;
  const int t = tableIndex(difficulty);
  if (t < 0) return 0;
  ScoreEntry e;
  memset(&e, 0, sizeof(e));
  e.score = score;
  e.moves = moves;
  e.gameNo = gameNo;
  bool ended = (initials == nullptr);
  for (int i = 0; i < 3; ++i) {
    if (!ended && initials[i] == '\0') ended = true;
    const char c = ended ? ' ' : initials[i];
    e.initials[i] = (c >= 0x20 && c <= 0x7E) ? c : ' ';
  }
  const int rank = place(data_.tables[t], e);
  const int overall = place(data_.tables[kOverall], e);
  if (overallRank) *overallRank = overall;
  return rank;
}

const ScoreEntry& HighScores::entry(ScoreTable table, int index) const {
  const auto t = static_cast<uint8_t>(table);
  if (t >= kScoreTables || index < 0 || index >= kScoreEntries) return kEmptyEntry;
  return data_.tables[t][index];
}

int HighScores::count(ScoreTable table) const {
  const auto t = static_cast<uint8_t>(table);
  if (t >= kScoreTables) return 0;
  int n = 0;
  while (n < kScoreEntries && data_.tables[t][n].score > 0) ++n;
  return n;
}

void HighScores::reset() {
  memset(&data_, 0, sizeof(data_));
}

}  // namespace checkers
