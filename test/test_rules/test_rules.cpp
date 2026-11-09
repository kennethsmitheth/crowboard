// Rules test suite (ARCHITECTURE §5, §13): perft from the initial position, hand-verified edge
// cases written as position strings plus the expected legal move set in notation, game-end
// detection, and a fixed-seed random-playout sanity check.
//
// Depth 8 perft (845931) is off by default. Enable it with a build flag, e.g.
//   PLATFORMIO_BUILD_FLAGS=-DPERFT_DEEP pio test -e native -f test_rules
// (`-a` only passes program arguments, so it cannot define the macro).
#include <unity.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

#include "common/perft.h"
#include "game/Apply.h"
#include "game/Game.h"
#include "game/MoveGen.h"
#include "game/Notation.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

static Board parse(const char* s) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE_MESSAGE(Board::fromString(s, b), s);
  return b;
}

static std::string toText(const Move& m) {
  char buf[kMoveStringMax];
  TEST_ASSERT_TRUE(moveToString(m, buf, sizeof buf));
  return buf;
}

static std::string join(const std::vector<std::string>& v) {
  std::string s;
  for (const std::string& x : v) s += (s.empty() ? "" : " ") + x;
  return s;
}

// Asserts that the legal moves of `pos` are exactly `expected` (notation, any order). With the
// standard forced-capture rule each expected move must also parse back to the generated move.
static void expectMoves(const char* pos, std::initializer_list<const char*> expected,
                        bool forcedCapture = true) {
  const Board b = parse(pos);
  MoveList l;
  legalMoves(b, l, forcedCapture);
  TEST_ASSERT_FALSE(l.overflowed());

  std::vector<std::string> got;
  for (const Move& m : l) got.push_back(toText(m));
  std::vector<std::string> want(expected.begin(), expected.end());
  std::sort(got.begin(), got.end());
  std::sort(want.begin(), want.end());
  if (got != want) {
    std::printf("%s\n  expected: %s\n  got:      %s\n", pos, join(want).c_str(), join(got).c_str());
  }
  TEST_ASSERT_TRUE_MESSAGE(got == want, pos);

  if (!forcedCapture) return;
  for (const char* text : expected) {
    Move m{};
    TEST_ASSERT_TRUE_MESSAGE(parseMove(text, b, m), text);
    bool found = false;
    for (const Move& x : l) found = found || x == m;
    TEST_ASSERT_TRUE_MESSAGE(found, text);
  }
}

static uint32_t mask(std::initializer_list<int> notation) {
  uint32_t m = 0;
  for (int n : notation) m |= bit(fromNotation(n));
  return m;
}

// Plays `text` (notation) in g; fails the test if it does not parse or is rejected.
static void play(Game& g, const char* text) {
  Move m{};
  TEST_ASSERT_TRUE_MESSAGE(parseMove(text, g.board(), m), text);
  TEST_ASSERT_TRUE_MESSAGE(g.apply(m), text);
}

// ---- Perft -----------------------------------------------------------------------------------

static void test_perft_initial(void) {
  // Accepted English draughts counts from the initial position.
  const uint64_t expected[] = {7, 49, 302, 1469, 7361, 36768, 179740};
  for (int d = 1; d <= 7; ++d) perft_helpers::checkPerft(Board::initial(), d, expected[d - 1]);
}

#ifdef PERFT_DEEP
static void test_perft_initial_depth8(void) {
  perft_helpers::checkPerft(Board::initial(), 8, 845931);
}
#endif

// ---- Promotion ends the sequence -------------------------------------------------------------

static void test_promotion_ends_sequence_black(void) {
  // 19x10x3 crowns on 3. As a king it could go on 3x12 over 8, but the move stops.
  expectMoves("B:W7,8,15:B19", {"19x10x3"});
  MoveList l;
  legalMoves(parse("B:W7,8,15:B19"), l);
  TEST_ASSERT_TRUE(l[0].promotes);
  TEST_ASSERT_EQUAL_HEX32(mask({15, 7}), l[0].captured);
  TEST_ASSERT_TRUE(applyMove(parse("B:W7,8,15:B19"), l[0]) == parse("W:W8:BK3"));
}

