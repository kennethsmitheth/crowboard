#include "MainMenuScene.h"

#include "../../ui/Theme.h"
#include "../GameConfig.h"
#include "../Nav.h"

namespace checkers::app {
namespace {

constexpr int kButtonW = 420;
constexpr int kButtonGap = 10;
constexpr int kListTop = 112;  // below the title and the status line
constexpr int kStatusY = 76;
constexpr int kStatusH = 28;

}  // namespace

MainMenuScene::MainMenuScene() {
  constexpr int kX = (ui::kScreenW - kButtonW) / 2;
  title_.init(0, 16, ui::kScreenW, 56, "Main Menu", ui::Label::Size::Title,
              ui::Label::Align::Center);
  status_.init(0, kStatusY, ui::kScreenW, kStatusH, "", ui::Label::Size::Bold,
               ui::Label::Align::Center);
  status_.setColor(ui::kHudAccent);
  status_.setVisible(false);

  resume_.init(kX, kListTop, kButtonW, ui::kButtonH, "Resume", onResume, this);
  playComputer_.init(kX, kListTop, kButtonW, ui::kButtonH, "Play vs Computer", onPlayComputer,
                     this);
  twoPlayers_.init(kX, kListTop, kButtonW, ui::kButtonH, "Two Players", onTwoPlayers, this);
  highScores_.init(kX, kListTop, kButtonW, ui::kButtonH, "High Scores", onHighScores, this);
  settings_.init(kX, kListTop, kButtonW, ui::kButtonH, "Settings", onSettings, this);

  group_.add(&title_);
  group_.add(&status_);
  group_.add(&resume_);
  group_.add(&playComputer_);
  group_.add(&twoPlayers_);
  group_.add(&highScores_);
  group_.add(&settings_);
}

void MainMenuScene::enter(SceneContext& ctx) {
  ctx_ = &ctx;
  statusShown_ = false;
  status_.setVisible(false);
  resume_.setVisible(ctx.saveGame.exists());
  layoutButtons();

  ctx.renderer.setCellPainter(nullptr);
  ctx.renderer.setRectPainter(&group_);
  group_.invalidateAll();
  group_.collectDirty(ctx.renderer.dirty());
}

// Stacks the visible buttons from the top of the list area, so a hidden Resume leaves no gap.
void MainMenuScene::layoutButtons() {
  ui::Button* const buttons[] = {&resume_, &playComputer_, &twoPlayers_, &highScores_, &settings_};
  int y = kListTop;
  for (ui::Button* b : buttons) {
    if (!b->visible) continue;
    b->setBounds(ui::Rect{b->bounds.x, static_cast<int16_t>(y), b->bounds.w, b->bounds.h});
    y += ui::kButtonH + kButtonGap;
  }
}

void MainMenuScene::update(uint32_t nowMs, uint32_t, SceneContext&) {
  nowMs_ = nowMs;
  // Wrap-safe comparison: the status is over once nowMs has reached statusUntilMs_.
  if (statusShown_ && static_cast<int32_t>(nowMs - statusUntilMs_) >= 0) {
    statusShown_ = false;
    status_.setVisible(false);
  }
}

void MainMenuScene::render(SceneContext& ctx) {
  group_.collectDirty(ctx.renderer.dirty());
}

void MainMenuScene::go(SceneId id) {
  if (ctx_ == nullptr) return;
  if (!ctx_->nav.go(id)) showComingSoon();
}

void MainMenuScene::showComingSoon() {
  status_.setText("Coming soon");
  status_.setVisible(true);
  statusShown_ = true;
  statusUntilMs_ = nowMs_ + kStatusMs;
}

void MainMenuScene::onResume(void* self) {
  auto* s = static_cast<MainMenuScene*>(self);
  if (!s->ctx_) return;
  s->ctx_->config.resume = true;
  s->go(SceneId::Play);
}

void MainMenuScene::onPlayComputer(void* self) {
  auto* s = static_cast<MainMenuScene*>(self);
  if (!s->ctx_) return;
  s->ctx_->config.mode = GameConfig::Mode::VsComputer;
  s->ctx_->config.resume = false;
  s->go(SceneId::GameSetup);
}

void MainMenuScene::onTwoPlayers(void* self) {
  auto* s = static_cast<MainMenuScene*>(self);
  if (!s->ctx_) return;
  s->ctx_->config.mode = GameConfig::Mode::TwoPlayers;
  s->ctx_->config.resume = false;
  s->go(SceneId::GameSetup);
}

void MainMenuScene::onHighScores(void* self) {
  static_cast<MainMenuScene*>(self)->go(SceneId::HighScores);
}

void MainMenuScene::onSettings(void* self) {
  static_cast<MainMenuScene*>(self)->go(SceneId::Settings);
}

}  // namespace checkers::app
