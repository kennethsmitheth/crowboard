#pragma once
// Move notation (ARCHITECTURE §6) for logs, tests and saved games. Pure C++17, no heap.
//
// * Squares are written as the 1-32 notation numbers (toNotation()/fromNotation() in Board.h).
// * A simple move is `from-to`, e.g. `11-15`.
// * A capture is `from` followed by each landing square, each preceded by `x`, no spaces:
//   `22x15` is a single jump and `22x15x8` a double jump 22 -> 15 -> 8. Jumped pieces are not
//   written; parsing resolves the text against the legal moves of a position, so two sequences
//   with the same start and end but different landings stay distinct.
#include <cstddef>

#include "game/Board.h"
#include "game/Move.h"

namespace checkers {

// Buffer size that always fits moveToString() output plus its NUL: two digits for the start
// square, then up to kMaxPathLen landings of "x" and two digits (2 + 3 * 10 + 1 = 33).
constexpr size_t kMoveStringMax = 2 + 3 * kMaxPathLen + 1;

// Writes the notation of `m` as a NUL-terminated string. Returns false (and writes "" if
// cap > 0) when the text does not fit in `cap` bytes or `m` is malformed.
bool moveToString(const Move& m, char* out, size_t cap);

// Parses `text` against the legal moves of `board` (forced-capture rules applied). The move
// must match the start square, the separator role (`-` for a simple move, `x` for a capture)
// and the full landing path. On failure returns false and leaves `out` untouched. Rejects
// malformed text (spaces, mixed separators, leading zeros, numbers outside 1-32, trailing
// characters) as well as moves that are not legal in `board`.
bool parseMove(const char* text, const Board& board, Move& out);

}  // namespace checkers
