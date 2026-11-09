// Scene ids and the navigator that maps them to statically allocated scenes.
// Scenes move around with ctx.nav.go(SceneId::X) instead of holding pointers to each other;
// main.cpp registers each scene once with nav.set().
#pragma once

#include <stdint.h>

#include "Scene.h"
#include "SceneManager.h"

namespace checkers::app {

enum class SceneId : uint8_t {
  Splash,
  MainMenu,
  GameSetup,
  Play,
  GameOver,
  HighScores,
  Settings,
  Count
};
constexpr int kSceneCount = static_cast<int>(SceneId::Count);

class Nav {
 public:
  explicit Nav(SceneManager& scenes) : scenes_(scenes) {}

  void set(SceneId id, Scene* s) { table_[idx(id)] = s; }
  Scene* get(SceneId id) const { return table_[idx(id)]; }
  bool has(SceneId id) const { return get(id) != nullptr; }

  // Transitions are queued on the SceneManager and applied at the next tick.
  bool go(SceneId id) {
    Scene* s = get(id);
    if (!s) return false;
    scenes_.replace(s);
    return true;
  }
  bool push(SceneId id) {
    Scene* s = get(id);
    if (!s) return false;
    scenes_.push(s);
    return true;
  }
  void pop() { scenes_.pop(); }

 private:
  static int idx(SceneId id) { return static_cast<int>(id); }
  SceneManager& scenes_;
  Scene* table_[kSceneCount] = {};
};

}  // namespace checkers::app
