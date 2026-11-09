// Main Menu: Resume (when a save exists), Play vs Computer, Two Players, High Scores, Settings.
#pragma once

#include <stdint.h>

#include "../../ui/Widgets.h"
#include "../Nav.h"
#include "../Scene.h"

namespace checkers::app {

class MainMenuScene : public Scene {
 public:
  MainMenuScene();

  void enter(SceneContext& ctx) override;
  void update(uint32_t nowMs, uint32_t dtMs, SceneContext& ctx) override;
  void render(SceneContext& ctx) override;

  static constexpr uint32_t kStatusMs = 1500;

 private:
  void layoutButtons();
  void go(SceneId id);
  void showComingSoon();

  static void onResume(void* self);
  static void onPlayComputer(void* self);
  static void onTwoPlayers(void* self);
  static void onHighScores(void* self);
  static void onSettings(void* self);

  ui::WidgetGroup group_;
  ui::Label title_;
  ui::Label status_;
  ui::Button resume_;
  ui::Button playComputer_;
  ui::Button twoPlayers_;
  ui::Button highScores_;
  ui::Button settings_;
  SceneContext* ctx_ = nullptr;
  uint32_t nowMs_ = 0;
  uint32_t statusUntilMs_ = 0;
  bool statusShown_ = false;
};

}  // namespace checkers::app
