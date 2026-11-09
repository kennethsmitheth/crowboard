// Non-blocking, time-based board animations: a piece sliding along a (jump) path, a capture fade
// and a promotion pulse. See docs/ARCHITECTURE.md section 7.
//
// The Animator holds no game logic. The caller (Play scene) passes the piece and the Move, calls
// update(nowMs) once per loop iteration, then renderer.flush(). When busy() turns false the
// caller applies the move to its Game and BoardView::setBoard() in the same tick, before the
// flush, so the origin square never shows the old piece for a frame.
//
// How it draws: update() marks the cells under the sprite's previous and new position; the
// Renderer repaints them through BoardView, then calls this class as its OverlayPainter, which
// draws the sprite (or disc / ring) on top. The sliding piece's origin is hidden through
// BoardViewState::hiddenSquare for the duration. Square arguments: promotePulse takes 1-32
// notation numbers, fadeCapture takes Move::captured (bit i = 0-based square i).
//
//   animator.slidePiece(Piece::BlackKing, move, millis());
//   loop: animator.update(millis()); renderer.flush();
#pragma once

#include <stdint.h>

#include "../game/Move.h"
#include "BoardView.h"
#include "Renderer.h"
#include "Sprites.h"
#include "Tween.h"

namespace checkers::ui {

class Animator : public OverlayPainter {
 public:
  Animator(Renderer& renderer, Sprites& sprites, BoardView& view);

  // Slides `piece` from move.from through every landing in move.path. Replaces a slide in
  // progress. A move with pathLen 0 does nothing.
  void slidePiece(Piece piece, const Move& move, uint32_t nowMs, uint32_t msPerSegment = 220);
  // Shrinks a disc over each captured piece (radius 25 to 0), erasing it from the board view.
  // The board itself is not changed: the caller removes the pieces afterwards.
  void fadeCapture(uint32_t capturedMask, uint32_t nowMs, uint32_t durationMs = 200);
  // An amber ring expanding and fading on the square.
  void promotePulse(int square1to32, uint32_t nowMs, uint32_t durationMs = 400);

  bool busy() const { return slide_.active || fade_.active || pulse_.active; }
  // Advances all animations and marks the dirty cells; the caller flushes the renderer.
  void update(uint32_t nowMs);
  // Stops everything at once and repaints the affected cells.
  void cancel();

  void paintOverlay(LGFX& gfx) override;

 private:
  struct Slide {
    bool active = false;
    Piece piece = Piece::None;
    PathTween tween;
    uint32_t startMs = 0;
    int hidden = 0;  // origin square, 1-32
    Point pos;       // where the sprite is drawn now
  };
  struct Fade {
    bool active = false;
    uint32_t mask = 0;
    uint32_t startMs = 0;
    uint32_t durationMs = 1;
    int radius = 0;
  };
  struct Pulse {
    bool active = false;
    int square = 0;
    uint32_t startMs = 0;
    uint32_t durationMs = 1;
    float t = 0;
  };

  void markSprite(const Point& p);
  void markSquares(uint32_t mask);
  void setHidden(int square);
  static uint32_t elapsed(uint32_t now, uint32_t start) { return now - start; }

  Renderer& renderer_;
  Sprites& sprites_;
  BoardView& view_;
  Slide slide_;
  Fade fade_;
  Pulse pulse_;
};

}  // namespace checkers::ui