static void test_promotion_ends_sequence_white(void) {
  // 23x30 crowns on 30; a king could continue 30x21 over 25.
  expectMoves("W:W23:B25,26", {"23x30"});
  // Double jump into the back row: 15x22x31 over 18 and 26. As a king it could go on 31x24 over
  // 27, but the move stops on 31.
  expectMoves("W:W15:B18,26,27", {"15x22x31"});
  MoveList l;
  legalMoves(parse("W:W15:B18,26,27"), l);
  TEST_ASSERT_TRUE(l[0].promotes);
  TEST_ASSERT_TRUE(applyMove(parse("W:W15:B18,26,27"), l[0]) == parse("B:WK31:B27"));
}

// ---- Kings -----------------------------------------------------------------------------------

static void test_king_cycle_back_to_origin(void) {
  // Black king 22 runs the diamond around 23 either way and lands on its own origin, taking all
  // four White pieces exactly once.
  const char* pos = "B:W18,19,26,27:BK22";
  expectMoves(pos, {"22x15x24x31x22", "22x31x24x15x22"});
  MoveList l;
  legalMoves(parse(pos), l);
  for (const Move& m : l) {
    TEST_ASSERT_EQUAL_HEX32(mask({18, 19, 26, 27}), m.captured);
    TEST_ASSERT_EQUAL_UINT8(m.from, m.to);
    TEST_ASSERT_TRUE(applyMove(parse(pos), m) == parse("W:W:BK22"));
  }
}

static void test_captured_piece_not_jumped_twice(void) {
  // After 22x15 the king could jump 18 again back onto its empty origin; it may not.
  expectMoves("B:W18:BK22", {"22x15"});
  expectMoves("W:WK11:B15", {"11x18"});
  // A longer chain: 22x15x24 over 18 and 19. From 24 neither captured piece is available again.
  expectMoves("B:W18,19:BK22", {"22x15x24"});
}

static void test_king_moves_both_directions(void) {
  // A king on its promotion row moves back down; on the far row it moves up.
  expectMoves("B:W:BK2", {"2-6", "2-7"});
  expectMoves("W:WK31:B", {"31-26", "31-27"});
  expectMoves("B:W:BK18", {"18-14", "18-15", "18-22", "18-23"});
  // A king on its promotion row also captures back down, in both directions.
  expectMoves("B:W6,7:BK2", {"2x9", "2x11"});
}

// ---- Forced capture --------------------------------------------------------------------------

static void test_forced_capture_any_capture(void) {
  // The single jump 24x15 is as legal as the double jump 29x22x15; simple moves are filtered.
  expectMoves("B:W18,19,25:B24,29", {"24x15", "29x22x15"});
  expectMoves("B:W18:B22,30", {"22x15"});
}

static void test_forced_capture_off_keeps_simple_moves(void) {
  expectMoves("B:W18:B22,30", {"22x15", "22-17", "30-25", "30-26"}, false);
  expectMoves("B:W18,19,25:B24,29", {"24x15", "29x22x15", "24-20"}, false);
}

// ---- Board edges -----------------------------------------------------------------------------

static void test_no_jump_off_board(void) {
  expectMoves("B:W13:B17", {"17-14"});  // 17x13 would land off the left edge
  expectMoves("W:W16:B20", {"16-19"});  // 16x20 would land off the right edge
  expectMoves("B:W1:B6", {"6-2"});      // 6x1 would land off the top edge
  expectMoves("W:W26:B31", {"26-30"});  // 26x31 would land off the bottom edge
}

// ---- Game end --------------------------------------------------------------------------------

static void test_blocked_side_loses(void) {
  Game g;
  // White man 1 is hemmed in by 5 and 6 (10 blocks the jump over 6).
  TEST_ASSERT_TRUE(g.resetFrom(parse("W:W1:B5,6,9,10")));
  TEST_ASSERT_TRUE(g.legalMoves().empty());
  TEST_ASSERT_TRUE(g.result() == Result::P1Wins);
  TEST_ASSERT_TRUE(g.reason() == Reason::NoMoves);
  // Black man 32 is hemmed in by 27 and 28 (23 blocks the jump over 27, 28 is on the edge).
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:W23,27,28:B32")));
  TEST_ASSERT_TRUE(g.legalMoves().empty());
  TEST_ASSERT_TRUE(g.result() == Result::P2Wins);
  TEST_ASSERT_TRUE(g.reason() == Reason::NoMoves);
}

