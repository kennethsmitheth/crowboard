// Sidebar renderer (x 480..800): two player rows, a turn line, the clock of the side to move, an
// optional status line and the Undo / Hint / Menu buttons (ARCHITECTURE section 12). Plugs into
// Renderer as its RectPainter; setModel() marks only the regions whose values changed.
#pragma once

#include "BoardViewState.h"  // Board.h (Side) with the Arduino bit() macro guarded
#include "DirtyRects.h"
#include "Renderer.h"

namespace checkers::ui {

enum class HudButton : uint8_t { Undo, Hint, Menu };

struct HudModel {
  char p1Initials[4] = "P1";  // Side::Black, dark pieces
  char p2Initials[4] = "P2";  // Side::White, ivory pieces
  uint8_t p1Count = 12, p2Count = 12;
  uint8_t p1Captured = 0, p2Captured = 0;
  Side toMove = Side::Black;
  // Clock of each side in seconds, shown as mm:ss for the side to move. Both 0 = untimed game:
  // no clock is drawn.
  uint32_t clockP1Sec = 0, clockP2Sec = 0;
  // Two-player games: each player row shows its own clock (the side to move highlighted) and the
  // single clock in the turn row is not drawn. Clocks are drawn even at 00:00.
  bool dualClocks = false;
  bool showButtons = true;
  bool undoEnabled = true, hintEnabled = true;
  const char* status = nullptr;  // optional line under the turn row; copied by setModel()
};

class Hud : public RectPainter {
 public:
  explicit Hud(Renderer& renderer) : renderer_(renderer) {}

  // Stores the model (status text is copied, so the caller's pointer need not stay valid) and
  // marks the changed regions dirty. The first call marks the whole sidebar.
  void setModel(const HudModel& model);
  const HudModel& model() const { return model_; }

  // Marks the whole sidebar dirty.
  void invalidateAll();

  // Screen rectangle of a button, for hit-testing and drawing.
  static Rect buttonRect(HudButton b);
  // True when (x, y) is inside a visible, enabled button; `out` is set only then.
  bool hitButton(int16_t x, int16_t y, HudButton& out) const;

  void paintRect(LGFX& gfx, const Rect& r) override;

 private:
  static bool enabled(const HudModel& m, HudButton b);

  Renderer& renderer_;
  HudModel model_;
  char statusBuf_[48] = "";
  bool haveModel_ = false;
};

}  // namespace checkers::ui
