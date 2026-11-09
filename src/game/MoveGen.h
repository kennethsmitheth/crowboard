#pragma once
// Move generation. Pure C++17, no heap.
#include "game/Board.h"
#include "game/MoveList.h"

namespace checkers {

// Appends the non-capturing moves for b.side to `out` (does not clear it). Men step one square
// diagonally forward onto an empty square, kings one square in any direction. `promotes` is set
// when a man lands on its promotion row. Captures and the forced-capture rule are not handled.
void simpleMoves(const Board& b, MoveList& out);

}  // namespace checkers
