#include "Sprites.h"

#include "Theme.h"

namespace checkers::ui {
namespace {

constexpr int kScale = 2;  // supersampling factor
constexpr int kHi = kCell * kScale;
constexpr int kRadius = 25;  // 50 px disc inside the 60 px cell

struct Rgb {
  int r, g, b;
};

Rgb unpack(uint16_t c) {
  return {((c >> 11) & 0x1F) * 255 / 31, ((c >> 5) & 0x3F) * 255 / 63, (c & 0x1F) * 255 / 31};
}

uint16_t pack(int r, int g, int b) {
  return rgb565(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
}

// Draws the piece at 2x with hard edges onto a key-filled sprite.
void renderHi(LGFX_Sprite& hi, bool p1, bool king) {
  const uint16_t rim = p1 ? kP1Rim : kP2Rim;
  const uint16_t shadow = p1 ? kP1Shadow : kP2Shadow;
  const uint16_t body = p1 ? kP1Body : kP2Body;
  const uint16_t gloss = p1 ? kP1Highlight : kP2Highlight;
  const int c = kHi / 2;
  const int r = kRadius * kScale;
  const int bodyR = r - kPieceRimW * kScale;
  const int s = kScale;

  hi.fillScreen(kSpriteKey);
  hi.fillCircle(c, c, r, rim);
  hi.fillCircle(c, c, bodyR, shadow);                 // darker bevel edge
  hi.fillCircle(c, c, bodyR - 2 * s, body);           // face
  hi.fillCircle(c - 8 * s, c - 8 * s, 4 * s, gloss);  // inner highlight
  if (king) {
    hi.fillRect(c - 11 * s, c + 1 * s, 22 * s, 8 * s, kCrown);  // band
    hi.fillTriangle(c - 11 * s, c + 1 * s, c - 7 * s, c - 9 * s, c - 2 * s, c + 1 * s, kCrown);
    hi.fillTriangle(c - 4 * s, c + 1 * s, c, c - 13 * s, c + 4 * s, c + 1 * s, kCrown);
    hi.fillTriangle(c + 2 * s, c + 1 * s, c + 7 * s, c - 9 * s, c + 11 * s, c + 1 * s, kCrown);
  }
}

// Box-filters the 2x sprite into out. Key pixels are excluded from the colour average so the key
// never bleeds into the edge. Partly covered pixels are blended toward kSquareDark (the square
// pieces stand on) by their coverage; fully uncovered pixels become the key.
void downsample(LGFX_Sprite& hi, LGFX_Sprite& out) {
  const auto* src = static_cast<const uint16_t*>(hi.getBuffer());
  const Rgb bg = unpack(kSquareDark);
  const int total = kScale * kScale;
  for (int y = 0; y < kCell; ++y) {
    for (int x = 0; x < kCell; ++x) {
      int n = 0, r = 0, g = 0, b = 0;
      for (int dy = 0; dy < kScale; ++dy) {
        for (int dx = 0; dx < kScale; ++dx) {
          // Sprite buffers hold big-endian RGB565.
          const uint16_t v = __builtin_bswap16(src[(y * kScale + dy) * kHi + x * kScale + dx]);
          if (v == kSpriteKey) continue;
          const Rgb p = unpack(v);
          r += p.r;
          g += p.g;
          b += p.b;
          ++n;
        }
      }
      uint16_t px = kSpriteKey;
      if (n > 0) {
        px = pack((r + bg.r * (total - n)) / total, (g + bg.g * (total - n)) / total,
                  (b + bg.b * (total - n)) / total);
        if (px == kSpriteKey) px ^= 0x0020;  // never emit the key for a visible pixel
      }
      out.drawPixel(x, y, px);
    }
  }
}

}  // namespace

bool Sprites::begin(LGFX& gfx) {
  ready_ = false;
  LGFX_Sprite hi(&gfx);
  hi.setColorDepth(16);
  hi.setPsram(true);
  if (!hi.createSprite(kHi, kHi)) return false;

  for (int i = 0; i < 4; ++i) {
    const PieceKind kind = static_cast<PieceKind>(i);
    LGFX_Sprite& s = sprites_[i];
    s.setColorDepth(16);
    s.setPsram(true);
    if (!s.createSprite(kCell, kCell)) return false;
    renderHi(hi, kind == PieceKind::P1Man || kind == PieceKind::P1King,
             kind == PieceKind::P1King || kind == PieceKind::P2King);
    downsample(hi, s);
  }
  hi.deleteSprite();  // temporary 2x buffer is released
  ready_ = true;
  return true;
}

void Sprites::draw(LGFX& target, PieceKind kind, int16_t x, int16_t y) {
  if (!ready_) return;
  sprites_[static_cast<int>(kind)].pushSprite(&target, x, y, kSpriteKey);
}

void Sprites::draw(LGFX& target, Piece piece, int16_t x, int16_t y) {
  PieceKind kind;
  if (pieceKindOf(piece, kind)) draw(target, kind, x, y);
}

size_t Sprites::bytes() const {
  size_t total = 0;
  for (const auto& s : sprites_) total += static_cast<size_t>(s.width()) * s.height() * 2;
  return total;
}

}  // namespace checkers::ui
