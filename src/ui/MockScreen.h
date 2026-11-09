// Static drawing of the checkers screen with hard-coded data. A visual check of the theme on
// the panel, not the real renderer.
#pragma once

#include "../hal/crowpanel7_lgfx.h"
#include "Sprites.h"

namespace checkers::ui {

// Draws the board (initial setup, one selected piece with two legal targets, one king) and the
// sidebar HUD (players, turn, clock, buttons). Repaints the whole screen.
void drawStaticMock(LGFX& gfx, Sprites& sprites);

}  // namespace checkers::ui
