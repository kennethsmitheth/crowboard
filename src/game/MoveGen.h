#pragma once
// Move generation (ARCHITECTURE §5). Pure C++17, no heap. Jump recursion is one level per
// landing, bounded by kMaxPathLen.
//
// None of these functions clear `out`; they append. When `out` fills up, MoveList sets its
// overflow flag and further moves are dropped.
#include "game/Board.h"
#include "game/MoveList.h"

namespace checkers {

// Appends the non-capturing moves for b.side. Men step one square diagonally forward onto an
// empty square, kings one square in any direction. `promotes` is set when a man lands on its
// promotion row. Ignores the forced-capture rule (see legalMoves()).
void simpleMoves(const Board& b, MoveList& out);

// Appends every complete capture sequence for b.side (American rules):
// * a jump goes over an adjacent enemy piece onto the empty square directly beyond; men jump
//   forward only, kings in all four directions (one square over, no flying kings);
// * the same piece keeps jumping from each landing square while it can; only complete
//   sequences are emitted (a sequence that could continue is never emitted cut short);
// * a piece captured earlier in the sequence stays on the board as a blocker until the move
//   ends but cannot be jumped again; the moving piece's origin square counts as empty, so a
//   king may pass through or finish on it;
// * a man that lands on its promotion row stops there (`promotes` = true) even if it could
//   jump on as a king.
// Each sequence is a distinct Move: path[] lists the landing squares, captured the jumped
// squares. Sequences sharing from/to but differing in path or captured set are all emitted.
// A sequence is cut at kMaxPathLen landings (not reachable in legal play).
void captureMoves(const Board& b, MoveList& out);

// Appends the legal moves for b.side. With forcedCapture (the standard rule) only captures are
// returned when any exist (any capture, not necessarily the longest), otherwise simple moves.
// With forcedCapture == false, captures followed by simple moves are returned.
void legalMoves(const Board& b, MoveList& out, bool forcedCapture = true);

// Fast bitboard queries for the search and the evaluation (no Move is built). They agree with
// the generators above for every position (test_movegen checks this on random positions).
// True when b.side has at least one capture.
bool hasCapture(const Board& b);
// Number of moves simpleMoves() would append for b.side.
int countSimpleMoves(const Board& b);
// legalMoves(b, list, true).size() for an empty list: the capture sequences when any exist
// (generated, so this is only cheap when there are none), otherwise countSimpleMoves().
int countLegalMoves(const Board& b);

}  // namespace checkers
