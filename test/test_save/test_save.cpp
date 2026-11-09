// SaveGame service tests (host). Uses RamStorage; no Arduino or ESP-IDF headers.
#include <string.h>
#include <unity.h>

#include "game/Game.h"
#include "services/Blob.h"
#include "services/RamStorage.h"
#include "services/SaveGame.h"

using namespace checkers;

static RamStorage ram;

void setUp(void) {
  ram.eraseNamespace(SaveGame::kNamespace);
}
void tearDown(void) {}

static SaveHeader makeHeader() {
  SaveHeader h{};
  h.mode = 0;
  h.difficulty = 2;
  h.humanSide = 1;
  h.forcedCapture = 1;
  h.p1ClockSec = 123;
  h.p2ClockSec = 4567;
  return h;
}

// Plays up to `plies` moves, preferring captures, otherwise the first legal move.
// Returns the number of captures played.
static int playSome(Game& g, int plies) {
  int captures = 0;
  for (int i = 0; i < plies && !g.isOver(); ++i) {
    const MoveList& l = g.legalMoves();
    Move pick = l[0];
    for (const Move& m : l) {
      if (m.isCapture()) {
        pick = m;
        break;
      }
    }
    if (pick.isCapture()) ++captures;
    TEST_ASSERT_TRUE(g.apply(pick));
  }
  return captures;
}

static void assertSameGame(const Game& a, const Game& b) {
  TEST_ASSERT_TRUE(a.board() == b.board());
  TEST_ASSERT_EQUAL(static_cast<int>(a.sideToMove()), static_cast<int>(b.sideToMove()));
  TEST_ASSERT_EQUAL_UINT16(a.ply(), b.ply());
  TEST_ASSERT_EQUAL_UINT32(a.captured(Side::Black), b.captured(Side::Black));
  TEST_ASSERT_EQUAL_UINT32(a.captured(Side::White), b.captured(Side::White));
}

static void test_capacity(void) {
  TEST_ASSERT_TRUE(SaveGame::kMaxSavedMoves >= kMaxPlies);
  TEST_ASSERT_TRUE(SaveGame::kHeaderBytes + 3 * kMaxPlies <= Blob::kMaxPayload);
}

static void test_exists_false_when_empty(void) {
  SaveGame s(ram);
  TEST_ASSERT_FALSE(s.exists());
  Game g;
  SaveHeader h{};
  TEST_ASSERT_FALSE(s.load(g, h));
}

static void test_round_trip(void) {
  Game g;
  int captures = playSome(g, 40);
  TEST_ASSERT_TRUE(captures > 0);
  SaveGame s(ram);
  const SaveHeader in = makeHeader();
  TEST_ASSERT_TRUE(s.save(g, in));
  TEST_ASSERT_TRUE(s.exists());

  Game loaded;
  SaveHeader out{};
  TEST_ASSERT_TRUE(s.load(loaded, out));
  assertSameGame(g, loaded);
  TEST_ASSERT_EQUAL_UINT8(in.mode, out.mode);
  TEST_ASSERT_EQUAL_UINT8(in.difficulty, out.difficulty);
  TEST_ASSERT_EQUAL_UINT8(in.humanSide, out.humanSide);
  TEST_ASSERT_EQUAL_UINT8(1, out.forcedCapture);
  TEST_ASSERT_EQUAL_UINT32(123, out.p1ClockSec);
  TEST_ASSERT_EQUAL_UINT32(4567, out.p2ClockSec);
  TEST_ASSERT_EQUAL_UINT16(g.historySize(), out.moveCount);
}

static void test_forced_capture_off_round_trips(void) {
  Rules r;
  r.forcedCapture = false;
  Game g;
  g.reset(r);
  playSome(g, 6);
  SaveGame s(ram);
  TEST_ASSERT_TRUE(s.save(g, makeHeader()));
  Game loaded;
  SaveHeader out{};
  TEST_ASSERT_TRUE(s.load(loaded, out));
  TEST_ASSERT_FALSE(loaded.rules().forcedCapture);
  TEST_ASSERT_EQUAL_UINT8(0, out.forcedCapture);
  assertSameGame(g, loaded);
}

