#include "MockScreen.h"

#include <cstdio>

#include "Theme.h"

namespace checkers::ui {
namespace {

enum class Side : uint8_t { P1, P2 };

// Playable squares are the dark squares, (row + col) odd.
constexpr bool isPlayable(int row, int col) {
  return (row + col) % 2 == 1;
}

void drawPiece(LGFX& gfx, int cx, int cy, Side side, bool king) {
  const uint16_t rim = side == Side::P1 ? kP1Rim : kP2Rim;
  const uint16_t shadow = side == Side::P1 ? kP1Shadow : kP2Shadow;
  const uint16_t body = side == Side::P1 ? kP1Body : kP2Body;
  const uint16_t gloss = side == Side::P1 ? kP1Highlight : kP2Highlight;

  const int bodyR = kPieceR - kPieceRimW;
  gfx.fillCircle(cx, cy, kPieceR, rim);
  gfx.fillCircle(cx, cy, bodyR, shadow);
  gfx.fillCircle(cx, cy, bodyR - 2, body);
  gfx.fillCircle(cx - 7, cy - 7, 4, gloss);

  if (king) {
    // Simple crown: a band with three spikes.
    gfx.fillRect(cx - 10, cy - 2, 20, 8, kCrown);
    gfx.fillTriangle(cx - 10, cy - 2, cx - 6, cy - 10, cx - 2, cy - 2, kCrown);
    gfx.fillTriangle(cx - 2, cy - 2, cx, cy - 12, cx + 2, cy - 2, kCrown);
    gfx.fillTriangle(cx + 2, cy - 2, cx + 6, cy - 10, cx + 10, cy - 2, kCrown);
  }
}

void drawBoard(LGFX& gfx) {
  for (int row = 0; row < kBoardCells; ++row) {
    for (int col = 0; col < kBoardCells; ++col) {
      const uint16_t colour = isPlayable(row, col) ? kSquareDark : kSquareLight;
      gfx.fillRect(kBoardX + col * kCell, kBoardY + row * kCell, kCell, kCell, colour);
    }
  }

  // Initial setup: 12 pieces per side. P2 (ivory) in rows 0-2, P1 (dark) in rows 5-7.
  for (int row = 0; row < kBoardCells; ++row) {
    for (int col = 0; col < kBoardCells; ++col) {
      if (!isPlayable(row, col)) continue;
      if (row >= 3 && row <= 4) continue;
      const int cx = kBoardX + col * kCell + kCell / 2;
      const int cy = kBoardY + row * kCell + kCell / 2;
      const Side side = row < 3 ? Side::P2 : Side::P1;
      // Preview a king on one P1 piece.
      const bool king = (row == 5 && col == 4);
      drawPiece(gfx, cx, cy, side, king);
    }
  }

  // Selected P1 piece at (5,4), with legal targets at (4,3) and (4,5).
  const int sx = kBoardX + 4 * kCell;
  const int sy = kBoardY + 5 * kCell;
  gfx.fillRect(sx, sy, kCell, kCell, kSelect);
  drawPiece(gfx, sx + kCell / 2, sy + kCell / 2, Side::P1, true);

  const int targetCols[2] = {3, 5};
  for (int tc : targetCols) {
    const int tx = kBoardX + tc * kCell + kCell / 2;
    const int ty = kBoardY + 4 * kCell + kCell / 2;
    gfx.fillCircle(tx, ty, 10, kLegalTarget);
  }
}

// One player row: piece icon, initials, piece count, captured count.
void drawPlayerRow(LGFX& gfx, int y, Side side, const char* initials, int pieces, int captured) {
  const int x0 = kSidebarX + kPad;
  const int mid = y + kPlayerRowH / 2;
  gfx.fillCircle(x0 + 14, mid, 12, side == Side::P1 ? kP1Body : kP2Body);
  gfx.drawCircle(x0 + 14, mid, 12, side == Side::P1 ? kP1Rim : kP2Rim);

  gfx.setTextColor(kHudText, kHudBg);
  gfx.setTextDatum(textdatum_t::middle_left);
  gfx.setFont(&fonts::FreeSansBold12pt7b);
  gfx.drawString(initials, x0 + 40, mid);

  char count[8];
  snprintf(count, sizeof(count), "x%d", pieces);
  gfx.drawString(count, x0 + 110, mid);

  char cap[16];
  snprintf(cap, sizeof(cap), "captured %d", captured);
  gfx.setTextColor(kHudTextMuted, kHudBg);
  gfx.setFont(&fonts::FreeSans12pt7b);
  gfx.setTextDatum(textdatum_t::middle_right);
  gfx.drawString(cap, kScreenW - kPad, mid);
}

void drawButton(LGFX& gfx, int x, int y, int w, const char* label, bool enabled) {
  const uint16_t face = enabled ? kButtonFace : kButtonDisabled;
  const uint16_t text = enabled ? kButtonText : kButtonDisabledText;
  gfx.fillRoundRect(x, y, w, kButtonH, 8, face);
  gfx.drawRoundRect(x, y, w, kButtonH, 8, kButtonBorder);
  gfx.setTextColor(text, face);
  gfx.setTextDatum(textdatum_t::middle_center);
  gfx.setFont(&fonts::FreeSansBold12pt7b);
  gfx.drawString(label, x + w / 2, y + kButtonH / 2);
}

void drawSidebar(LGFX& gfx) {
  gfx.fillRect(kSidebarX, kBoardY, kSidebarW, kScreenH, kHudBg);

  // Player rows.
  drawPlayerRow(gfx, kPad, Side::P1, "KEN", 12, 0);
  drawPlayerRow(gfx, kPad + kPlayerRowH, Side::P2, "CPU", 12, 0);

  // Turn indicator and clock.
  const int turnY = 168;
  gfx.fillRect(kSidebarX + kPad, turnY + 6, 4, 28, kHudAccent);
  gfx.setTextColor(kHudText, kHudBg);
  gfx.setTextDatum(textdatum_t::middle_left);
  gfx.setFont(&fonts::FreeSansBold12pt7b);
  gfx.drawString("KEN to move", kSidebarX + kPad + 14, turnY + 20);

  gfx.setTextDatum(textdatum_t::middle_right);
  gfx.setFont(&fonts::FreeSansBold24pt7b);
  gfx.drawString("03:12", kScreenW - kPad, turnY + 20);

  // Buttons: Undo and Hint on one row, Menu below.
  const int btnW = (kSidebarW - 2 * kPad - kButtonGap) / 2;  // 136
  const int row1 = 248;
  const int row2 = row1 + kButtonH + kButtonGap;
  const int x1 = kSidebarX + kPad;
  const int x2 = x1 + btnW + kButtonGap;
  drawButton(gfx, x1, row1, btnW, "Undo", true);
  drawButton(gfx, x2, row1, btnW, "Hint", true);
  drawButton(gfx, x1, row2, btnW, "Menu", true);
}

}  // namespace

void drawStaticMock(LGFX& gfx) {
  gfx.fillScreen(kHudBg);
  drawBoard(gfx);
  drawSidebar(gfx);
}

}  // namespace checkers::ui
