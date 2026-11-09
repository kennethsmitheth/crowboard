// Game Over scene (issue #36): result and reason, the score breakdown (vs computer), the
// high-score initials entry when the score qualifies, and Rematch / Main Menu buttons.
// Stats and the save erase are already done by the Play scene.
#pragma once

#include <stdint.h>

#include "../../services/Scoring.h"
#include "../../ui/Widgets.h"
#include "../Scene.h"
#include "PlayScene.h"

namespace checkers::app {

class GameOverScene : public Scene, public ui::RectPainter {
 public:
  explicit GameOverScene(PlayScene& play);

  void enter(SceneContext& ctx) override;
  void handle(const ui::TouchEvent& ev, SceneContext& ctx) override;
  void render(SceneContext& ctx) override;
  void paintRect(LGFX& gfx, const ui::Rect& r) override;

 private:
  static constexpr int kLines = 5;

  static void onRematch(void* ctx);
  static void onMenu(void* ctx);
  static void onInitials(void* ctx, const char* initials);

  PlayScene& play_;
  SceneContext* ctx_ = nullptr;
  ScoreBreakdown breakdown_ = {};
  Difficulty difficulty_ = Difficulty::Medium;
  uint16_t moves_ = 0;

  ui::WidgetGroup group_;
  ui::Label headline_;
  ui::Label reason_;
  ui::Label names_[kLines];
  ui::Label values_[kLines];
  ui::Label note_;
  ui::Button rematch_;
  ui::Button menu_;
  ui::Keypad keypad_;
};

}  // namespace checkers::app
