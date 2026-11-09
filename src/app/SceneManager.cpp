#include "SceneManager.h"

namespace checkers::app {

void SceneManager::applyPending(SceneContext& ctx) {
  stack_.applyPending([&ctx](Scene* s) { s->exit(ctx); }, [&ctx](Scene* s) { s->enter(ctx); });
}

void SceneManager::tick(uint32_t nowMs, hal::Touch& touch, SceneContext& ctx) {
  const bool transition = stack_.hasPending();
  applyPending(ctx);
  Scene* s = stack_.current();
  if (s == nullptr) return;

  // A scene only marks its own regions on enter() (widget rectangles, the board, the sidebar),
  // so after a change of scene the whole screen is marked here: every cell and one full-screen
  // rectangle. The new scene's painters repaint it and nothing of the previous scene stays
  // visible around their widgets (the board used to remain around the Main Menu's buttons).
  if (transition) ctx.renderer.dirty().markAll();

  const uint32_t dt = started_ ? nowMs - lastMs_ : 0;
  lastMs_ = nowMs;
  started_ = true;

  // handle() and update() only queue transitions, so s stays current for the whole frame.
  ui::TouchEvent ev;
  while (touch.pop(ev)) s->handle(ev, ctx);
  s->update(nowMs, dt, ctx);
  s->render(ctx);
  ctx.renderer.flush();
}

}  // namespace checkers::app
