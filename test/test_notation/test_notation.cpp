// Move notation: formatting, parsing and round-trips over legal moves (ARCHITECTURE §6).
// Square arguments to helpers are 1-32 notation numbers.
#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "game/Board.h"
#include "game/MoveGen.h"
#include "game/MoveList.h"
#include "game/Notation.h"

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

// Formats `m` into a fresh buffer and returns the text.
static const char* format(const Move& m, char* buf, size_t cap = kMoveStringMax) {
  TEST_ASSERT_TRUE(moveToString(m, buf, cap));
  return buf;
}

// The legal move of `b` whose text is `text`, found by formatting every legal move.
static bool findByText(const Board& b, const char* text, Move& out) {
  MoveList l;
  legalMoves(b, l);
  char buf[kMoveStringMax];
  for (const Move& m : l) {
    TEST_ASSERT_TRUE(moveToString(m, buf, sizeof buf));
    if (std::strcmp(buf, text) == 0) {
      out = m;
      return true;
    }
  }
  return false;
}

// Every legal move of `b` must format and parse back to itself.
static void assertRoundTrip(const Board& b) {
  MoveList l;
  legalMoves(b, l);
  TEST_ASSERT_TRUE(l.size() > 0);
  char buf[kMoveStringMax];
  for (const Move& m : l) {
    TEST_ASSERT_TRUE(moveToString(m, buf, sizeof buf));
    Move parsed{};
    TEST_ASSERT_TRUE_MESSAGE(parseMove(buf, b, parsed), buf);
    TEST_ASSERT_TRUE_MESSAGE(parsed == m, buf);
  }
}

// ---- Formatting ------------------------------------------------------------------------------

static void test_format_simple_move(void) {
  char buf[kMoveStringMax];
  const Move m = Move::simple(fromNotation(11), fromNotation(15), false);
  TEST_ASSERT_EQUAL_STRING("11-15", format(m, buf));
}

static void test_format_simple_move_two_digits_and_promotion(void) {
  char buf[kMoveStringMax];
  const Move m = Move::simple(fromNotation(5), fromNotation(1), true);
  TEST_ASSERT_EQUAL_STRING("5-1", format(m, buf));
  const Move n = Move::simple(fromNotation(29), fromNotation(25), false);
  TEST_ASSERT_EQUAL_STRING("29-25", format(n, buf));
}

static void test_format_single_jump(void) {
  MoveList l;
  captureMoves(parse("B:W18:B22"), l);  // 22 x 18 -> 15
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  char buf[kMoveStringMax];
  TEST_ASSERT_EQUAL_STRING("22x15", format(l[0], buf));
}

static void test_format_double_jump(void) {
  MoveList l;
  captureMoves(parse("B:W18,25:B29"), l);  // 29 -> 22 -> 15
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  char buf[kMoveStringMax];
  TEST_ASSERT_EQUAL_STRING("29x22x15", format(l[0], buf));
}

static void test_format_triple_jump(void) {
  MoveList l;
  captureMoves(parse("B:W11,18,25:B29"), l);  // 29 -> 22 -> 15 -> 8
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  char buf[kMoveStringMax];
  TEST_ASSERT_EQUAL_STRING("29x22x15x8", format(l[0], buf));
}

static void test_format_overflow_writes_empty(void) {
  MoveList l;
  captureMoves(parse("B:W11,18,25:B29"), l);
  TEST_ASSERT_EQUAL_UINT(1, l.size());
  // "29x22x15x8" is 10 characters: it needs 11 bytes with the NUL.
  char buf[16];
  std::memset(buf, 'Z', sizeof buf);
  TEST_ASSERT_FALSE(moveToString(l[0], buf, 10));
  TEST_ASSERT_EQUAL_STRING("", buf);
  TEST_ASSERT_FALSE(moveToString(l[0], buf, 1));
  TEST_ASSERT_EQUAL_STRING("", buf);
  TEST_ASSERT_TRUE(moveToString(l[0], buf, 11));
  TEST_ASSERT_EQUAL_STRING("29x22x15x8", buf);
}

