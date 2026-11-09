// Stats service tests (host). src/services must not include Arduino/ESP-IDF headers.
#include <string.h>
#include <unity.h>

#include "services/Blob.h"
#include "services/GameTypes.h"
#include "services/RamStorage.h"
#include "services/Stats.h"

using namespace checkers;

static RamStorage ram;

void setUp(void) { ram.eraseNamespace(Stats::kNamespace); }
void tearDown(void) {}

static void assertRow(const DifficultyStats& row, uint16_t played, uint16_t won, uint16_t lost,
                      uint16_t drawn) {
  TEST_ASSERT_EQUAL_UINT16(played, row.played);
  TEST_ASSERT_EQUAL_UINT16(won, row.won);
  TEST_ASSERT_EQUAL_UINT16(lost, row.lost);
  TEST_ASSERT_EQUAL_UINT16(drawn, row.drawn);
}

static void assertAllZero(const StatsData& d) {
  const StatsData zero = StatsData();
  TEST_ASSERT_EQUAL_MEMORY(&zero, &d, sizeof(d));
}

static void test_empty_storage_is_zero(void) {
  Stats s(ram);
  TEST_ASSERT_FALSE(s.load());
  assertAllZero(s.data());
}

static void test_vs_computer_updates_right_row(void) {
  Stats s(ram);
  s.recordVsComputer(Difficulty::Medium, Outcome::Win, 30);
  s.recordVsComputer(Difficulty::Medium, Outcome::Loss, 20);
  s.recordVsComputer(Difficulty::Hard, Outcome::Draw, 50);
  const StatsData& d = s.data();
  assertRow(d.byDifficulty[0], 0, 0, 0, 0);
  assertRow(d.byDifficulty[1], 2, 1, 1, 0);
  assertRow(d.byDifficulty[2], 1, 0, 0, 1);
  TEST_ASSERT_EQUAL_UINT32(100, d.totalMoves);
  TEST_ASSERT_EQUAL_UINT16(0, d.twoPlayerGames);
}

static void test_streaks_win_win_loss_win(void) {
  Stats s(ram);
  s.recordVsComputer(Difficulty::Easy, Outcome::Win, 1);
  s.recordVsComputer(Difficulty::Easy, Outcome::Win, 1);
  TEST_ASSERT_EQUAL_UINT16(2, s.data().currentStreak);
  s.recordVsComputer(Difficulty::Easy, Outcome::Loss, 1);
  TEST_ASSERT_EQUAL_UINT16(0, s.data().currentStreak);
  s.recordVsComputer(Difficulty::Easy, Outcome::Win, 1);
  TEST_ASSERT_EQUAL_UINT16(1, s.data().currentStreak);
  TEST_ASSERT_EQUAL_UINT16(2, s.data().bestStreak);
}

static void test_draw_keeps_streak(void) {
  Stats s(ram);
  s.recordVsComputer(Difficulty::Hard, Outcome::Win, 1);
  s.recordVsComputer(Difficulty::Hard, Outcome::Draw, 1);
  TEST_ASSERT_EQUAL_UINT16(1, s.data().currentStreak);
  s.recordVsComputer(Difficulty::Hard, Outcome::Win, 1);
  TEST_ASSERT_EQUAL_UINT16(2, s.data().currentStreak);
  TEST_ASSERT_EQUAL_UINT16(2, s.data().bestStreak);
}

static void test_two_player_counted_separately(void) {
  Stats s(ram);
  s.recordTwoPlayer(40);
  s.recordTwoPlayer(10);
  s.recordVsComputer(Difficulty::Easy, Outcome::Win, 5);
  const StatsData& d = s.data();
  TEST_ASSERT_EQUAL_UINT16(2, d.twoPlayerGames);
  TEST_ASSERT_EQUAL_UINT32(55, d.totalMoves);
  assertRow(d.byDifficulty[0], 1, 1, 0, 0);
  TEST_ASSERT_EQUAL_UINT16(1, d.currentStreak);  // two-player games do not touch streaks
}

