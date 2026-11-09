// Rendering experiments and a test animation for issue #20, driven from the serial console:
//   bench              run the timing experiments and print the numbers
//   anim [mode] [sec]  slide a disc around the board rim for sec seconds (default 8) through the
//                      dirty-cell path; mode = beam (default) | vsync | raw
//   render             print the Renderer's flush statistics
// Results and how to read them: docs/rendering-notes.md.
#pragma once

#include <stdint.h>

#include "../hal/crowpanel7_lgfx.h"
#include "Renderer.h"

namespace checkers::ui {

// Paints a MockScreen-style board (squares, initial pieces, one king) plus an optional moving
// disc, as the Renderer's CellPainter. Also the painter used by the experiments.
class BenchCellPainter : public CellPainter {
 public:
  void paintCell(LGFX& gfx, int row, int col, int16_t x, int16_t y) override;

  void setDisc(bool visible, int16_t cx, int16_t cy) {
    disc_ = visible;
    cx_ = cx;
    cy_ = cy;
  }
  static Rect discBounds(int16_t cx, int16_t cy);

  // Same look as MockScreen's piece: rim, shadow, body, gloss, optional crown.
  static void drawPiece(lgfx::LovyanGFX& g, int cx, int cy, bool darkSide, bool king);
  static void paintStatic(lgfx::LovyanGFX& g, int row, int col, int16_t x, int16_t y);

 private:
  bool disc_ = false;
  int16_t cx_ = 0;
  int16_t cy_ = 0;
};

class RenderBench {
 public:
  enum class AnimMode : uint8_t {
    Beam,   // beam-synced flushes, one per panel refresh
    Vsync,  // flush right after vsync, no beam avoidance
    Raw,    // free-running at ~33 Hz, unrelated to the scan-out
  };

  RenderBench(LGFX& gfx, Renderer& renderer) : gfx_(gfx), renderer_(renderer) {}

  // Handles bench / anim / render and appends to "help". Returns true if it consumed the line.
  bool handleCommand(const char* line);

  void runAll();
  void anim(AnimMode mode, uint32_t seconds);
  void printStats();

 private:
  void benchFullBoard();
  void benchFillRects();
  void benchCell();
  void benchSprite();
  void benchScanout();
  void benchMemory();
  void restoreBoard();

  LGFX& gfx_;
  Renderer& renderer_;
  BenchCellPainter painter_;
};

}  // namespace checkers::ui
