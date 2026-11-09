#include "SettingsScene.h"

#include <stdio.h>

#include "../../ui/Theme.h"
#include "../Nav.h"
#include "../Version.h"

namespace checkers::app {
namespace {

const char* const kDifficultyNames[] = {"Easy", "Medium", "Hard"};
const char* const kThemeNames[] = {"Classic", "Slate"};
constexpr int kBrightnessStep = 10;

// Next brightness on the 10-point grid in direction dir (+1 or -1), kept within 10..100.
// An off-grid stored value (for example 85) steps onto the grid.
int stepBrightness(int v, int dir) {
  const int next = dir < 0 ? ((v - 1) / kBrightnessStep) * kBrightnessStep
                           : (v / kBrightnessStep + 1) * kBrightnessStep;
  if (next < ::Settings::kMinBrightness) return ::Settings::kMinBrightness;
  if (next > ::Settings::kMaxBrightness) return ::Settings::kMaxBrightness;
  return next;
}

}  // namespace

SettingsScene::SettingsScene() {
  title_.init(16, 8, 768, 48, "Settings", ui::Label::Size::Title);

  brightLabel_.init(16, 56, 240, 56, "Brightness", ui::Label::Size::Bold);
  brightDown_.init(272, 56, 72, 56, "-", onBrightnessDown, this);
  brightValue_.init(352, 56, 128, 56, "", ui::Label::Size::Bold, ui::Label::Align::Center);
  brightUp_.init(488, 56, 72, 56, "+", onBrightnessUp, this);

  forced_.init(16, 116, 768, 56, "Forced capture", false, onForcedCapture, this);
  flip_.init(16, 176, 768, 56, "Flip board in two-player", false, onFlipBoard, this);

  diffLabel_.init(16, 236, 400, 56, "Default difficulty", ui::Label::Size::Bold);
  diffBtn_.init(432, 236, 352, 56, "", onDifficulty, this);

  themeLabel_.init(16, 296, 400, 56, "Theme", ui::Label::Size::Bold);
  themeBtn_.init(432, 296, 352, 56, "", onTheme, this);
  themeNote_.init(16, 354, 768, 28, "Theme applies to the board in a later update.");

  resetBtn_.init(16, 388, 280, 64, "Reset scores and data", onReset, this);
  restoreBtn_.init(312, 388, 240, 64, "Restore defaults", onRestoreDefaults, this);
  backBtn_.init(568, 388, 216, 64, "Back", onBack, this);

  snprintf(aboutText_, sizeof aboutText_, "Firmware %s, built %s", kFirmwareVersion, __DATE__);
  aboutLabel_.init(16, 456, 768, 24, aboutText_);

  confirm_.init(160, 120, 480, 240, onConfirmReset, this);
  confirm_.hide();

  group_.add(&title_);
  group_.add(&brightLabel_);
  group_.add(&brightDown_);
  group_.add(&brightValue_);
  group_.add(&brightUp_);
  group_.add(&forced_);
  group_.add(&flip_);
  group_.add(&diffLabel_);
  group_.add(&diffBtn_);
  group_.add(&themeLabel_);
  group_.add(&themeBtn_);
  group_.add(&themeNote_);
  group_.add(&resetBtn_);
  group_.add(&restoreBtn_);
  group_.add(&backBtn_);
  group_.add(&aboutLabel_);
  group_.add(&confirm_);
}

void SettingsScene::enter(SceneContext& ctx) {
  ctx_ = &ctx;
  confirm_.hide();
  syncWidgets();
  ctx.renderer.setCellPainter(nullptr);
  ctx.renderer.setRectPainter(&group_);
  group_.invalidateAll();
  group_.collectDirty(ctx.renderer.dirty());
}

void SettingsScene::handle(const ui::TouchEvent& ev, SceneContext&) {
  group_.dispatch(ev);
}

void SettingsScene::render(SceneContext& ctx) {
  group_.collectDirty(ctx.renderer.dirty());
}

void SettingsScene::syncWidgets() {
  const SettingsData& d = ctx_->settings.data();
  snprintf(brightText_, sizeof brightText_, "%u%%", static_cast<unsigned>(d.brightness));
  brightValue_.setText(brightText_);
  forced_.setOn(d.forcedCapture);
  flip_.setOn(d.flipBoard);
  const int diff = d.defaultDifficulty <= ::Settings::kMaxDifficulty
                       ? d.defaultDifficulty
                       : ::Settings::kDefaultDifficulty;
  diffBtn_.setLabel(kDifficultyNames[diff]);
  themeBtn_.setLabel(kThemeNames[d.theme == 1 ? 1 : 0]);
}

void SettingsScene::applyBrightness() {
  ctx_->gfx.setBrightness(static_cast<uint8_t>(ctx_->settings.data().brightness * 255u / 100u));
}

void SettingsScene::onBrightnessDown(void* self) {
  auto* s = static_cast<SettingsScene*>(self);
  SettingsData& d = s->ctx_->settings.data();
  d.brightness = static_cast<uint8_t>(stepBrightness(d.brightness, -1));
  s->ctx_->settings.save();
  s->applyBrightness();
  s->syncWidgets();
}

void SettingsScene::onBrightnessUp(void* self) {
  auto* s = static_cast<SettingsScene*>(self);
  SettingsData& d = s->ctx_->settings.data();
  d.brightness = static_cast<uint8_t>(stepBrightness(d.brightness, +1));
  s->ctx_->settings.save();
  s->applyBrightness();
  s->syncWidgets();
}

void SettingsScene::onForcedCapture(void* self, bool on) {
  auto* s = static_cast<SettingsScene*>(self);
  s->ctx_->settings.data().forcedCapture = on;
  s->ctx_->settings.save();
}

void SettingsScene::onFlipBoard(void* self, bool on) {
  auto* s = static_cast<SettingsScene*>(self);
  s->ctx_->settings.data().flipBoard = on;
  s->ctx_->settings.save();
}

void SettingsScene::onDifficulty(void* self) {
  auto* s = static_cast<SettingsScene*>(self);
  SettingsData& d = s->ctx_->settings.data();
  d.defaultDifficulty =
      static_cast<uint8_t>((d.defaultDifficulty + 1) % (::Settings::kMaxDifficulty + 1));
  s->ctx_->settings.save();
  s->syncWidgets();
}

void SettingsScene::onTheme(void* self) {
  auto* s = static_cast<SettingsScene*>(self);
  SettingsData& d = s->ctx_->settings.data();
  d.theme = d.theme == 1 ? 0 : 1;  // stored only; the board palette follows in a later update
  s->ctx_->settings.save();
  s->syncWidgets();
}

void SettingsScene::onReset(void* self) {
  static_cast<SettingsScene*>(self)->confirm_.show(
      "Reset scores and data?",
      "Clears high scores, stats and the saved game.\nThis cannot be undone.", "Cancel", "Reset");
}

void SettingsScene::onConfirmReset(void* self, int button) {
  auto* s = static_cast<SettingsScene*>(self);  // the Modal hides itself before this call
  if (button != 1) return;
  s->ctx_->highScores.reset();
  s->ctx_->highScores.save();
  s->ctx_->stats.reset();
  s->ctx_->stats.save();
  s->ctx_->saveGame.erase();
}

void SettingsScene::onRestoreDefaults(void* self) {
  auto* s = static_cast<SettingsScene*>(self);
  s->ctx_->settings.resetToDefaults();
  s->ctx_->settings.save();
  s->applyBrightness();
  s->syncWidgets();
}

void SettingsScene::onBack(void* self) {
  static_cast<SettingsScene*>(self)->ctx_->nav.go(SceneId::MainMenu);
}

}  // namespace checkers::app
