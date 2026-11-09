#include "Stats.h"

#include <string.h>

#include "Blob.h"

namespace checkers {

namespace {

constexpr uint16_t kMax16 = 0xFFFF;
constexpr uint32_t kMax32 = 0xFFFFFFFFu;

void bump16(uint16_t& v) {
  if (v != kMax16) ++v;
}

void add32(uint32_t& v, uint32_t by) { v = (by > kMax32 - v) ? kMax32 : v + by; }

}  // namespace

Stats::Stats(IStorage& storage) : storage_(storage) { memset(&data_, 0, sizeof(data_)); }

bool Stats::load() {
  StatsData in;
  size_t len = 0;
  if (!Blob::load(storage_, kNamespace, kKey, kMagic, kVersion, &in, sizeof(in), &len) ||
      len != sizeof(in)) {
    reset();
    return false;
  }
  data_ = in;
  return true;
}

bool Stats::save() {
  return Blob::save(storage_, kNamespace, kKey, kMagic, kVersion, &data_, sizeof(data_));
}

void Stats::recordVsComputer(Difficulty difficulty, Outcome outcome, uint16_t moves) {
  const auto idx = static_cast<uint8_t>(difficulty);
  if (idx >= kDifficultyCount) return;
  DifficultyStats& row = data_.byDifficulty[idx];
  bump16(row.played);
  switch (outcome) {
    case Outcome::Win:
      bump16(row.won);
      bump16(data_.currentStreak);
      if (data_.currentStreak > data_.bestStreak) data_.bestStreak = data_.currentStreak;
      break;
    case Outcome::Loss:
      bump16(row.lost);
      data_.currentStreak = 0;
      break;
    case Outcome::Draw:
      bump16(row.drawn);
      break;
  }
  add32(data_.totalMoves, moves);
}

void Stats::recordTwoPlayer(uint16_t moves) {
  bump16(data_.twoPlayerGames);
  add32(data_.totalMoves, moves);
}

void Stats::reset() { memset(&data_, 0, sizeof(data_)); }

}  // namespace checkers
