// HighScores service tests (host). src/services must not include Arduino/ESP-IDF headers.
#include <string.h>
#include <unity.h>

#include "services/Blob.h"
#include "services/GameTypes.h"
#include "services/HighScores.h"
#include "services/RamStorage.h"

using namespace checkers;

static RamStorage ram;

void setUp(void) {
  ram.eraseNamespace(HighScores::kNamespace);
}
void tearDown(void) {}

static void assertAllEmpty(const HighScores& h) {
  for (int t = 0; t < kScoreTables; ++t) {
    const ScoreTable tab = static_cast<ScoreTable>(t);
    TEST_ASSERT_EQUAL_INT(0, h.count(tab));
    for (int i = 0; i < kScoreEntries; ++i) {
      TEST_ASSERT_EQUAL_UINT32(0, h.entry(tab, i).score);
      TEST_ASSERT_EQUAL_STRING("", h.entry(tab, i).initials);
    }
  }
}

static void test_empty_storage_gives_empty_tables(void) {
  HighScores h(ram);
  TEST_ASSERT_FALSE(h.load());
  assertAllEmpty(h);
}

static void test_insert_places_in_difficulty_and_overall(void) {
  HighScores h(ram);
  int overall = -1;
  TEST_ASSERT_EQUAL_INT(1, h.insert(Difficulty::Easy, 500, 20, "AAA", 1, &overall));
  TEST_ASSERT_EQUAL_INT(1, overall);
  TEST_ASSERT_EQUAL_INT(1, h.insert(Difficulty::Hard, 900, 30, "BBB", 2, &overall));
  TEST_ASSERT_EQUAL_INT(1, overall);
  TEST_ASSERT_EQUAL_INT(2, h.insert(Difficulty::Easy, 400, 25, "CCC", 3, &overall));
  TEST_ASSERT_EQUAL_INT(3, overall);
  TEST_ASSERT_EQUAL_INT(2, h.count(ScoreTable::Easy));
  TEST_ASSERT_EQUAL_INT(0, h.count(ScoreTable::Medium));
  TEST_ASSERT_EQUAL_INT(1, h.count(ScoreTable::Hard));
  TEST_ASSERT_EQUAL_INT(3, h.count(ScoreTable::Overall));
  TEST_ASSERT_EQUAL_STRING("BBB", h.entry(ScoreTable::Overall, 0).initials);
  TEST_ASSERT_EQUAL_UINT16(30, h.entry(ScoreTable::Overall, 0).moves);
  TEST_ASSERT_EQUAL_UINT32(2, h.entry(ScoreTable::Overall, 0).gameNo);
  TEST_ASSERT_EQUAL_STRING("CCC", h.entry(ScoreTable::Easy, 1).initials);
}

static void test_ordering_descending(void) {
  HighScores h(ram);
  const uint32_t scores[] = {300, 900, 100, 700, 500};
  for (uint32_t s : scores) h.insert(Difficulty::Medium, s, 10, "ABC", 1);
  const uint32_t expect[] = {900, 700, 500, 300, 100};
  for (int i = 0; i < 5; ++i) {
    TEST_ASSERT_EQUAL_UINT32(expect[i], h.entry(ScoreTable::Medium, i).score);
    TEST_ASSERT_EQUAL_UINT32(expect[i], h.entry(ScoreTable::Overall, i).score);
  }
}

static void test_ties_keep_earlier_ahead(void) {
  HighScores h(ram);
  h.insert(Difficulty::Easy, 500, 10, "OLD", 1);
  int overall = 0;
  TEST_ASSERT_EQUAL_INT(2, h.insert(Difficulty::Easy, 500, 10, "NEW", 2, &overall));
  TEST_ASSERT_EQUAL_INT(2, overall);
  TEST_ASSERT_EQUAL_STRING("OLD", h.entry(ScoreTable::Easy, 0).initials);
  TEST_ASSERT_EQUAL_STRING("NEW", h.entry(ScoreTable::Easy, 1).initials);
  TEST_ASSERT_EQUAL_STRING("OLD", h.entry(ScoreTable::Overall, 0).initials);
}