static void test_save_load_round_trip(void) {
  Stats in(ram);
  in.recordVsComputer(Difficulty::Easy, Outcome::Win, 12);
  in.recordVsComputer(Difficulty::Hard, Outcome::Loss, 7);
  in.recordTwoPlayer(33);
  TEST_ASSERT_TRUE(in.save());

  Stats out(ram);
  TEST_ASSERT_TRUE(out.load());
  TEST_ASSERT_EQUAL_MEMORY(&in.data(), &out.data(), sizeof(StatsData));
}

static void test_corrupt_blob_yields_zeros(void) {
  Stats s(ram);
  s.recordVsComputer(Difficulty::Easy, Outcome::Win, 9);
  TEST_ASSERT_TRUE(s.save());
  uint8_t* raw = ram.raw(Stats::kNamespace, Stats::kKey);
  TEST_ASSERT_NOT_NULL(raw);
  raw[Blob::kHeaderSize + 1] ^= 0x40;  // break the CRC

  Stats out(ram);
  out.recordTwoPlayer(1);  // load() must overwrite this
  TEST_ASSERT_FALSE(out.load());
  assertAllZero(out.data());
}

static void test_truncated_blob_yields_zeros(void) {
  Stats s(ram);
  s.recordVsComputer(Difficulty::Easy, Outcome::Win, 9);
  TEST_ASSERT_TRUE(s.save());
  TEST_ASSERT_TRUE(ram.truncate(Stats::kNamespace, Stats::kKey, Blob::kHeaderSize + 4));
  Stats out(ram);
  TEST_ASSERT_FALSE(out.load());
  assertAllZero(out.data());
}

static void test_reset_clears_everything(void) {
  Stats s(ram);
  s.recordVsComputer(Difficulty::Medium, Outcome::Win, 20);
  s.recordTwoPlayer(20);
  TEST_ASSERT_TRUE(s.save());
  s.reset();
  assertAllZero(s.data());
  TEST_ASSERT_TRUE(s.save());

  Stats out(ram);
  TEST_ASSERT_TRUE(out.load());
  assertAllZero(out.data());
}

static void test_saturates_at_max(void) {
  Stats s(ram);
  for (int i = 0; i < 65536; ++i) {
    s.recordVsComputer(Difficulty::Hard, Outcome::Win, 0xFFFF);
  }
  const StatsData& d = s.data();
  assertRow(d.byDifficulty[2], 65535, 65535, 0, 0);
  TEST_ASSERT_EQUAL_UINT16(65535, d.currentStreak);
  TEST_ASSERT_EQUAL_UINT16(65535, d.bestStreak);
  TEST_ASSERT_EQUAL_UINT32(0xFFFF0000u, d.totalMoves);  // 65536 * 65535, not yet saturated

  s.recordTwoPlayer(0xFFFF);
  s.recordTwoPlayer(0xFFFF);
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, s.data().totalMoves);
  s.recordTwoPlayer(0xFFFF);  // would wrap without saturation
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, s.data().totalMoves);

  for (int i = 0; i < 65536; ++i) s.recordTwoPlayer(0);
  TEST_ASSERT_EQUAL_UINT16(65535, s.data().twoPlayerGames);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_storage_is_zero);
  RUN_TEST(test_vs_computer_updates_right_row);
  RUN_TEST(test_streaks_win_win_loss_win);
  RUN_TEST(test_draw_keeps_streak);
  RUN_TEST(test_two_player_counted_separately);
  RUN_TEST(test_save_load_round_trip);
  RUN_TEST(test_corrupt_blob_yields_zeros);
  RUN_TEST(test_truncated_blob_yields_zeros);
  RUN_TEST(test_reset_clears_everything);
  RUN_TEST(test_saturates_at_max);
  return UNITY_END();
}
