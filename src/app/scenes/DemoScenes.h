// Two trivial scenes that switch on tap. Replaced by the real scenes (issues #33-#38).
#pragma once

#include "../Scene.h"
#include "../SceneManager.h"

namespace checkers::app {

class DemoScene : public Scene, public ui::RectPainter {
 public:
  void enter(SceneContext& ctx) override;
  void handle(const ui::TouchEvent& ev, SceneContext& ctx) override;
  void render(SceneContext&) override {}  // static content: dirty marked in enter()
  void paintRect(LGFX& gfx, const ui::Rect& r) override;

  void setOther(Scene* other) { other_ = other; }

 protected:
  DemoScene(uint16_t bg, uint16_t fg, const char* label) : bg_(bg), fg_(fg), label_(label) {}

 private:
  uint16_t bg_, fg_;
  const char* label_;
  Scene* other_ = nullptr;
};

class DemoSceneA : public DemoScene {
 public:
  DemoSceneA();
};

class DemoSceneB : public DemoScene {
 public:
  DemoSceneB();
};

}  // namespace checkers::app
