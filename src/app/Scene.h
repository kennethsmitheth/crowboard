// Scene interface and the services every scene may use.
#pragma once

#include <stdint.h>

#include "../hal/AiWorker.h"
#include "../hal/crowpanel7_lgfx.h"
#include "../ui/Renderer.h"
#include "../ui/Sprites.h"
#include "../ui/TouchEvent.h"
#include "../services/HighScores.h"
#include "../services/SaveGame.h"
#include "../services/Settings.h"
#include "../services/Stats.h"
#include "GameConfig.h"

namespace checkers::app {

class SceneManager;
class Nav;

struct SceneContext {
  LGFX& gfx;
  ui::Renderer& renderer;
  ui::Sprites& sprites;
  hal::AiWorker& ai;
  SceneManager& scenes;  // low-level transitions; prefer ctx.nav.go(SceneId::X)
  Nav& nav;              // scene-id based navigation (app/Nav.h)
  ::Settings& settings;  // loaded at boot; call save() after changing data()
  HighScores& highScores;
  Stats& stats;
  SaveGame& saveGame;
  GameConfig& config;  // menus write it, Play reads it
};

class Scene {
 public:
  virtual ~Scene() = default;
  // Called when the scene becomes current (also when a scene above it is popped): install the
  // painters on ctx.renderer. The SceneManager marks the whole screen dirty after any
  // transition, so the painters must be able to repaint everything they own on request.
  virtual void enter(SceneContext&) {}
  virtual void exit(SceneContext&) {}
  virtual void handle(const ui::TouchEvent&, SceneContext&) {}
  virtual void update(uint32_t nowMs, uint32_t dtMs, SceneContext&) {}
  // Mark dirty regions. The manager calls renderer.flush() once after render().
  virtual void render(SceneContext&) = 0;
};

}  // namespace checkers::app
