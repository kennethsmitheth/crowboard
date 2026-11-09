// Board hit-testing for the 480x480 board at screen (0,0) with 60 px cells. No Arduino deps.
//
// Square numbering follows ARCHITECTURE section 6: row 0 is at the top of the screen and holds
// squares 1-4 on columns 1,3,5,7; row 1 holds 5-8 on columns 0,2,4,6; and so on. A cell is dark
// (playable) when (row + col) is odd. square = row * 4 + col / 2 + 1.
// Corners: top-left and bottom-right are light; top-right is 4 and bottom-left is 29.
#pragma once

#include <stdint.h>

namespace checkers::ui {

constexpr int kBoardCell = 60;
constexpr int kBoardSize = 8 * kBoardCell;

// Returns the 1-32 square under (x, y), or 0 for light squares and points outside the board.
inline int cellAt(int16_t x, int16_t y) {
  if (x < 0 || y < 0 || x >= kBoardSize || y >= kBoardSize) return 0;
  int col = x / kBoardCell, row = y / kBoardCell;
  if (((row + col) & 1) == 0) return 0;
  return row * 4 + col / 2 + 1;
}

// Top-left pixel of a square (1-32). Out-of-range squares yield (0, 0).
inline void cellRect(int square, int16_t& x, int16_t& y) {
  x = y = 0;
  if (square < 1 || square > 32) return;
  int row = (square - 1) / 4, k = (square - 1) % 4;
  int col = 2 * k + ((row & 1) ? 0 : 1);
  x = (int16_t)(col * kBoardCell);
  y = (int16_t)(row * kBoardCell);
}

}  // namespace checkers::ui
