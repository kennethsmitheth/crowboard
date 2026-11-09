// Scene interface and the services every scene may use.
#pragma once

#include <stdint.h>

#include "../hal/AiWorker.h"
#include "../hal/crowpanel7_lgfx.h"
#include "../ui/Renderer.h"
#include "../ui/Sprites.h"
#include "../ui/TouchEvent.h"

namespace checkers::app {

class SceneManager;

struct SceneContext {
  LGFX& gfx;
  ui::Renderer& renderer;
  ui::Sprites& sprites;
  hal::AiWorker& ai;
  SceneManager& scenes;  // request transitions: ctx.scenes.replace(&next)
  // Services (settings, stats, high scores, save game) are added by later issues.
};

class Scene {
 public:
  virtual ~Scene() = default;
  // Called when the scene becomes current (also when a scene above it is popped): install the
  // painters on ctx.renderer and mark everything dirty.
  virtual void enter(SceneContext&) {}
  virtual void exit(SceneContext&) {}
  virtual void handle(const ui::TouchEvent&, SceneContext&) {}
  virtual void update(uint32_t nowMs, uint32_t dtMs, SceneContext&) {}
  // Mark dirty regions. The manager calls renderer.flush() once after render().
  virtual void render(SceneContext&) = 0;
};

}  // namespace checkers::app