static void test_zero_moves(void) {
  Game g;
  SaveGame s(ram);
  TEST_ASSERT_TRUE(s.save(g, makeHeader()));
  TEST_ASSERT_TRUE(s.exists());
  Game loaded;
  SaveHeader out{};
  TEST_ASSERT_TRUE(s.load(loaded, out));
  TEST_ASSERT_EQUAL_UINT16(0, loaded.ply());
  TEST_ASSERT_EQUAL_UINT16(0, out.moveCount);
  assertSameGame(g, loaded);
}

// Rewrites the stored payload with a valid CRC after `mutate` edits it.
template <typename F>
static void rewrite(F mutate) {
  uint8_t payload[Blob::kMaxPayload];
  size_t len = 0;
  TEST_ASSERT_TRUE(Blob::load(ram, SaveGame::kNamespace, SaveGame::kKey, SaveGame::kMagic,
                              SaveGame::kVersion, payload, sizeof(payload), &len));
  mutate(payload, len);
  TEST_ASSERT_TRUE(Blob::save(ram, SaveGame::kNamespace, SaveGame::kKey, SaveGame::kMagic,
                              SaveGame::kVersion, payload, len));
}

static void test_illegal_move_rejected(void) {
  Game g;
  playSome(g, 10);
  SaveGame s(ram);
  TEST_ASSERT_TRUE(s.save(g, makeHeader()));
  // First move's from-square becomes square 0: not a Black piece at the start.
  rewrite([](uint8_t* p, size_t) { p[SaveGame::kHeaderBytes] = 0; });

  Game loaded;
  playSome(loaded, 4);  // make sure a reset is observable
  SaveHeader out{};
  TEST_ASSERT_FALSE(s.load(loaded, out));
  TEST_ASSERT_EQUAL_UINT16(0, loaded.ply());
  TEST_ASSERT_TRUE(loaded.board() == Board::initial());
}

static void test_bad_header_fields_and_length_rejected(void) {
  Game g;
  playSome(g, 6);
  SaveGame s(ram);
  TEST_ASSERT_TRUE(s.save(g, makeHeader()));
  rewrite([](uint8_t* p, size_t) { p[0] = 7; });  // invalid mode
  Game loaded;
  SaveHeader out{};
  TEST_ASSERT_FALSE(s.load(loaded, out));

  TEST_ASSERT_TRUE(s.save(g, makeHeader()));
  rewrite([](uint8_t* p, size_t) { p[12] = 100; });  // claims more moves than stored
  playSome(loaded, 2);
  TEST_ASSERT_FALSE(s.load(loaded, out));
  TEST_ASSERT_EQUAL_UINT16(0, loaded.ply());
}

static void test_crc_corruption_rejected(void) {
  Game g;
  playSome(g, 6);
  SaveGame s(ram);
  TEST_ASSERT_TRUE(s.save(g, makeHeader()));
  size_t len = 0;
  uint8_t* raw = ram.raw(SaveGame::kNamespace, SaveGame::kKey, &len);
  TEST_ASSERT_NOT_NULL(raw);
  raw[len - 1] ^= 0xFF;
  TEST_ASSERT_FALSE(s.exists());
  Game loaded;
  SaveHeader out{};
  TEST_ASSERT_FALSE(s.load(loaded, out));
}

static void test_erase(void) {
  Game g;
  playSome(g, 4);
  SaveGame s(ram);
  TEST_ASSERT_TRUE(s.save(g, makeHeader()));
  TEST_ASSERT_TRUE(s.exists());
  s.erase();
  TEST_ASSERT_FALSE(s.exists());
}

// A long game of ordinary moves fits. Exceeding the byte budget is not reachable in a legal
// game within kMaxPlies, so capacity is asserted in test_capacity instead.
static void test_long_game_fits(void) {
  Game g;
  playSome(g, 200);
  SaveGame s(ram);
  TEST_ASSERT_TRUE(s.save(g, makeHeader()));
  Game loaded;
  SaveHeader out{};
  TEST_ASSERT_TRUE(s.load(loaded, out));
  assertSameGame(g, loaded);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_capacity);
  RUN_TEST(test_exists_false_when_empty);
  RUN_TEST(test_round_trip);
  RUN_TEST(test_forced_capture_off_round_trips);
  RUN_TEST(test_zero_moves);
  RUN_TEST(test_illegal_move_rejected);
  RUN_TEST(test_bad_header_fields_and_length_rejected);
  RUN_TEST(test_crc_corruption_rejected);
  RUN_TEST(test_erase);
  RUN_TEST(test_long_game_fits);
  return UNITY_END();
}
