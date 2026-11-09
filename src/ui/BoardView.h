// Renders the 8x8 board: square colours, pieces (via Sprites) and highlights, repainting only
// the cells whose appearance changed. Plugs into Renderer as its CellPainter.
//
// Usage:
//   BoardView view(renderer, sprites);
//   renderer.setCellPainter(&view);
//   view.setBoard(&board);   // the board must outlive the view; call again after it changes
//   view.setState(state);    // marks only the changed cells dirty; renderer.flush() paints them
//   view.invalidateAll();    // marks all 64 cells (e.g. after another painter used the screen)
//
// setBoard() and setState() diff against what was last set (see changedSquares()). If the Board
// object is modified in place, the view cannot see the old contents: call setBoard() again with
// the cells you know changed via invalidateSquare(), or use invalidateAll().
#pragma once

#include "BoardViewState.h"
#include "Renderer.h"
#include "Sprites.h"

namespace checkers::ui {

class BoardView : public CellPainter {
 public:
  BoardView(Renderer& renderer, Sprites& sprites) : renderer_(renderer), sprites_(sprites) {}

  // Pointer to the board to draw (may be null: empty board). Marks cells whose piece differs
  // from the previously set board's snapshot.
  void setBoard(const Board* board);
  // Sets the highlights; marks only the cells that changed (also against the board snapshot).
  void setState(const BoardViewState& state);
  const Board* board() const { return board_; }
  const BoardViewState& state() const { return state_; }

  // Marks one square (0-based) or every cell dirty.
  void invalidateSquare(Square sq);
  void invalidateAll();

  void paintCell(LGFX& gfx, int row, int col, int16_t x, int16_t y) override;

 private:
  void markMask(uint32_t squares);

  Renderer& renderer_;
  Sprites& sprites_;
  const Board* board_ = nullptr;
  Board shown_ = Board::emptyBoard();  // snapshot of the board as last marked, for diffing
  BoardViewState state_;
};

}  // namespace checkers::ui
