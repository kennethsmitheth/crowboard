// Capture generation, forced capture, applying moves, and perft (ARCHITECTURE §5, §13).
// Square arguments to helpers are 1-32 notation numbers.
#include <unity.h>

#include <initializer_list>

#include "common/perft.h"
#include "game/Apply.h"
#include "game/MoveGen.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

static Board parse(const char* s) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE_MESSAGE(Board::fromString(s, b), s);
  return b;
}

static uint32_t mask(std::initializer_list<int> notation) {
  uint32_t m = 0;
  for (int n : notation) m |= bit(fromNotation(n));
  return m;
}

// True if `m` goes from `fromN` through the landing squares `pathN` (notation numbers).
static bool pathIs(const Move& m, int fromN, std::initializer_list<int> pathN) {
  if (m.from != fromNotation(fromN) || m.pathLen != pathN.size()) return false;
  int i = 0;
  for (int n : pathN) {
    if (m.path[i++] != fromNotation(n)) return false;
  }
  return m.to == m.path[m.pathLen - 1];
}

static const Move* findPath(const MoveList& l, int fromN, std::initializer_list<int> pathN) {
  for (const Move& m : l) {
    if (pathIs(m, fromN, pathN)) return &m;
  }
  return nullptr;
}

static int countCaptures(const MoveList& l) {
  int n = 0;
  for (const Move& m : l) n += m.isCapture() ? 1 : 0;
  return n;
}

// ---- Captures --------------------------------------------------------------------------------

static void test_single_jump_black_man(void) {
  MoveList l;
  captureMoves(parse("B:W18:B22"), l);  // 22 x 18 -> 15
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  const Move& m = l[0];
  TEST_ASSERT_TRUE(pathIs(m, 22, {15}));
  TEST_ASSERT_EQUAL_HEX32(mask({18}), m.captured);
  TEST_ASSERT_EQUAL_INT(1, m.captureCount());
  TEST_ASSERT_FALSE(m.promotes);
}

static void test_single_jump_white_man(void) {
  MoveList l;
  captureMoves(parse("W:W11:B15"), l);  // 11 x 15 -> 18
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(pathIs(l[0], 11, {18}));
  TEST_ASSERT_EQUAL_HEX32(mask({15}), l[0].captured);
}

static void test_men_do_not_capture_backward(void) {
  MoveList l;
  captureMoves(parse("B:W22:B18"), l);  // 22 is behind the Black man on 18
  TEST_ASSERT_EQUAL_UINT(0, l.size());
  captureMoves(parse("W:W18:B15"), l);  // 15 is behind the White man on 18
  TEST_ASSERT_EQUAL_UINT(0, l.size());
}

static void test_king_captures_backward(void) {
  MoveList l;
  captureMoves(parse("B:W22:BK18"), l);  // 18 x 22 -> 25
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(pathIs(l[0], 18, {25}));
  TEST_ASSERT_FALSE(l[0].promotes);
}

static void test_jump_blocked_by_occupied_landing_or_edge(void) {
  MoveList l;
  captureMoves(parse("B:W15,18:B22"), l);  // landing 15 is occupied
  TEST_ASSERT_EQUAL_UINT(0, l.size());
  captureMoves(parse("B:W13:B17"), l);  // 17 x 13 would land off the board
  TEST_ASSERT_EQUAL_UINT(0, l.size());
  captureMoves(parse("B:W:B22,18"), l);  // own pieces are not captured
  TEST_ASSERT_EQUAL_UINT(0, l.size());
}

static void test_double_jump(void) {
  MoveList l;
  captureMoves(parse("B:W18,25:B29"), l);  // 29 x 25 -> 22 x 18 -> 15
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(pathIs(l[0], 29, {22, 15}));
  TEST_ASSERT_EQUAL_HEX32(mask({25, 18}), l[0].captured);
  TEST_ASSERT_EQUAL_INT(2, l[0].captureCount());
}

static void test_triple_jump(void) {
  MoveList l;
  captureMoves(parse("B:W11,18,25:B29"), l);  // 29 -> 22 -> 15 -> 8
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(pathIs(l[0], 29, {22, 15, 8}));
  TEST_ASSERT_EQUAL_HEX32(mask({25, 18, 11}), l[0].captured);
  TEST_ASSERT_EQUAL_INT(3, l[0].captureCount());
  TEST_ASSERT_FALSE(l[0].promotes);  // 8 is on row 1
}

static void test_must_continue_jumping(void) {
  // No partial sequence is emitted: only the full 29 -> 22 -> 15 exists, not 29 -> 22.
  MoveList l;
  captureMoves(parse("B:W18,25:B29"), l);
  TEST_ASSERT_NULL(findPath(l, 29, {22}));
}

