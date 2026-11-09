// Game Setup scene (issue #34): difficulty and first-move side for Play vs Computer, initials for
// each human player through a full-screen Keypad, then Start (Play) or Back (Main Menu). Choices
// live in scene state and reach settings and GameConfig only when Start is pressed.
#pragma once

#include <stdint.h>

#include "../GameConfig.h"  // before Widgets.h: Board.h's bit() clashes with Arduino's macro
#include "../../ui/Widgets.h"
#include "../Scene.h"

namespace checkers::app {

class GameSetupScene : public Scene {
 public:
  GameSetupScene();

  void enter(SceneContext& ctx) override;
  void update(uint32_t nowMs, uint32_t dtMs, SceneContext& ctx) override;
  void handle(const ui::TouchEvent& ev, SceneContext& ctx) override;
  void render(SceneContext& ctx) override;

  static constexpr uint32_t kStatusMs = 1500;

 private:
  // Copies the working choices into the labels, buttons and row positions.
  void syncWidgets();
  // Side whose initials row (0 or 1) is shown: the human in Play vs Computer, else Dark / Light.
  checkers::Side rowSide(int row) const;
  void openKeypad(checkers::Side side);
  void showComingSoon();

  static void onDifficulty(void* self);
  static void onSide(void* self);
  static void onEditA(void* self);
  static void onEditB(void* self);
  static void onKeypadOk(void* self, const char* initials);
  static void onStart(void* self);
  static void onBack(void* self);

  SceneContext* ctx_ = nullptr;
  GameConfig::Mode mode_ = GameConfig::Mode::VsComputer;
  checkers::Difficulty difficulty_ = checkers::Difficulty::Medium;
  checkers::Side humanSide_ = checkers::Side::Black;
  char initials_[2][4] = {};  // 3 chars plus NUL, indexed by sideIndex(Side)
  checkers::Side editing_ = checkers::Side::Black;
  uint32_t nowMs_ = 0;
  uint32_t statusUntilMs_ = 0;
  bool statusShown_ = false;

  ui::WidgetGroup group_;
  ui::Label title_;
  ui::Label diffLabel_;
  ui::Button diffBtn_;
  ui::Label sideLabel_;
  ui::Button sideBtn_;
  ui::Label rowALabel_;
  ui::Label rowAValue_;
  ui::Button rowAEdit_;
  ui::Label rowBLabel_;
  ui::Label rowBValue_;
  ui::Button rowBEdit_;
  ui::Label status_;
  ui::Button startBtn_;
  ui::Button backBtn_;
  ui::Keypad keypad_;  // last in the group: full-screen overlay on top while open
};

}  // namespace checkers::app
