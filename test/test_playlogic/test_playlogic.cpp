// Play scene selection / multi-jump continuation logic (app/PlayLogic.h, ARCHITECTURE §8).
#include <unity.h>

#include <initializer_list>

#include "app/PlayLogic.h"
#include "game/Game.h"

using namespace checkers;
using checkers::app::PlaySelection;
using checkers::app::PlayTapResult;

void setUp(void) {}
void tearDown(void) {}

// Bitmask of 0-based squares from 1-32 notation numbers.
static uint32_t mask(std::initializer_list<int> notation) {
  uint32_t m = 0;
  for (int n : notation) m |= uint32_t{1} << (n - 1);
  return m;
}

static Game game;  // Game is large (history arrays); keep it off the stack

static Game& fromPosition(const char* s) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE_MESSAGE(Board::fromString(s, b), s);
  TEST_ASSERT_TRUE(game.resetFrom(b));
  return game;
}

static Game& initial() {
  game.reset();
  return game;
}

// Feeds taps (notation numbers) one after the other and returns the last result.
static PlayTapResult taps(const MoveList& moves, std::initializer_list<int> squares,
                          PlaySelection* selOut = nullptr) {
  PlaySelection sel;
  PlayTapResult r;
  for (int n : squares) {
    r = app::playTap(moves, sel, n);
    sel = r.sel;
  }
  if (selOut) *selOut = sel;
  return r;
}

static void test_select_piece_with_moves(void) {
  Game& g = initial();
  PlayTapResult r = taps(g.legalMoves(), {22});
  TEST_ASSERT_FALSE(r.resolved);
  TEST_ASSERT_EQUAL_INT(fromNotation(22), r.sel.from);
  TEST_ASSERT_EQUAL_UINT8(0, r.sel.pathLen);
  TEST_ASSERT_EQUAL_HEX32(mask({17, 18}), r.targets);
  TEST_ASSERT_EQUAL_HEX32(0, r.forced);
}

static void test_cannot_select_blocked_or_enemy_or_empty(void) {
  Game& g = initial();
  TEST_ASSERT_FALSE(taps(g.legalMoves(), {29}).sel.active());  // own piece, no move
  TEST_ASSERT_FALSE(taps(g.legalMoves(), {9}).sel.active());   // enemy piece
  TEST_ASSERT_FALSE(taps(g.legalMoves(), {15}).sel.active());  // empty square
  TEST_ASSERT_FALSE(taps(g.legalMoves(), {0}).sel.active());   // light square / off board
}

static void test_deselect(void) {
  Game& g = initial();
  const MoveList& ml = g.legalMoves();
  PlayTapResult r = taps(ml, {22, 22});  // re-tap the selected piece
  TEST_ASSERT_FALSE(r.sel.active());
  TEST_ASSERT_EQUAL_HEX32(0, r.targets);
  r = taps(ml, {22, 15});  // an empty, non-target square
  TEST_ASSERT_FALSE(r.sel.active());
  TEST_ASSERT_FALSE(r.resolved);
  r = taps(ml, {22, 0});  // a light square
  TEST_ASSERT_FALSE(r.sel.active());
}

static void test_switch_selection(void) {
  Game& g = initial();
  PlayTapResult r = taps(g.legalMoves(), {22, 24});
  TEST_ASSERT_EQUAL_INT(fromNotation(24), r.sel.from);
  TEST_ASSERT_EQUAL_HEX32(mask({19, 20}), r.targets);
}

static void test_simple_move_resolves(void) {
  Game& g = initial();
  PlayTapResult r = taps(g.legalMoves(), {22, 18});
  TEST_ASSERT_TRUE(r.resolved);
  TEST_ASSERT_EQUAL_UINT8(fromNotation(22), r.move.from);
  TEST_ASSERT_EQUAL_UINT8(fromNotation(18), r.move.to);
  TEST_ASSERT_FALSE(r.sel.active());
  TEST_ASSERT_TRUE(g.apply(r.move));
}

static void test_forced_capture_restricts_selection(void) {
  // Black 22 must jump 18; 24 and 30 have only simple moves, so they are not selectable.
  Game& g = fromPosition("B:W18:B22,24,30");
  const MoveList& ml = g.legalMoves();
  TEST_ASSERT_EQUAL_HEX32(mask({22}), app::forcedPieces(ml));
  PlayTapResult r = taps(ml, {24});
  TEST_ASSERT_FALSE(r.sel.active());
  TEST_ASSERT_EQUAL_HEX32(mask({22}), r.forced);
  r = taps(ml, {30});
  TEST_ASSERT_FALSE(r.sel.active());
  r = taps(ml, {22});
  TEST_ASSERT_EQUAL_INT(fromNotation(22), r.sel.from);
  TEST_ASSERT_EQUAL_HEX32(mask({15}), r.targets);
  r = taps(ml, {22, 15});
  TEST_ASSERT_TRUE(r.resolved);
  TEST_ASSERT_EQUAL_HEX32(mask({18}), r.move.captured);
}

