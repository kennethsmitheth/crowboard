// Pre-rendered checkers pieces held in PSRAM sprites, drawn with a transparent colour key.
// See docs/ARCHITECTURE.md section 7.
#pragma once

#include <cstddef>
#include <cstdint>

// Arduino.h defines a bit() macro that collides with checkers::bit() in Board.h.
#pragma push_macro("bit")
#undef bit
#include "../game/Board.h"
#pragma pop_macro("bit")
#include "../hal/crowpanel7_lgfx.h"

namespace checkers::ui {

enum class PieceKind : uint8_t { P1Man, P1King, P2Man, P2King };

// Black pieces are player 1 (dark, bottom), white pieces are player 2 (ivory, top).
// Returns false for Piece::None.
constexpr bool pieceKindOf(Piece p, PieceKind& out) {
  switch (p) {
    case Piece::BlackMan:
      out = PieceKind::P1Man;
      return true;
    case Piece::BlackKing:
      out = PieceKind::P1King;
      return true;
    case Piece::WhiteMan:
      out = PieceKind::P2Man;
      return true;
    case Piece::WhiteKing:
      out = PieceKind::P2King;
      return true;
    default:
      return false;
  }
}

// Transparent colour key (RGB565 magenta). No piece colour in Theme is near it, and begin()
// nudges any blended pixel that would equal it, so a key pixel always means "transparent".
constexpr uint16_t kSpriteKey = 0xF81F;

class Sprites {
 public:
  // Renders the four sprites. Returns false if a PSRAM allocation failed.
  bool begin(LGFX& gfx);
  // Draws the piece with its cell's top-left corner at (x, y); the cell background shows through.
  void draw(LGFX& target, PieceKind kind, int16_t x, int16_t y);
  // Draws a board piece; Piece::None is ignored.
  void draw(LGFX& target, Piece piece, int16_t x, int16_t y);
  // PSRAM bytes held by the sprite pixel buffers.
  size_t bytes() const;

 private:
  LGFX_Sprite sprites_[4];
  bool ready_ = false;
};

}  // namespace checkers::ui
