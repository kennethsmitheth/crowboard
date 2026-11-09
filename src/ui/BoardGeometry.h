// Board hit-testing for the 480x480 board at screen (0,0) with 60 px cells. No Arduino deps.
//
// Square numbering follows ARCHITECTURE section 6: row 0 is at the top of the screen and holds
// squares 1-4 on columns 1,3,5,7; row 1 holds 5-8 on columns 0,2,4,6; and so on. A cell is dark
// (playable) when (row + col) is odd. square = row * 4 + col / 2 + 1.
// Corners: top-left and bottom-right are light; top-right is 4 and bottom-left is 29.
#pragma once

#include <stdint.h>

#include "Theme.h"

namespace checkers::ui {

// Cell size and board size come from Theme.h (kCell, kBoardSize); kBoardCell is kept as an alias.
constexpr int kBoardCell = kCell;
static_assert(kBoardSize == 8 * kBoardCell, "Theme board metrics must match the geometry");

// Board flip (two-player view): the flipped board is the point-symmetric image of the board,
// screen row = 7 - row and screen col = 7 - col. It is its own inverse, so the same mapping goes
// board -> screen and screen -> board. For squares this is 33 - square (1 <-> 32, 4 <-> 29).
// boardRowCol <-> screenRowCol: with flipped == false both are the identity.
inline void screenRowCol(int row, int col, bool flipped, int& outRow, int& outCol) {
  outRow = flipped ? 7 - row : row;
  outCol = flipped ? 7 - col : col;
}
inline void boardRowCol(int row, int col, bool flipped, int& outRow, int& outCol) {
  screenRowCol(row, col, flipped, outRow, outCol);  // point symmetry is an involution
}

// The square (1-32) drawn where `square` normally is when the view is flipped; a square outside
// 1-32 yields 0. viewSquare(viewSquare(s, f), f) == s.
inline int viewSquare(int square, bool flipped) {
  if (square < 1 || square > 32) return 0;
  return flipped ? 33 - square : square;
}

// Returns the 1-32 square under (x, y), or 0 for light squares and points outside the board.
// With flipped == true (x, y) is a point on the flipped screen.
inline int cellAt(int16_t x, int16_t y, bool flipped = false) {
  if (x < 0 || y < 0 || x >= kBoardSize || y >= kBoardSize) return 0;
  int col = x / kBoardCell, row = y / kBoardCell;
  if (((row + col) & 1) == 0) return 0;
  return viewSquare(row * 4 + col / 2 + 1, flipped);
}

// Top-left pixel of a square (1-32) as drawn on the (possibly flipped) screen. Out-of-range
// squares yield (0, 0).
inline void cellRect(int square, int16_t& x, int16_t& y, bool flipped = false) {
  x = y = 0;
  square = viewSquare(square, flipped);
  if (square < 1) return;
  int row = (square - 1) / 4, k = (square - 1) % 4;
  int col = 2 * k + ((row & 1) ? 0 : 1);
  x = (int16_t)(col * kBoardCell);
  y = (int16_t)(row * kBoardCell);
}

}  // namespace checkers::ui
