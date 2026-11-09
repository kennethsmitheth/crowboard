#include "BoardView.h"

#include "BoardGeometry.h"

namespace checkers::ui {

namespace {
constexpr int kLastW = 3;     // last-move outline width
constexpr int kPendingW = 4;  // pending-path outline width
constexpr int kDotR = 10;     // legal target dot radius
constexpr int kForcedR = 27;  // forced-capture ring: radii 27..29 around the 24 px piece
constexpr int kHintW = 3;     // hint outline (Hint button), drawn over the last-move outline
}  // namespace

void BoardView::markMask(uint32_t squares) {
  for (; squares; squares &= squares - 1) {
    renderer_.dirty().markSquare(viewSquare(toNotation(__builtin_ctz(squares)), state_.flipped));
  }
}

void BoardView::setBoard(const Board* board) {
  const BoardViewState& s = state_;
  markMask(changedSquares(&shown_, s, board, s));
  board_ = board;
  shown_ = board ? *board : Board::emptyBoard();
}

void BoardView::setState(const BoardViewState& state) {
  // The board may have been edited in place since setBoard(); compare against the snapshot.
  const Board* current = board_;
  markMask(changedSquares(&shown_, state_, current, state));
  if (current) shown_ = *current;
  const bool flipChanged = state_.flipped != state.flipped;
  state_ = state;
  if (flipChanged) invalidateAll();  // every cell moves
}

void BoardView::invalidateSquare(Square sq) {
  if (isValidSquare(sq)) renderer_.dirty().markSquare(viewSquare(toNotation(sq), state_.flipped));
}

void BoardView::invalidateAll() {
  for (int row = 0; row < kBoardCells; ++row) renderer_.dirty().markRow(row);
}

void BoardView::paintCell(LGFX& gfx, int row, int col, int16_t x, int16_t y) {
  int boardRow = row, boardCol = col;
  boardRowCol(row, col, state_.flipped, boardRow, boardCol);  // screen cell -> board cell
  const Square sq = squareFromRowCol(boardRow, boardCol);
  if (sq == kNoSquare) {
    gfx.fillRect(x, y, kCell, kCell, kSquareLight);
    return;
  }
  const BoardViewState& s = state_;
  const uint32_t b = uint32_t{1} << sq;
  const int n = toNotation(sq);

  const bool selected = s.selected == n;
  gfx.fillRect(x, y, kCell, kCell, selected ? kSelect : kSquareDark);

  if (n == s.lastFrom || n == s.lastTo) {
    for (int i = 0; i < kLastW; ++i) {
      gfx.drawRect(x + i, y + i, kCell - 2 * i, kCell - 2 * i, kLastMove);
    }
  }
  if (detail::pendingMask(s) & b) {
    for (int i = 0; i < kPendingW; ++i) {
      gfx.drawRect(x + i, y + i, kCell - 2 * i, kCell - 2 * i, kSelect);
    }
  }
  if (s.legalTargets & b) gfx.fillCircle(x + kCell / 2, y + kCell / 2, kDotR, kLegalTarget);
  if (s.forcedPieces & b) {
    for (int r = kForcedR; r < kForcedR + 3; ++r) {
      gfx.drawCircle(x + kCell / 2, y + kCell / 2, r, kForcedCapture);
    }
  }
  if (n == s.hintFrom || n == s.hintTo) {
    for (int i = 0; i < kHintW; ++i) {
      gfx.drawRect(x + i, y + i, kCell - 2 * i, kCell - 2 * i, kHint);
    }
  }
  if (board_ && s.hiddenSquare != n) sprites_.draw(gfx, board_->pieceAt(sq), x, y);
}

}  // namespace checkers::ui