static void fillEasy(HighScores& h) {
  for (int i = 0; i < kScoreEntries; ++i) {
    h.insert(Difficulty::Easy, 1000 - i * 10, 10, "AAA", i + 1);  // 1000 .. 910
  }
}

static void test_lower_eleventh_not_inserted(void) {
  HighScores h(ram);
  fillEasy(h);
  TEST_ASSERT_FALSE(h.qualifies(Difficulty::Easy, 910));  // tie with 10th
  TEST_ASSERT_FALSE(h.qualifies(Difficulty::Easy, 100));
  int overall = -1;
  TEST_ASSERT_EQUAL_INT(0, h.insert(Difficulty::Easy, 910, 5, "ZZZ", 99, &overall));
  TEST_ASSERT_EQUAL_INT(0, overall);
  TEST_ASSERT_EQUAL_INT(10, h.count(ScoreTable::Easy));
  TEST_ASSERT_EQUAL_UINT32(910, h.entry(ScoreTable::Easy, 9).score);
  TEST_ASSERT_EQUAL_STRING("AAA", h.entry(ScoreTable::Easy, 9).initials);
}

static void test_higher_eleventh_evicts_tenth(void) {
  HighScores h(ram);
  fillEasy(h);
  TEST_ASSERT_TRUE(h.qualifies(Difficulty::Easy, 955));
  TEST_ASSERT_EQUAL_INT(6, h.insert(Difficulty::Easy, 955, 5, "ZZZ", 99));
  TEST_ASSERT_EQUAL_INT(10, h.count(ScoreTable::Easy));
  TEST_ASSERT_EQUAL_UINT32(920, h.entry(ScoreTable::Easy, 9).score);
  TEST_ASSERT_EQUAL_UINT32(955, h.entry(ScoreTable::Easy, 5).score);
}

static void test_qualifies_edge_cases(void) {
  HighScores h(ram);
  TEST_ASSERT_FALSE(h.qualifies(Difficulty::Easy, 0));
  TEST_ASSERT_TRUE(h.qualifies(Difficulty::Easy, 1));
  // Hard fills Overall with high scores; Medium is empty so a low score still qualifies there
  // (own table) even though it cannot reach Overall.
  for (int i = 0; i < kScoreEntries; ++i) h.insert(Difficulty::Hard, 1000 + i, 10, "HHH", i + 1);
  TEST_ASSERT_TRUE(h.qualifies(Difficulty::Medium, 10));
  int overall = -1;
  TEST_ASSERT_EQUAL_INT(1, h.insert(Difficulty::Medium, 10, 1, "MMM", 50, &overall));
  TEST_ASSERT_EQUAL_INT(0, overall);
}

static void test_qualifies_agrees_with_insert(void) {
  HighScores h(ram);
  const uint32_t seq[] = {50, 900, 0, 900, 10, 400, 400, 1, 700, 650, 300, 200, 100, 800, 5, 2000};
  uint32_t g = 0;
  for (uint32_t s : seq) {
    for (int d = 0; d < 3; ++d) {
      const Difficulty diff = static_cast<Difficulty>(d);
      const bool q = h.qualifies(diff, s);
      int overall = 0;
      const int rank = h.insert(diff, s, 1, "QQQ", ++g, &overall);
      TEST_ASSERT_EQUAL(q, rank > 0 || overall > 0);
    }
  }
}

static void test_initials_padding(void) {
  HighScores h(ram);
  h.insert(Difficulty::Easy, 400, 1, "AB", 1);
  h.insert(Difficulty::Easy, 300, 1, "", 2);
  h.insert(Difficulty::Easy, 200, 1, nullptr, 3);
  h.insert(Difficulty::Easy, 100, 1, "WXYZ", 4);
  TEST_ASSERT_EQUAL_STRING("AB ", h.entry(ScoreTable::Easy, 0).initials);
  TEST_ASSERT_EQUAL_STRING("   ", h.entry(ScoreTable::Easy, 1).initials);
  TEST_ASSERT_EQUAL_STRING("   ", h.entry(ScoreTable::Easy, 2).initials);
  TEST_ASSERT_EQUAL_STRING("WXY", h.entry(ScoreTable::Easy, 3).initials);
  TEST_ASSERT_EQUAL_CHAR('\0', h.entry(ScoreTable::Easy, 0).initials[3]);
}