static void test_no_forced_mask_without_captures(void) {
  Game& g = initial();
  TEST_ASSERT_EQUAL_HEX32(0, app::forcedPieces(g.legalMoves()));
}

static void test_two_jump_needs_two_taps(void) {
  // 26 x 17 x 10 over White 22 and 14.
  Game& g = fromPosition("B:W14,22:B26");
  const MoveList& ml = g.legalMoves();
  PlaySelection sel;
  PlayTapResult r = taps(ml, {26, 17}, &sel);
  TEST_ASSERT_FALSE(r.resolved);
  TEST_ASSERT_EQUAL_INT(fromNotation(26), sel.from);
  TEST_ASSERT_EQUAL_UINT8(1, sel.pathLen);
  TEST_ASSERT_EQUAL_UINT8(fromNotation(17), sel.path[0]);
  TEST_ASSERT_EQUAL_HEX32(mask({10}), r.targets);
  r = app::playTap(ml, sel, 10);
  TEST_ASSERT_TRUE(r.resolved);
  TEST_ASSERT_EQUAL_UINT8(fromNotation(10), r.move.to);
  TEST_ASSERT_EQUAL_INT(2, r.move.captureCount());
  TEST_ASSERT_TRUE(g.apply(r.move));
}

static void test_partial_path_cleared_by_other_tap(void) {
  Game& g = fromPosition("B:W14,22:B26");
  const MoveList& ml = g.legalMoves();
  PlayTapResult r = taps(ml, {26, 17, 5});  // 5 is not a landing square
  TEST_ASSERT_FALSE(r.resolved);
  TEST_ASSERT_FALSE(r.sel.active());
  TEST_ASSERT_EQUAL_UINT8(0, r.sel.pathLen);
  TEST_ASSERT_EQUAL_HEX32(0, r.targets);
  r = taps(ml, {26, 17, 26});  // the origin piece again
  TEST_ASSERT_FALSE(r.sel.active());
}

static void test_shared_first_landing_disambiguated(void) {
  // 30 x 23 then either x 14 (over 18) or x 16 (over 19).
  Game& g = fromPosition("B:W18,19,26:B30");
  const MoveList& ml = g.legalMoves();
  TEST_ASSERT_EQUAL_UINT(2, ml.size());
  PlaySelection sel;
  PlayTapResult r = taps(ml, {30}, &sel);
  TEST_ASSERT_EQUAL_HEX32(mask({23}), r.targets);  // one target for both sequences
  r = taps(ml, {30, 23}, &sel);
  TEST_ASSERT_FALSE(r.resolved);
  TEST_ASSERT_EQUAL_HEX32(mask({14, 16}), r.targets);
  PlayTapResult a = app::playTap(ml, sel, 16);
  TEST_ASSERT_TRUE(a.resolved);
  TEST_ASSERT_EQUAL_UINT8(fromNotation(16), a.move.to);
  TEST_ASSERT_EQUAL_HEX32(mask({26, 19}), a.move.captured);
  PlayTapResult b = app::playTap(ml, sel, 14);
  TEST_ASSERT_TRUE(b.resolved);
  TEST_ASSERT_EQUAL_UINT8(fromNotation(14), b.move.to);
  TEST_ASSERT_EQUAL_HEX32(mask({26, 18}), b.move.captured);
}

static void test_promotion_move_resolves(void) {
  Game& g = fromPosition("B:W25:B5");
  PlayTapResult r = taps(g.legalMoves(), {5, 1});
  TEST_ASSERT_TRUE(r.resolved);
  TEST_ASSERT_TRUE(r.move.promotes);
  TEST_ASSERT_TRUE(g.apply(r.move));
  TEST_ASSERT_TRUE(g.board().pieceAt(fromNotation(1)) == Piece::BlackKing);
}

static void test_capture_promotion_ends_in_one_tap(void) {
  // 10 x 1 over 6 promotes; the turn ends on the promotion row.
  Game& g = fromPosition("B:W6,25:B10");
  PlayTapResult r = taps(g.legalMoves(), {10, 1});
  TEST_ASSERT_TRUE(r.resolved);
  TEST_ASSERT_TRUE(r.move.promotes);
  TEST_ASSERT_EQUAL_UINT8(1, r.move.pathLen);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_select_piece_with_moves);
  RUN_TEST(test_cannot_select_blocked_or_enemy_or_empty);
  RUN_TEST(test_deselect);
  RUN_TEST(test_switch_selection);
  RUN_TEST(test_simple_move_resolves);
  RUN_TEST(test_forced_capture_restricts_selection);
  RUN_TEST(test_no_forced_mask_without_captures);
  RUN_TEST(test_two_jump_needs_two_taps);
  RUN_TEST(test_partial_path_cleared_by_other_tap);
  RUN_TEST(test_shared_first_landing_disambiguated);
  RUN_TEST(test_promotion_move_resolves);
  RUN_TEST(test_capture_promotion_ends_in_one_tap);
  return UNITY_END();
}
