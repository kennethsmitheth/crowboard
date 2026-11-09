#include "DemoScenes.h"

#include "../../ui/Theme.h"

namespace checkers::app {

DemoSceneA::DemoSceneA() : DemoScene(ui::kSquareDark, ui::kHudText, "Scene A, tap to switch") {}
DemoSceneB::DemoSceneB() : DemoScene(ui::kHudBg, ui::kHudAccent, "Scene B, tap to switch") {}

void DemoScene::enter(SceneContext& ctx) {
  ctx.renderer.setCellPainter(nullptr);
  ctx.renderer.setRectPainter(this);
  ctx.renderer.dirty().markAll();
}

void DemoScene::handle(const ui::TouchEvent& ev, SceneContext& ctx) {
  if (ev.type == ui::TouchType::Tap && other_ != nullptr) ctx.scenes.replace(other_);
}

void DemoScene::paintRect(LGFX& gfx, const ui::Rect&) {
  gfx.fillRect(0, 0, ui::kScreenW, ui::kScreenH, bg_);
  gfx.setFont(&fonts::FreeSansBold24pt7b);
  gfx.setTextColor(fg_, bg_);
  gfx.setTextDatum(middle_center);
  gfx.drawString(label_, ui::kScreenW / 2, ui::kScreenH / 2);
}

}  // namespace checkers::app