static void test_format_zero_capacity_and_null(void) {
  const Move m = Move::simple(0, 4, false);
  char buf[4] = {'Z', 'Z', 'Z', 'Z'};
  TEST_ASSERT_FALSE(moveToString(m, buf, 0));
  TEST_ASSERT_FALSE(moveToString(m, nullptr, sizeof buf));
  TEST_ASSERT_TRUE(moveToString(m, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("1-5", buf);
}

static void test_format_longest_move_fits_max_buffer(void) {
  // Ten landings on square 32: "32" then ten "x32" = 32 characters.
  Move m{};
  m.from = fromNotation(32);
  m.to = fromNotation(32);
  m.captured = 1;
  m.pathLen = kMaxPathLen;
  for (int i = 0; i < kMaxPathLen; ++i) m.path[i] = fromNotation(32);
  char buf[kMoveStringMax];
  TEST_ASSERT_TRUE(moveToString(m, buf, sizeof buf));
  TEST_ASSERT_EQUAL_UINT(32, std::strlen(buf));
  TEST_ASSERT_EQUAL_UINT(kMoveStringMax, sizeof buf);
}

// ---- Parsing ---------------------------------------------------------------------------------

static void test_parse_simple_move_from_initial(void) {
  Move m{};
  TEST_ASSERT_TRUE(parseMove("22-18", Board::initial(), m));
  TEST_ASSERT_EQUAL_UINT8(fromNotation(22), m.from);
  TEST_ASSERT_EQUAL_UINT8(fromNotation(18), m.to);
  TEST_ASSERT_FALSE(m.isCapture());
}

static void test_parse_capture_resolves_path(void) {
  const Board b = parse("B:W18,25:B29");
  Move m{};
  TEST_ASSERT_TRUE(parseMove("29x22x15", b, m));
  TEST_ASSERT_EQUAL_UINT(2, m.pathLen);
  TEST_ASSERT_EQUAL_HEX32(mask({25, 18}), m.captured);
  // A prefix of the sequence is not a legal move (the jump must be completed).
  TEST_ASSERT_FALSE(parseMove("29x22", b, m));
}

static void test_parse_two_paths_same_destination(void) {
  // Black man 26 can reach 10 via 17 (taking 22, 14) or via 19 (taking 23, 15).
  const Board b = parse("B:W14,15,22,23:B26");
  Move via17{};
  Move via19{};
  TEST_ASSERT_TRUE(parseMove("26x17x10", b, via17));
  TEST_ASSERT_TRUE(parseMove("26x19x10", b, via19));
  TEST_ASSERT_EQUAL_HEX32(mask({22, 14}), via17.captured);
  TEST_ASSERT_EQUAL_HEX32(mask({23, 15}), via19.captured);
  TEST_ASSERT_TRUE(via17 != via19);
  TEST_ASSERT_TRUE(via17.to == via19.to);
}

static void test_parse_leaves_out_untouched_on_failure(void) {
  Move sentinel = Move::simple(fromNotation(3), fromNotation(7), true);
  Move out = sentinel;
  TEST_ASSERT_FALSE(parseMove("garbage", Board::initial(), out));
  TEST_ASSERT_TRUE(out == sentinel);
  TEST_ASSERT_FALSE(parseMove("11-15", Board::initial(), out));
  TEST_ASSERT_TRUE(out == sentinel);
}

static void test_parse_rejects_null_and_empty(void) {
  Move m{};
  const Board b = Board::initial();
  TEST_ASSERT_FALSE(parseMove(nullptr, b, m));
  TEST_ASSERT_FALSE(parseMove("", b, m));
  TEST_ASSERT_FALSE(parseMove("-", b, m));
  TEST_ASSERT_FALSE(parseMove("x", b, m));
  TEST_ASSERT_FALSE(parseMove("22", b, m));
}

static void test_parse_rejects_garbage(void) {
  const Board b = Board::initial();
  Move m{};
  TEST_ASSERT_FALSE(parseMove("22-", b, m));
  TEST_ASSERT_FALSE(parseMove("-18", b, m));
  TEST_ASSERT_FALSE(parseMove("22--18", b, m));
  TEST_ASSERT_FALSE(parseMove("22+18", b, m));
  TEST_ASSERT_FALSE(parseMove("22_18", b, m));
  TEST_ASSERT_FALSE(parseMove("a2-18", b, m));
  TEST_ASSERT_FALSE(parseMove("22-18 ", b, m));
  TEST_ASSERT_FALSE(parseMove(" 22-18", b, m));
  TEST_ASSERT_FALSE(parseMove("22 - 18", b, m));
  TEST_ASSERT_FALSE(parseMove("22-18\n", b, m));
  TEST_ASSERT_FALSE(parseMove("22X18", b, m));
  TEST_ASSERT_FALSE(parseMove("22-1x8", b, m));
  TEST_ASSERT_FALSE(parseMove("22-18-", b, m));
}

static void test_parse_rejects_leading_zero_and_long_numbers(void) {
  const Board b = Board::initial();
  Move m{};
  TEST_ASSERT_FALSE(parseMove("022-18", b, m));
  TEST_ASSERT_FALSE(parseMove("22-018", b, m));
  TEST_ASSERT_FALSE(parseMove("22-180", b, m));
  TEST_ASSERT_FALSE(parseMove("222-18", b, m));
}

static void test_parse_rejects_out_of_range_squares(void) {
  const Board b = Board::initial();
  Move m{};
  TEST_ASSERT_FALSE(parseMove("0-4", b, m));
  TEST_ASSERT_FALSE(parseMove("00-4", b, m));
  TEST_ASSERT_FALSE(parseMove("33-1", b, m));
  TEST_ASSERT_FALSE(parseMove("1-33", b, m));
  TEST_ASSERT_FALSE(parseMove("21-99", b, m));
  TEST_ASSERT_FALSE(parseMove("22-18x40", b, m));
  TEST_ASSERT_FALSE(parseMove("22-18-0", b, m));
  const Board c = parse("B:W18,25:B29");
  TEST_ASSERT_FALSE(parseMove("29x22x15x40", c, m));
}

static void test_parse_rejects_wrong_separator(void) {
  Move m{};
  // Simple move written as a capture.
  TEST_ASSERT_FALSE(parseMove("22x18", Board::initial(), m));
  // Capture written as a simple move.
  const Board b = parse("B:W18:B22");
  TEST_ASSERT_FALSE(parseMove("22-15", b, m));
  TEST_ASSERT_TRUE(parseMove("22x15", b, m));
  // Mixed separators in one string.
  const Board c = parse("B:W18,25:B29");
  TEST_ASSERT_FALSE(parseMove("29x22-15", c, m));
  TEST_ASSERT_FALSE(parseMove("29-22x15", c, m));
  // A dash may join only two numbers.
  TEST_ASSERT_FALSE(parseMove("29-22-15", c, m));
}

static void test_parse_rejects_illegal_moves(void) {
  Move m{};
  const Board b = Board::initial();
  TEST_ASSERT_FALSE(parseMove("11-15", b, m));                  // White's man
  TEST_ASSERT_FALSE(parseMove("22-19", b, m));                  // not diagonal-adjacent
  TEST_ASSERT_FALSE(parseMove("25-21", b, m));                  // target occupied
  TEST_ASSERT_FALSE(parseMove("21-14", b, m));                  // not adjacent
  TEST_ASSERT_FALSE(parseMove("1-5", b, m));                    // not Black's move
  TEST_ASSERT_FALSE(parseMove("21-17", parse("W:W1:B21"), m));  // wrong side to move
}

static void test_parse_enforces_forced_capture(void) {
  // 22 can jump 18, so the quiet moves 22-17 and 30-25 are not legal.
  const Board b = parse("B:W18:B22,30");
  Move m{};
  TEST_ASSERT_FALSE(parseMove("22-17", b, m));
  TEST_ASSERT_FALSE(parseMove("30-25", b, m));
  TEST_ASSERT_TRUE(parseMove("22x15", b, m));
}

static void test_parse_rejects_longer_or_shorter_capture(void) {
  // The jump ends on 15 and cannot continue, so 22x15x8 is not a legal move.
  const Board b = parse("B:W18:B22");
  Move m{};
  TEST_ASSERT_FALSE(parseMove("22x15x8", b, m));
  TEST_ASSERT_FALSE(parseMove("22x8", b, m));
}

// ---- Round-trips -----------------------------------------------------------------------------

static void test_round_trip_initial_position(void) {
  assertRoundTrip(Board::initial());
  MoveList l;
  legalMoves(Board::initial(), l);
  TEST_ASSERT_EQUAL_UINT(7, l.size());
  Move m{};
  TEST_ASSERT_TRUE(parseMove("21-17", Board::initial(), m));
}

static void test_round_trip_single_and_multi_jumps(void) {
  assertRoundTrip(parse("B:W18:B22"));           // single jump
  assertRoundTrip(parse("B:W18,25:B29"));        // double jump
  assertRoundTrip(parse("B:W11,18,25:B29"));     // triple jump
  assertRoundTrip(parse("B:W1,11,K18,25:B29"));  // white king in the chain
}

static void test_round_trip_two_paths_same_destination(void) {
  assertRoundTrip(parse("B:W14,15,22,23:B26"));
  Move a{};
  Move b{};
  TEST_ASSERT_TRUE(findByText(parse("B:W14,15,22,23:B26"), "26x17x10", a));
  TEST_ASSERT_TRUE(findByText(parse("B:W14,15,22,23:B26"), "26x19x10", b));
  TEST_ASSERT_TRUE(a != b);
}

static void test_round_trip_mixed_position(void) {
  // Mixed men and kings with a forced capture set.
  assertRoundTrip(parse("B:W18,19,26,27:BK22"));
  assertRoundTrip(parse("W:W8,9,K12:B14,K20,K23"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_format_simple_move);
  RUN_TEST(test_format_simple_move_two_digits_and_promotion);
  RUN_TEST(test_format_single_jump);
  RUN_TEST(test_format_double_jump);
  RUN_TEST(test_format_triple_jump);
  RUN_TEST(test_format_overflow_writes_empty);
  RUN_TEST(test_format_zero_capacity_and_null);
  RUN_TEST(test_format_longest_move_fits_max_buffer);
  RUN_TEST(test_parse_simple_move_from_initial);
  RUN_TEST(test_parse_capture_resolves_path);
  RUN_TEST(test_parse_two_paths_same_destination);
  RUN_TEST(test_parse_leaves_out_untouched_on_failure);
  RUN_TEST(test_parse_rejects_null_and_empty);
  RUN_TEST(test_parse_rejects_garbage);
  RUN_TEST(test_parse_rejects_leading_zero_and_long_numbers);
  RUN_TEST(test_parse_rejects_out_of_range_squares);
  RUN_TEST(test_parse_rejects_wrong_separator);
  RUN_TEST(test_parse_rejects_illegal_moves);
  RUN_TEST(test_parse_enforces_forced_capture);
  RUN_TEST(test_parse_rejects_longer_or_shorter_capture);
  RUN_TEST(test_round_trip_initial_position);
  RUN_TEST(test_round_trip_single_and_multi_jumps);
  RUN_TEST(test_round_trip_two_paths_same_destination);
  RUN_TEST(test_round_trip_mixed_position);
  return UNITY_END();
}