static void test_eighty_ply_draw(void) {
  Game g;
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:WK2:BK17")));
  // Kings on a 6-cycle and a 10-cycle: the joint position repeats only every 60 plies, so no
  // position occurs three times within 80 plies and only the 80-ply rule can end the game.
  const char* bc[6] = {"17-22", "22-26", "26-30", "30-25", "25-21", "21-17"};
  const char* wc[10] = {"2-7",   "7-11",  "11-16", "16-20", "20-24",
                        "24-19", "19-15", "15-10", "10-6",  "6-2"};
  for (int i = 0; i < 80; ++i) {
    TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
    play(g, i % 2 == 0 ? bc[(i / 2) % 6] : wc[(i / 2) % 10]);
  }
  TEST_ASSERT_EQUAL_UINT16(80, g.halfmoveClock());
  TEST_ASSERT_TRUE(g.result() == Result::Draw);
  TEST_ASSERT_TRUE(g.reason() == Reason::FortyMoveRule);
}

static void test_threefold_repetition_draw(void) {
  Game g;
  TEST_ASSERT_TRUE(g.resetFrom(parse("B:WK4:BK29")));
  // The start position recurs at plies 4 and 8; the third occurrence draws.
  const char* cycle[4] = {"29-25", "4-8", "25-29", "8-4"};
  for (int i = 0; i < 8; ++i) {
    TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
    play(g, cycle[i % 4]);
  }
  TEST_ASSERT_TRUE(g.result() == Result::Draw);
  TEST_ASSERT_TRUE(g.reason() == Reason::Repetition);
}

// ---- Random playouts -------------------------------------------------------------------------

struct Lcg {
  uint64_t s;
  uint32_t next() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<uint32_t>(s >> 33);
  }
};

static void test_random_playouts(void) {
  static Game g;  // large (fixed-capacity history); keep it off the stack
  Lcg rng{0xC0FFEEULL};
  const Board start = Board::initial();
  const uint64_t startHash = hashBoard(start);
  int finished = 0, historyFull = 0;
  for (int game = 0; game < 200; ++game) {
    g.reset();
    while (!g.isOver() && g.ply() < kMaxPlies) {
      const MoveList& l = g.legalMoves();
      TEST_ASSERT_FALSE(l.empty());  // an ongoing game always has a move
      TEST_ASSERT_FALSE(l.overflowed());
      const Move m = l[rng.next() % l.size()];
      // Notation round trip for every move actually played.
      Move parsed{};
      TEST_ASSERT_TRUE(parseMove(toText(m).c_str(), g.board(), parsed));
      TEST_ASSERT_TRUE(parsed == m);
      TEST_ASSERT_TRUE_MESSAGE(g.apply(m), "Game::apply rejected a listed legal move");
    }
    if (g.isOver()) {
      ++finished;
    } else {
      ++historyFull;  // reached kMaxPlies while still ongoing: accepted
    }
    while (g.undo()) {
    }
    TEST_ASSERT_EQUAL_UINT16(0, g.ply());
    TEST_ASSERT_TRUE(g.board() == start);
    TEST_ASSERT_EQUAL_UINT64(startHash, hashBoard(g.board()));
    TEST_ASSERT_TRUE(g.result() == Result::Ongoing);
    TEST_ASSERT_EQUAL_UINT(7, g.legalMoves().size());
  }
  char msg[64];
  std::snprintf(msg, sizeof msg, "random playouts: %d finished, %d hit history cap", finished,
                historyFull);
  TEST_MESSAGE(msg);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_perft_initial);
#ifdef PERFT_DEEP
  RUN_TEST(test_perft_initial_depth8);
#endif
  RUN_TEST(test_promotion_ends_sequence_black);
  RUN_TEST(test_promotion_ends_sequence_white);
  RUN_TEST(test_king_cycle_back_to_origin);
  RUN_TEST(test_captured_piece_not_jumped_twice);
  RUN_TEST(test_king_moves_both_directions);
  RUN_TEST(test_forced_capture_any_capture);
  RUN_TEST(test_forced_capture_off_keeps_simple_moves);
  RUN_TEST(test_no_jump_off_board);
  RUN_TEST(test_blocked_side_loses);
  RUN_TEST(test_eighty_ply_draw);
  RUN_TEST(test_threefold_repetition_draw);
  RUN_TEST(test_random_playouts);
  return UNITY_END();
}
