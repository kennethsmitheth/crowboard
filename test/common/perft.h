#pragma once
// Shared perft helpers for the native test suites. Header-only: PlatformIO compiles only the
// sources inside each test_* folder, but test/ itself is on the include path, so suites include
// this as "common/perft.h".
//
// perft(b, depth) counts the complete legal move sequences of length `depth` from b with forced
// capture; a multi-jump counts as one move. checkPerft() asserts a total and, on a mismatch,
// prints a divide (perft per root move, in notation) first to localize the bug.
#include <unity.h>

#include <cstdint>
#include <cstdio>

#include "game/Apply.h"
#include "game/MoveGen.h"
#include "game/Notation.h"

namespace perft_helpers {

inline uint64_t perft(const checkers::Board& b, int depth) {
  if (depth == 0) return 1;
  checkers::MoveList l;
  checkers::legalMoves(b, l);
  TEST_ASSERT_FALSE_MESSAGE(l.overflowed(), "MoveList overflow during perft");
  if (depth == 1) return l.size();
  uint64_t n = 0;
  for (const checkers::Move& m : l) n += perft(checkers::applyMove(b, m), depth - 1);
  return n;
}

// Prints perft(depth - 1) after each root move of b.
inline void divide(const checkers::Board& b, int depth) {
  checkers::MoveList l;
  checkers::legalMoves(b, l);
  char pos[checkers::kPositionStringMax];
  b.toString(pos, sizeof pos);
  std::printf("divide(%d) of %s:\n", depth, pos);
  for (const checkers::Move& m : l) {
    char text[checkers::kMoveStringMax];
    if (!checkers::moveToString(m, text, sizeof text)) std::snprintf(text, sizeof text, "?");
    std::printf("  %-12s %llu\n", text,
                static_cast<unsigned long long>(perft(checkers::applyMove(b, m), depth - 1)));
  }
}

// Asserts perft(b, depth) == expected; prints a divide first when it is not.
inline void checkPerft(const checkers::Board& b, int depth, uint64_t expected) {
  const uint64_t got = perft(b, depth);
  if (got != expected) {
    std::printf("perft(%d) = %llu, expected %llu\n", depth, static_cast<unsigned long long>(got),
                static_cast<unsigned long long>(expected));
    divide(b, depth);
  }
  TEST_ASSERT_EQUAL_UINT64(expected, got);
}

}  // namespace perft_helpers
