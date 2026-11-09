// Rendering experiments and a test animation for issue #20, driven from the serial console:
//   bench              run the timing experiments and print the numbers
//   anim [mode] [sec]  slide a disc around the board rim for sec seconds (default 8) through the
//                      dirty-cell path; mode = beam (default) | vsync | raw
//   render             print the Renderer's flush statistics
//   view 0|1|2         show a demo BoardView state: 0 initial position, 1 selected piece with
//                      legal targets and last move, 2 mid-capture with forced-capture rings and
//                      a pending path (needs attachView())
//   hud think          toggle the HUD status line ("Computer thinking...")
//   slide              from view 1: slide the king 21 -> 14 -> 7, fade two captures, pulse a
//                      promotion, logging frame times (needs attachAnimator())
//   widgets            show the touch widget demo (WidgetsDemo, issue #23)
// Results and how to read them: docs/rendering-notes.md.
#pragma once

#include <stdint.h>

#include "../hal/crowpanel7_lgfx.h"
#include "Animator.h"
#include "BoardView.h"
#include "Hud.h"
#include "Renderer.h"
#include "WidgetsDemo.h"

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

  // Hands the BoardView / Hud used by the `view` and `hud` commands to the bench.
  void attachView(BoardView* view, Hud* hud) {
    view_ = view;
    hud_ = hud;
  }
  // Switches to demo view 0..2 (BoardView state + HudModel). Marks what changed; call
  // renderer.flush() afterwards.
  void showView(int n);

  // Hands the Animator used by the `slide` command to the bench.
  void attachAnimator(Animator* a) { animator_ = a; }
  // True while the `slide` demo runs; the main loop then calls tickSlide() instead of the
  // scenes (the bench owns the screen). tickSlide updates the Animator, flushes the renderer
  // and steps through slide -> capture fade -> promotion pulse, logging frame times.
  bool slideRunning() const { return slideStage_ != 0; }
  void tickSlide(uint32_t nowMs);

  void runAll();
  void anim(AnimMode mode, uint32_t seconds);
  void printStats();
  WidgetsDemo& widgetsDemo() { return widgetsDemo_; }

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

  void buildViews();
  BoardView* view_ = nullptr;
  Hud* hud_ = nullptr;
  Board boards_[3];
  BoardViewState states_[3];
  HudModel hudModel_;
  Animator* animator_ = nullptr;
  int slideStage_ = 0;  // 0 idle, 1 slide, 2 capture fade, 3 promotion pulse
  uint32_t stageStartMs_ = 0;
  Move slideMove_{};
  void startSlideStage(int stage, uint32_t nowMs);
  void logSlideStage(uint32_t nowMs);
  bool thinking_ = false;
  bool viewsBuilt_ = false;
  WidgetsDemo widgetsDemo_;
};

}  // namespace checkers::ui