static void test_branching_sequences(void) {
  // Black man 26 can take 22 or 23 first, then 14 or 15: two sequences ending on 10.
  MoveList l;
  captureMoves(parse("B:W14,15,22,23:B26"), l);
  TEST_ASSERT_EQUAL_UINT(2, l.size());
  const Move* a = findPath(l, 26, {17, 10});
  const Move* b = findPath(l, 26, {19, 10});
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_NOT_NULL(b);
  TEST_ASSERT_EQUAL_HEX32(mask({22, 14}), a->captured);
  TEST_ASSERT_EQUAL_HEX32(mask({23, 15}), b->captured);
  TEST_ASSERT_EQUAL_UINT8(a->from, b->from);
  TEST_ASSERT_EQUAL_UINT8(a->to, b->to);
  TEST_ASSERT_TRUE(*a != *b);  // same from/to, different paths: distinct moves
}

static void test_cannot_rejump_captured_piece(void) {
  // King 22 x 18 -> 15. From 15 a jump back over 18 onto 22 (the now empty origin) would be
  // geometrically possible, but 18 was already captured.
  MoveList l;
  captureMoves(parse("B:W18:BK22"), l);
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(pathIs(l[0], 22, {15}));
  TEST_ASSERT_EQUAL_INT(1, l[0].captureCount());
}

static void test_king_ring_terminates(void) {
  // Black king on 22 surrounded by White on 18, 19, 26, 27 (a diamond around 23). It can run the
  // ring either way: 22 -> 15 -> 24 -> 31 -> 22 or 22 -> 31 -> 24 -> 15 -> 22, ending on its own
  // origin square. Each sequence captures all four pieces exactly once, then stops.
  MoveList l;
  captureMoves(parse("B:W18,19,26,27:BK22"), l);
  TEST_ASSERT_FALSE(l.overflowed());
  TEST_ASSERT_EQUAL_UINT(2, l.size());
  const Move* cw = findPath(l, 22, {15, 24, 31, 22});
  const Move* ccw = findPath(l, 22, {31, 24, 15, 22});
  TEST_ASSERT_NOT_NULL(cw);
  TEST_ASSERT_NOT_NULL(ccw);
  for (const Move& m : l) {
    TEST_ASSERT_EQUAL_HEX32(mask({18, 19, 26, 27}), m.captured);
    TEST_ASSERT_EQUAL_INT(4, m.captureCount());
    TEST_ASSERT_EQUAL_UINT8(m.from, m.to);
    TEST_ASSERT_FALSE(m.promotes);
  }
  TEST_ASSERT_TRUE(*cw != *ccw);

  // Applying it leaves the king alone on 22 with White to move.
  const Board after = applyMove(parse("B:W18,19,26,27:BK22"), *cw);
  TEST_ASSERT_TRUE(after == parse("W:W:BK22"));
}

static void test_promotion_ends_sequence(void) {
  // Black man 19 x 15 -> 10 x 7 -> 3 reaches row 0. As a king it could go on 3 x 8 -> 12, but
  // promotion ends the turn.
  MoveList l;
  captureMoves(parse("B:W7,8,15:B19"), l);
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  const Move& m = l[0];
  TEST_ASSERT_TRUE(pathIs(m, 19, {10, 3}));
  TEST_ASSERT_EQUAL_UINT8(2, m.pathLen);
  TEST_ASSERT_EQUAL_HEX32(mask({15, 7}), m.captured);
  TEST_ASSERT_TRUE(m.promotes);
  for (const Move& x : l) TEST_ASSERT_TRUE(x.pathLen <= 2);

  const Board after = applyMove(parse("B:W7,8,15:B19"), m);
  TEST_ASSERT_TRUE(after == parse("W:W8:BK3"));
}

static void test_promotion_single_jump_white(void) {
  // White man 23 x 26 -> 30 promotes; as a king it could continue 30 x 25 -> 21.
  MoveList l;
  captureMoves(parse("W:W23:B25,26"), l);
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(pathIs(l[0], 23, {30}));
  TEST_ASSERT_EQUAL_UINT8(1, l[0].pathLen);
  TEST_ASSERT_TRUE(l[0].promotes);
}

static void test_king_reaching_back_row_continues(void) {
  // A king is not "promoted" again: Black king 10 x 7 -> 3 x 8 -> 12 keeps going.
  MoveList l;
  captureMoves(parse("B:W7,8:BK10"), l);
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(pathIs(l[0], 10, {3, 12}));
  TEST_ASSERT_FALSE(l[0].promotes);
}

// ---- legalMoves / forced capture -------------------------------------------------------------

static void test_forced_capture_filters_simple_moves(void) {
  const Board b = parse("B:W18:B22,30");  // 22 x 18; simple moves 22-17, 30-25, 30-26
  MoveList l;
  legalMoves(b, l);
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  TEST_ASSERT_TRUE(pathIs(l[0], 22, {15}));
  TEST_ASSERT_TRUE(l[0].isCapture());
}

