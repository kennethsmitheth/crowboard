#include "SplashScene.h"

#include <stdio.h>

#include "../../ui/Theme.h"
#include "../Nav.h"
#include "../Version.h"

namespace checkers::app {

SplashScene::SplashScene() {
  constexpr int kW = ui::kScreenW;
  title_.init(0, 150, kW, 60, "Crowboard Checkers", ui::Label::Size::Title,
              ui::Label::Align::Center);
  version_.init(0, 230, kW, 36, "", ui::Label::Size::Body, ui::Label::Align::Center);
  version_.setColor(ui::kHudTextMuted);

  char line[48];
  snprintf(line, sizeof line, "v%s  built %s", kFirmwareVersion, __DATE__);
  version_.setText(line);

  group_.add(&title_);
  group_.add(&version_);
}

void SplashScene::enter(SceneContext& ctx) {
  ctx_ = &ctx;
  started_ = false;
  done_ = false;
  ctx.renderer.setCellPainter(nullptr);
  ctx.renderer.setRectPainter(&group_);
  group_.invalidateAll();
  group_.collectDirty(ctx.renderer.dirty());
}

void SplashScene::handle(const ui::TouchEvent& ev, SceneContext&) {
  if (ev.type == ui::TouchType::Tap) leave();
}

void SplashScene::update(uint32_t nowMs, uint32_t, SceneContext&) {
  if (!started_) {
    started_ = true;
    startMs_ = nowMs;
  }
  if (nowMs - startMs_ >= kShowMs) leave();
}

void SplashScene::render(SceneContext& ctx) {
  group_.collectDirty(ctx.renderer.dirty());
}

void SplashScene::leave() {
  if (done_ || ctx_ == nullptr) return;
  done_ = true;
  ctx_->nav.go(SceneId::MainMenu);
}

}  // namespace checkers::app
