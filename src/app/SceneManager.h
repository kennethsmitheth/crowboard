// Owns the scene stack and runs one frame: transitions, touch, update, render, flush.
// Scenes are statically allocated by the app; the manager never allocates.
#pragma once

#include <stdint.h>

#include "../hal/Touch.h"
#include "Scene.h"
#include "SceneStack.h"
#include "TouchGate.h"

namespace checkers::app {

class SceneManager {
 public:
  static constexpr int kDepth = 4;

  // Transitions are applied at the top of the next tick(), never mid-frame.
  void replace(Scene* s) { stack_.requestReplace(s); }
  void push(Scene* s) { stack_.requestPush(s); }
  void pop() { stack_.requestPop(); }

  Scene* current() const { return stack_.current(); }
  int depth() const { return stack_.depth(); }

  void tick(uint32_t nowMs, hal::Touch& touch, SceneContext& ctx);

 private:
  void applyPending(SceneContext& ctx);

  SceneStack<Scene, kDepth> stack_;
  TouchGate gate_;  // drops touches that began on the previous scene
  uint32_t lastMs_ = 0;
  bool started_ = false;
};

}  // namespace checkers::app