static void test_forced_capture_off_keeps_both(void) {
  const Board b = parse("B:W18:B22,30");
  MoveList l;
  legalMoves(b, l, false);
  TEST_ASSERT_EQUAL_UINT(4, l.size());
  TEST_ASSERT_EQUAL_INT(1, countCaptures(l));
  TEST_ASSERT_NOT_NULL(findPath(l, 22, {15}));
  TEST_ASSERT_NOT_NULL(findPath(l, 22, {17}));
  TEST_ASSERT_NOT_NULL(findPath(l, 30, {25}));
  TEST_ASSERT_NOT_NULL(findPath(l, 30, {26}));
}

static void test_any_capture_not_longest(void) {
  // The single jump 24 x 19 -> 15 is as legal as the double jump 29 x 25 x 18.
  MoveList l;
  legalMoves(parse("B:W18,19,25:B24,29"), l);
  TEST_ASSERT_EQUAL_UINT(2, l.size());
  TEST_ASSERT_NOT_NULL(findPath(l, 29, {22, 15}));
  TEST_ASSERT_NOT_NULL(findPath(l, 24, {15}));
  TEST_ASSERT_EQUAL_INT(2, countCaptures(l));
}

static void test_initial_position_no_captures(void) {
  MoveList l;
  captureMoves(Board::initial(), l);
  TEST_ASSERT_EQUAL_UINT(0, l.size());
  legalMoves(Board::initial(), l);
  TEST_ASSERT_EQUAL_UINT(7, l.size());
  TEST_ASSERT_EQUAL_INT(0, countCaptures(l));
}

static void test_legal_moves_appends(void) {
  MoveList l;
  l.push(Move::simple(0, 4, false));
  legalMoves(parse("B:W18:B22,30"), l);  // the pre-existing move does not count as a capture
  TEST_ASSERT_EQUAL_UINT(2, l.size());
  TEST_ASSERT_TRUE(pathIs(l[1], 22, {15}));
}

// ---- applyMove -------------------------------------------------------------------------------

static void test_apply_simple_move(void) {
  const Board b = Board::initial();
  const Board after = applyMove(b, Move::simple(fromNotation(22), fromNotation(18), false));
  TEST_ASSERT_TRUE(after.side == Side::White);
  TEST_ASSERT_TRUE(after.pieceAt(fromNotation(22)) == Piece::None);
  TEST_ASSERT_TRUE(after.pieceAt(fromNotation(18)) == Piece::BlackMan);
  TEST_ASSERT_TRUE(b == Board::initial());  // the source is untouched
}

static void test_apply_capture_removes_pieces(void) {
  const Board b = parse("B:W1,11,K18,25:B29");
  MoveList l;
  legalMoves(b, l);
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  const Board after = applyMove(b, l[0]);
  TEST_ASSERT_TRUE(after == parse("W:W1:B8"));
}

static void test_apply_promotion_simple_and_king_move(void) {
  Board b = parse("B:W:B6");
  apply(b, Move::simple(fromNotation(6), fromNotation(1), true));
  TEST_ASSERT_TRUE(b == parse("W:W:BK1"));
  b = parse("W:WK18:B");
  apply(b, Move::simple(fromNotation(18), fromNotation(14), false));
  TEST_ASSERT_TRUE(b == parse("B:WK14:B"));
}

// ---- Perft -----------------------------------------------------------------------------------

// Quick smoke check; test_rules carries the full perft to depth 7 (8 with PERFT_DEEP).
static void test_perft_initial(void) {
  const uint64_t expected[] = {7, 49, 302, 1469, 7361};
  for (int d = 1; d <= 5; ++d) perft_helpers::checkPerft(Board::initial(), d, expected[d - 1]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_single_jump_black_man);
  RUN_TEST(test_single_jump_white_man);
  RUN_TEST(test_men_do_not_capture_backward);
  RUN_TEST(test_king_captures_backward);
  RUN_TEST(test_jump_blocked_by_occupied_landing_or_edge);
  RUN_TEST(test_double_jump);
  RUN_TEST(test_triple_jump);
  RUN_TEST(test_must_continue_jumping);
  RUN_TEST(test_branching_sequences);
  RUN_TEST(test_cannot_rejump_captured_piece);
  RUN_TEST(test_king_ring_terminates);
  RUN_TEST(test_promotion_ends_sequence);
  RUN_TEST(test_promotion_single_jump_white);
  RUN_TEST(test_king_reaching_back_row_continues);
  RUN_TEST(test_forced_capture_filters_simple_moves);
  RUN_TEST(test_forced_capture_off_keeps_both);
  RUN_TEST(test_any_capture_not_longest);
  RUN_TEST(test_initial_position_no_captures);
  RUN_TEST(test_legal_moves_appends);
  RUN_TEST(test_apply_simple_move);
  RUN_TEST(test_apply_capture_removes_pieces);
  RUN_TEST(test_apply_promotion_simple_and_king_move);
  RUN_TEST(test_perft_initial);
  return UNITY_END();
}
