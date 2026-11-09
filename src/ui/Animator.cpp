#include "Animator.h"

#include "BoardGeometry.h"
#include "Theme.h"

namespace checkers::ui {

namespace {
constexpr int kFadeR0 = 25;   // capture disc start radius
constexpr int kEraseR = 26;   // covers the piece sprite (radius 24 plus rim anti-aliasing)
constexpr int kPulseR0 = 12;  // pulse ring radii; the ring is kPulseW wide and stays in the cell
constexpr int kPulseR1 = 27;
constexpr int kPulseW = 3;

uint16_t pieceBodyColor(const Board* board, int square1to32) {
  if (!board) return kSelect;
  switch (board->pieceAt(fromNotation(square1to32))) {
    case Piece::BlackMan:
    case Piece::BlackKing:
      return kP1Body;
    case Piece::WhiteMan:
    case Piece::WhiteKing:
      return kP2Body;
    default:
      return kSelect;
  }
}
}  // namespace

Animator::Animator(Renderer& renderer, Sprites& sprites, BoardView& view)
    : renderer_(renderer), sprites_(sprites), view_(view) {
  renderer_.setOverlay(this);
}

void Animator::markSprite(const Point& p) {
  renderer_.dirty().markCellsUnder(p.x, p.y, kCell, kCell);
}

void Animator::markSquares(uint32_t mask) {
  for (; mask; mask &= mask - 1) renderer_.dirty().markSquare(toNotation(__builtin_ctz(mask)));
}

void Animator::setHidden(int square) {
  BoardViewState s = view_.state();
  s.hiddenSquare = square;
  view_.setState(s);  // marks the old and new hidden squares dirty
}

void Animator::slidePiece(Piece piece, const Move& move, uint32_t nowMs, uint32_t msPerSegment) {
  if (move.pathLen == 0 || move.pathLen > kMaxPathLen) return;
  if (slide_.active) markSprite(slide_.pos);
  Point pts[PathTween::kMaxPoints];
  int n = 0;
  int16_t x, y;
  cellRect(toNotation(move.from), x, y);
  pts[n++] = Point{x, y};
  for (int i = 0; i < move.pathLen; ++i) {
    cellRect(toNotation(move.path[i]), x, y);
    pts[n++] = Point{x, y};
  }
  slide_.active = true;
  slide_.piece = piece;
  slide_.tween.set(pts, n, msPerSegment);
  slide_.startMs = nowMs;
  slide_.hidden = toNotation(move.from);
  slide_.pos = pts[0];
  setHidden(slide_.hidden);
  markSprite(slide_.pos);
}

void Animator::fadeCapture(uint32_t capturedMask, uint32_t nowMs, uint32_t durationMs) {
  if (fade_.active) markSquares(fade_.mask);
  fade_.active = capturedMask != 0;
  fade_.mask = capturedMask;
  fade_.startMs = nowMs;
  fade_.durationMs = durationMs ? durationMs : 1;
  fade_.radius = kFadeR0;
  markSquares(capturedMask);
}

void Animator::promotePulse(int square1to32, uint32_t nowMs, uint32_t durationMs) {
  if (square1to32 < 1 || square1to32 > 32) return;
  if (pulse_.active) renderer_.dirty().markSquare(pulse_.square);
  pulse_.active = true;
  pulse_.square = square1to32;
  pulse_.startMs = nowMs;
  pulse_.durationMs = durationMs ? durationMs : 1;
  pulse_.t = 0;
  renderer_.dirty().markSquare(square1to32);
}

void Animator::update(uint32_t nowMs) {
  if (slide_.active) {
    const uint32_t t = elapsed(nowMs, slide_.startMs);
    if (slide_.tween.done(t)) {
      markSprite(slide_.pos);
      slide_.active = false;
      setHidden(0);
    } else {
      const Point p = slide_.tween.at(t);
      if (p.x != slide_.pos.x || p.y != slide_.pos.y) {
        markSprite(slide_.pos);  // restore what the sprite covered
        markSprite(p);
        slide_.pos = p;
      }
    }
  }
  if (fade_.active) {
    const uint32_t t = elapsed(nowMs, fade_.startMs);
    markSquares(fade_.mask);
    if (t >= fade_.durationMs) {
      fade_.active = false;  // the last repaint shows the board as it is
    } else {
      fade_.radius = kFadeR0 - static_cast<int>(kFadeR0 * t / fade_.durationMs);
    }
  }
  if (pulse_.active) {
    const uint32_t t = elapsed(nowMs, pulse_.startMs);
    renderer_.dirty().markSquare(pulse_.square);
    if (t >= pulse_.durationMs) {
      pulse_.active = false;
    } else {
      pulse_.t = static_cast<float>(t) / static_cast<float>(pulse_.durationMs);
    }
  }
}

void Animator::cancel() {
  if (slide_.active) {
    markSprite(slide_.pos);
    slide_.active = false;
    setHidden(0);
  }
  if (fade_.active) {
    markSquares(fade_.mask);
    fade_.active = false;
  }
  if (pulse_.active) {
    renderer_.dirty().markSquare(pulse_.square);
    pulse_.active = false;
  }
}

void Animator::paintOverlay(LGFX& gfx) {
  if (fade_.active) {
    const Board* board = view_.board();
    for (uint32_t m = fade_.mask; m; m &= m - 1) {
      const int n = toNotation(__builtin_ctz(m));
      int16_t x, y;
      cellRect(n, x, y);
      const int cx = x + kCell / 2, cy = y + kCell / 2;
      gfx.fillCircle(cx, cy, kEraseR, kSquareDark);
      if (fade_.radius > 0) gfx.fillCircle(cx, cy, fade_.radius, pieceBodyColor(board, n));
    }
  }
  if (pulse_.active) {
    int16_t x, y;
    cellRect(pulse_.square, x, y);
    const float e = easeInOutQuad(pulse_.t);
    const int r = kPulseR0 + static_cast<int>((kPulseR1 - kPulseR0) * e);
    const uint16_t c = blend565(kSelect, kSquareDark, static_cast<uint8_t>(pulse_.t * 255.0f));
    for (int i = 0; i < kPulseW; ++i) gfx.drawCircle(x + kCell / 2, y + kCell / 2, r + i, c);
  }
  if (slide_.active) sprites_.draw(gfx, slide_.piece, slide_.pos.x, slide_.pos.y);
}

}  // namespace checkers::ui
