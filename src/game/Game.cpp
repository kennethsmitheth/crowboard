#include "game/Game.h"

#include "game/Apply.h"
#include "game/MoveGen.h"

namespace checkers {

uint64_t hashBoard(const Board& b) {
  uint64_t h = 14695981039346656037ULL;
  auto mix32 = [&h](uint32_t v) {
    for (int i = 0; i < 4; ++i) {
      h ^= (v >> (8 * i)) & 0xFFu;
      h *= 1099511628211ULL;
    }
  };
  mix32(b.men[0]);
  mix32(b.men[1]);
  mix32(b.kings[0]);
  mix32(b.kings[1]);
  h ^= static_cast<uint8_t>(b.side);
  h *= 1099511628211ULL;
  return h;
}

void Game::reset(const Rules& rules) {
  resetFrom(Board::initial(), rules);
}

bool Game::resetFrom(const Board& b, const Rules& rules) {
  const bool overlap = (b.men[0] & b.kings[0]) || (b.men[1] & b.kings[1]) ||
                       (b.pieces(Side::Black) & b.pieces(Side::White));
  if (overlap) return false;
  board_ = b;
  rules_ = rules;
  ply_ = 0;
  halfmove_ = 0;
  hashes_[0] = hashBoard(board_);
  legalValid_ = false;
  evaluate();
  return true;
}

const MoveList& Game::legalMoves() {
  if (!legalValid_) {
    legal_.clear();
    checkers::legalMoves(board_, legal_, rules_.forcedCapture);
    legalValid_ = true;
  }
  return legal_;
}

bool Game::apply(const Move& m) {
  if (result_ != Result::Ongoing || ply_ >= kMaxPlies) return false;
  bool found = false;
  for (const Move& l : legalMoves()) {
    if (l == m) {
      found = true;
      break;
    }
  }
  if (!found) return false;

  const bool irreversible = m.isCapture() || (board_.men[sideIndex(board_.side)] & bit(m.from));
  undo_[ply_] = Undo{board_, halfmove_, hashes_[ply_], result_, reason_};
  moves_[ply_] = m;
  checkers::apply(board_, m);
  ++ply_;
  halfmove_ = irreversible ? 0 : static_cast<uint16_t>(halfmove_ + 1);
  hashes_[ply_] = hashBoard(board_);
  legalValid_ = false;
  evaluate();
  return true;
}

bool Game::undo() {
  if (ply_ == 0) return false;
  --ply_;
  const Undo& u = undo_[ply_];
  board_ = u.board;
  halfmove_ = u.halfmove;
  hashes_[ply_] = u.hash;
  result_ = u.result;
  reason_ = u.reason;
  legalValid_ = false;
  return true;
}

void Game::setWinner(Side winner, Reason r) {
  result_ = winner == Side::Black ? Result::P1Wins : Result::P2Wins;
  reason_ = r;
}

void Game::evaluate() {
  result_ = Result::Ongoing;
  reason_ = Reason::None;
  const Side me = board_.side;
  if (board_.pieces(me) == 0) {
    setWinner(opposite(me), Reason::NoPieces);
    return;
  }
  if (legalMoves().empty()) {
    setWinner(opposite(me), Reason::NoMoves);
    return;
  }
  if (halfmove_ >= kFortyMovePlies) {
    result_ = Result::Draw;
    reason_ = Reason::FortyMoveRule;
    return;
  }
  // Positions since the last irreversible move are hashes_[ply_ - halfmove_ .. ply_].
  int count = 0;
  for (uint16_t i = static_cast<uint16_t>(ply_ - halfmove_); i <= ply_; ++i) {
    if (hashes_[i] == hashes_[ply_]) ++count;
  }
  if (count >= 3) {
    result_ = Result::Draw;
    reason_ = Reason::Repetition;
  }
}

void Game::resign(Side loser) {
  setWinner(opposite(loser), Reason::Resign);
}

void Game::agreeDraw() {
  result_ = Result::Draw;
  reason_ = Reason::Agreement;
}

uint32_t Game::captured(Side s) const {
  uint32_t n = 0;
  for (uint32_t x = board_.pieces(s); x != 0; x &= x - 1) ++n;
  return n >= 12 ? 0 : 12 - n;
}

}  // namespace checkers
