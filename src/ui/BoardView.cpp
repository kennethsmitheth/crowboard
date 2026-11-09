#include "BoardView.h"

namespace checkers::ui {

namespace {
constexpr int kLastW = 3;     // last-move outline width
constexpr int kPendingW = 4;  // pending-path outline width
constexpr int kDotR = 10;     // legal target dot radius
constexpr int kForcedR = 27;  // forced-capture ring: radii 27..29 around the 24 px piece
}  // namespace

void BoardView::markMask(uint32_t squares) {
  for (; squares; squares &= squares - 1) {
    renderer_.dirty().markSquare(toNotation(__builtin_ctz(squares)));
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
  state_ = state;
}

void BoardView::invalidateSquare(Square sq) {
  if (isValidSquare(sq)) renderer_.dirty().markSquare(toNotation(sq));
}

void BoardView::invalidateAll() {
  for (int row = 0; row < kBoardCells; ++row) renderer_.dirty().markRow(row);
}

void BoardView::paintCell(LGFX& gfx, int row, int col, int16_t x, int16_t y) {
  const Square sq = squareFromRowCol(row, col);
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
  if (board_ && s.hiddenSquare != n) sprites_.draw(gfx, board_->pieceAt(sq), x, y);
}

}  // namespace checkers::ui
