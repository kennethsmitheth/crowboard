#pragma once
// Game state wrapper: Board + undo history + result detection (ARCHITECTURE §5, §6). Pure C++17,
// no heap; all storage is fixed-capacity (kMaxPlies).
#include <cstdint>

#include "game/Board.h"
#include "game/Move.h"
#include "game/MoveList.h"
#include "game/Zobrist.h"

namespace checkers {

enum class Result : uint8_t { Ongoing, P1Wins, P2Wins, Draw };  // P1 is Side::Black
enum class Reason : uint8_t {
  None,
  NoPieces,
  NoMoves,
  FortyMoveRule,
  Repetition,
  Agreement,
  Resign,
  MoveLimit  // the fixed history (kMaxPlies) is full; declared a draw so the game can end
};

struct Rules {
  bool forcedCapture = true;
};

// History capacity. A game that reaches it is drawn (Reason::MoveLimit) because apply() cannot
// record another move; the 40-move rule keeps real games far below it.
constexpr uint16_t kMaxPlies = 256;
// 40 moves by each side without a capture or man move.
constexpr uint16_t kFortyMovePlies = 80;

// Position hash used for repetition detection: the Zobrist key (game/Zobrist.h). Game keeps it
// up to date incrementally with zobristApply(); this computes it from scratch.
inline uint64_t hashBoard(const Board& b) {
  return zobristHash(b);
}

class Game {
 public:
  Game() { reset(); }

  void reset(const Rules& rules = {});
  // Starts from an arbitrary position (ply 0). Returns false (state unchanged) if the board is
  // inconsistent (overlapping pieces). Terminal conditions of the position itself (no pieces /
  // no moves) are detected.
  bool resetFrom(const Board& b, const Rules& rules = {});

  const Board& board() const { return board_; }
  Side sideToMove() const { return board_.side; }
  uint16_t ply() const { return ply_; }
  // Plies since the last capture or man move.
  uint16_t halfmoveClock() const { return halfmove_; }
  const Rules& rules() const { return rules_; }

  // Legal moves for the side to move (cached per position).
  const MoveList& legalMoves();

  // Plays m if it is in legalMoves(), the game is ongoing and the history is not full.
  bool apply(const Move& m);
  bool undo();
  bool canUndo() const { return ply_ > 0; }

  Result result() const { return result_; }
  Reason reason() const { return reason_; }
  bool isOver() const { return result_ != Result::Ongoing; }
  void resign(Side loser);
  void agreeDraw();

  uint16_t historySize() const { return ply_; }
  const Move& historyAt(uint16_t i) const { return moves_[i]; }

  // Number of `s` pieces lost so far (12 minus its current piece count).
  uint32_t captured(Side s) const;

 private:
  struct Undo {
    Board board;
    uint16_t halfmove;
    uint64_t hash;
    Result result;
    Reason reason;
  };

  void evaluate();
  void setWinner(Side winner, Reason r);

  Board board_;
  Rules rules_;
  uint16_t ply_ = 0;
  uint16_t halfmove_ = 0;
  Result result_ = Result::Ongoing;
  Reason reason_ = Reason::None;
  Move moves_[kMaxPlies];
  Undo undo_[kMaxPlies];
  uint64_t hashes_[kMaxPlies + 1];  // hashes_[i] = position hash after i plies
  MoveList legal_;
  bool legalValid_ = false;
};

}  // namespace checkers