static void test_game_counter_increments_and_persists(void) {
  HighScores h(ram);
  TEST_ASSERT_EQUAL_UINT32(1, h.nextGameNo());
  TEST_ASSERT_EQUAL_UINT32(2, h.nextGameNo());
  TEST_ASSERT_TRUE(h.save());
  HighScores h2(ram);
  TEST_ASSERT_TRUE(h2.load());
  TEST_ASSERT_EQUAL_UINT32(3, h2.nextGameNo());
}

static void test_save_load_round_trip(void) {
  HighScores h(ram);
  h.insert(Difficulty::Easy, 500, 20, "AAA", h.nextGameNo());
  h.insert(Difficulty::Hard, 1500, 33, "BBB", h.nextGameNo());
  TEST_ASSERT_TRUE(h.save());
  HighScores h2(ram);
  TEST_ASSERT_TRUE(h2.load());
  for (int t = 0; t < kScoreTables; ++t) {
    const ScoreTable tab = static_cast<ScoreTable>(t);
    TEST_ASSERT_EQUAL_INT(h.count(tab), h2.count(tab));
    for (int i = 0; i < kScoreEntries; ++i) {
      TEST_ASSERT_EQUAL_MEMORY(&h.entry(tab, i), &h2.entry(tab, i), sizeof(ScoreEntry));
    }
  }
  TEST_ASSERT_EQUAL_STRING("BBB", h2.entry(ScoreTable::Overall, 0).initials);
  TEST_ASSERT_EQUAL_UINT16(33, h2.entry(ScoreTable::Hard, 0).moves);
}

static void test_corrupt_blob_yields_empty(void) {
  HighScores h(ram);
  h.insert(Difficulty::Easy, 500, 20, "AAA", 1);
  TEST_ASSERT_TRUE(h.save());
  uint8_t raw[Blob::kHeaderSize + sizeof(HighScoresData)];
  size_t len = 0;
  TEST_ASSERT_TRUE(ram.get(HighScores::kNamespace, HighScores::kKey, raw, sizeof(raw), &len));
  raw[len - 1] ^= 0xFF;
  TEST_ASSERT_TRUE(ram.put(HighScores::kNamespace, HighScores::kKey, raw, len));
  HighScores h2(ram);
  h2.insert(Difficulty::Easy, 77, 1, "OLD", 1);  // stale in-memory data must be cleared
  TEST_ASSERT_FALSE(h2.load());
  assertAllEmpty(h2);
}

static void test_reset_clears_everything(void) {
  HighScores h(ram);
  h.insert(Difficulty::Medium, 500, 20, "AAA", h.nextGameNo());
  h.reset();
  assertAllEmpty(h);
  TEST_ASSERT_EQUAL_UINT32(1, h.nextGameNo());
  h.reset();
  TEST_ASSERT_TRUE(h.save());
  HighScores h2(ram);
  TEST_ASSERT_TRUE(h2.load());
  assertAllEmpty(h2);
  TEST_ASSERT_EQUAL_UINT32(1, h2.nextGameNo());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_storage_gives_empty_tables);
  RUN_TEST(test_insert_places_in_difficulty_and_overall);
  RUN_TEST(test_ordering_descending);
  RUN_TEST(test_ties_keep_earlier_ahead);
  RUN_TEST(test_lower_eleventh_not_inserted);
  RUN_TEST(test_higher_eleventh_evicts_tenth);
  RUN_TEST(test_qualifies_edge_cases);
  RUN_TEST(test_qualifies_agrees_with_insert);
  RUN_TEST(test_initials_padding);
  RUN_TEST(test_game_counter_increments_and_persists);
  RUN_TEST(test_save_load_round_trip);
  RUN_TEST(test_corrupt_blob_yields_empty);
  RUN_TEST(test_reset_clears_everything);
  return UNITY_END();
}
