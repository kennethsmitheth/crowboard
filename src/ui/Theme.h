// Palette, fonts and layout metrics for the 800x480 checkers screen.
// See docs/ARCHITECTURE.md section 12 for the screen layout.
//
// No Arduino or LovyanGFX includes: this header is plain constants so it can be used
// from host tests too. Colours are RGB565 values (see rgb565()).
//
// Fonts (LovyanGFX bundled, selected by the code that draws):
//   title / clock : fonts::FreeSansBold24pt7b
//   body          : fonts::FreeSans12pt7b
//   HUD / buttons : fonts::FreeSansBold12pt7b
#pragma once

#include <cstdint>

namespace checkers::ui {

// Converts 8-bit RGB to RGB565 (5-6-5 bits).
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ---- Layout metrics (pixels) ------------------------------------------------------------------

constexpr int kScreenW = 800;
constexpr int kScreenH = 480;

constexpr int kBoardX = 0;
constexpr int kBoardY = 0;
constexpr int kBoardSize = 480;  // 8 cells
constexpr int kCell = 60;
constexpr int kBoardCells = 8;

constexpr int kSidebarX = 480;
constexpr int kSidebarW = 320;

constexpr int kPad = 16;         // side gutter inside the sidebar
constexpr int kPlayerRowH = 56;  // one row per player
constexpr int kButtonH = 64;     // touch target minimum is 64 px
constexpr int kButtonGap = 16;
constexpr int kPieceR = 24;    // piece radius; 48 px diameter inside a 60 px cell
constexpr int kPieceRimW = 4;  // outer rim width

static_assert(kCell * kBoardCells == kBoardSize, "board must be 8 cells square");
static_assert(kBoardX + kBoardSize == kSidebarX, "sidebar must start where the board ends");
static_assert(kSidebarX + kSidebarW == kScreenW, "sidebar must reach the right edge");
static_assert(kBoardSize <= kScreenH, "board must fit vertically");
static_assert(kButtonH >= 64, "touch targets must be at least 64 px tall");

// ---- Palette ----------------------------------------------------------------------------------

// Board squares: walnut and cream.
constexpr uint16_t kSquareLight = rgb565(240, 217, 181);
constexpr uint16_t kSquareDark = rgb565(181, 136, 99);

// Player 1 (dark, near-black with a grey rim), sits at the bottom.
constexpr uint16_t kP1Body = rgb565(28, 26, 24);
constexpr uint16_t kP1Shadow = rgb565(8, 8, 8);
constexpr uint16_t kP1Highlight = rgb565(92, 88, 84);
constexpr uint16_t kP1Rim = rgb565(124, 124, 124);

// Player 2 (light, ivory with a warm rim), sits at the top.
constexpr uint16_t kP2Body = rgb565(250, 246, 234);
constexpr uint16_t kP2Shadow = rgb565(200, 188, 160);
constexpr uint16_t kP2Highlight = rgb565(255, 255, 255);
constexpr uint16_t kP2Rim = rgb565(170, 140, 90);

constexpr uint16_t kCrown = rgb565(212, 175, 55);

// Board overlays.
constexpr uint16_t kSelect = rgb565(255, 193, 7);         // amber: selected piece
constexpr uint16_t kLegalTarget = rgb565(76, 175, 80);    // green: legal destination
constexpr uint16_t kLastMove = rgb565(205, 210, 90);      // olive: last move squares
constexpr uint16_t kForcedCapture = rgb565(220, 60, 60);  // red: capture is mandatory
constexpr uint16_t kHint = rgb565(0, 200, 230);           // cyan: Hint's suggested from / to

// HUD (sidebar).
constexpr uint16_t kHudBg = rgb565(34, 30, 28);
constexpr uint16_t kHudText = rgb565(245, 240, 230);
constexpr uint16_t kHudTextMuted = rgb565(170, 160, 150);
constexpr uint16_t kHudAccent = rgb565(255, 193, 7);  // turn marker

// Buttons.
constexpr uint16_t kButtonFace = rgb565(72, 62, 56);
constexpr uint16_t kButtonPressed = rgb565(48, 40, 36);
constexpr uint16_t kButtonBorder = rgb565(120, 104, 92);
constexpr uint16_t kButtonDisabled = rgb565(50, 46, 44);
constexpr uint16_t kButtonText = rgb565(245, 240, 230);
constexpr uint16_t kButtonDisabledText = rgb565(110, 104, 98);

}  // namespace checkers::ui
