// Splash: title, firmware version and build date. Moves to the Main Menu after 1.5 s or on tap.
#pragma once

#include <stdint.h>

#include "../../ui/Widgets.h"
#include "../Scene.h"

namespace checkers::app {

class SplashScene : public Scene {
 public:
  SplashScene();

  void enter(SceneContext& ctx) override;
  void handle(const ui::TouchEvent& ev, SceneContext& ctx) override;
  void update(uint32_t nowMs, uint32_t dtMs, SceneContext& ctx) override;
  void render(SceneContext& ctx) override;

  static constexpr uint32_t kShowMs = 1500;

 private:
  void leave();

  ui::WidgetGroup group_;
  ui::Label title_;
  ui::Label version_;
  SceneContext* ctx_ = nullptr;
  uint32_t startMs_ = 0;
  bool started_ = false;
  bool done_ = false;
};

}  // namespace checkers::app
