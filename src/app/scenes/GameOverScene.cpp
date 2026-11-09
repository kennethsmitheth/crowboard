#include "GameOverScene.h"

#include <stdio.h>

#include "../GameOverText.h"
#include "../Nav.h"

namespace checkers::app {
namespace {

constexpr const char* kLineNames[5] = {"Result", "Material", "Efficiency", "Speed", "Total"};
constexpr int kRowsY = 128;
constexpr int kRowH = 36;

}  // namespace

GameOverScene::GameOverScene(PlayScene& play) : play_(play) {
  headline_.init(16, 16, 768, 56, "", ui::Label::Size::Title, ui::Label::Align::Center);
  reason_.init(16, 76, 768, 32, "", ui::Label::Size::Body, ui::Label::Align::Center);
  for (int i = 0; i < kLines; ++i) {
    const ui::Label::Size sz = (i == kLines - 1) ? ui::Label::Size::Bold : ui::Label::Size::Body;
    names_[i].init(200, kRowsY + i * kRowH, 220, 32, kLineNames[i], sz, ui::Label::Align::Left);
    values_[i].init(420, kRowsY + i * kRowH, 180, 32, "", sz, ui::Label::Align::Right);
  }
  note_.init(16, 324, 768, 40, "", ui::Label::Size::Bold, ui::Label::Align::Center);
  rematch_.init(120, 400, 260, ui::kButtonH, "Rematch", onRematch, this);
  menu_.init(420, 400, 260, ui::kButtonH, "Main Menu", onMenu, this);
  keypad_.init(0, 0, ui::kScreenW, ui::kScreenH, onInitials, this);
  keypad_.setVisible(false);

  group_.add(&headline_);
  group_.add(&reason_);
  for (auto& l : names_) group_.add(&l);
  for (auto& l : values_) group_.add(&l);
  group_.add(&note_);
  group_.add(&rematch_);
  group_.add(&menu_);
  group_.add(&keypad_);  // last: on top
}

void GameOverScene::enter(SceneContext& ctx) {
  ctx_ = &ctx;
  const PlayScene::Summary& s = play_.summary();
  difficulty_ = s.config.difficulty;
  moves_ = s.humanMoves;

  char buf[48];
  headlineText(s.result, s.config, buf, sizeof buf);
  headline_.setText(buf);
  reason_.setText(reasonText(s.reason));

  const bool vsComputer = s.config.mode == GameConfig::Mode::VsComputer;
  breakdown_ = ScoreBreakdown{};
  keypad_.setVisible(false);
  note_.setText("");
  for (int i = 0; i < kLines; ++i) {
    names_[i].visible = vsComputer;
    values_[i].visible = vsComputer;
  }

  if (vsComputer) {
    breakdown_ = computeScore(humanOutcome(s.result, s.config.humanSide), s.config.difficulty,
                              s.finalBoard, s.config.humanSide, s.humanMoves, s.elapsedSec,
                              ctx.settings.data().forcedCapture);
    const uint32_t v[kLines] = {breakdown_.result, breakdown_.material, breakdown_.efficiency,
                                breakdown_.speed, breakdown_.total};
    for (int i = 0; i < kLines; ++i) {
      snprintf(buf, sizeof buf, "%lu", static_cast<unsigned long>(v[i]));
      values_[i].setText(buf);
    }
    if (!breakdown_.recorded) {
      note_.setText("Not recorded");
    } else if (ctx.highScores.qualifies(difficulty_, breakdown_.total)) {
      note_.setText("New high score!");
      keypad_.setText(s.config.p1Initials);
      keypad_.setVisible(true);
    }
  } else {
    note_.setText("Scores are recorded for games against the computer only.");
  }

  ctx.renderer.setCellPainter(nullptr);
  ctx.renderer.setRectPainter(&group_);
  group_.invalidateAll();
  group_.collectDirty(ctx.renderer.dirty());
}

void GameOverScene::handle(const ui::TouchEvent& ev, SceneContext& ctx) {
  ctx_ = &ctx;
  group_.dispatch(ev);
  group_.collectDirty(ctx.renderer.dirty());
}

void GameOverScene::render(SceneContext& ctx) {
  group_.collectDirty(ctx.renderer.dirty());
}

void GameOverScene::paintRect(LGFX& gfx, const ui::Rect& r) {
  group_.paintRect(gfx, r);
}

void GameOverScene::onRematch(void* ctx) {
  auto* s = static_cast<GameOverScene*>(ctx);
  if (!s->ctx_) return;
  s->ctx_->config.resume = false;
  s->ctx_->nav.go(SceneId::Play);
}

void GameOverScene::onMenu(void* ctx) {
  auto* s = static_cast<GameOverScene*>(ctx);
  if (s->ctx_) s->ctx_->nav.go(SceneId::MainMenu);
}

void GameOverScene::onInitials(void* ctx, const char* initials) {
  auto* s = static_cast<GameOverScene*>(ctx);
  if (!s->ctx_) return;
  SceneContext& c = *s->ctx_;
  int overall = 0;
  const int rank = c.highScores.insert(s->difficulty_, s->breakdown_.total, s->moves_, initials,
                                       c.highScores.nextGameNo(), &overall);
  c.highScores.save();
  char line[48];
  snprintf(line, sizeof line, "Rank %d (overall %d)", rank, overall);
  s->note_.setText(line);
  s->keypad_.setVisible(false);
  s->group_.invalidateAll();
}

}  // namespace checkers::app
