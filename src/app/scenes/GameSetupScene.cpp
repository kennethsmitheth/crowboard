#include "GameSetupScene.h"

#include <string.h>

#include "../../ui/Theme.h"
#include "../Nav.h"

namespace checkers::app {
namespace {

const char* const kDifficultyNames[] = {"Easy", "Medium", "Hard"};
constexpr int kRowTop = 72;
constexpr int kRowStep = 72;  // 64 px row plus 8 px gap
constexpr int kLabelW = 280;
constexpr int kValueX = 312;
constexpr int kValueW = 472;     // difficulty and side buttons
constexpr int kInitialsW = 200;  // initials shown in the row
constexpr int kEditX = 528;
constexpr int kEditW = 256;
constexpr int kStatusY = 352;
constexpr int kStatusH = 28;
constexpr int kActionY = 400;
constexpr int kStartW = 380;
constexpr int kBackX = 412;
constexpr int kBackW = 372;

// Copies the stored initials up to the first space, for display ("P1 " shows as "P1").
void trimInitials(const char* stored, char out[4]) {
  int n = 0;
  for (; n < 3 && stored[n] != '\0' && stored[n] != ' '; ++n) out[n] = stored[n];
  out[n] = '\0';
}

// Text to prefill the Keypad with. The Keypad takes letters only, so initials that hold anything
// else (the default "P1") start empty and the player types new ones.
void keypadPrefill(const char* stored, char out[4]) {
  int n = 0;
  for (; n < 3 && stored[n] != '\0' && stored[n] != ' '; ++n) {
    if (stored[n] < 'A' || stored[n] > 'Z') {
      n = 0;
      break;
    }
    out[n] = stored[n];
  }
  out[n] = '\0';
}

// Stores the typed letters in the 3-slot form used by Settings, padded with spaces.
void storeInitials(char dst[4], const char* typed) {
  bool ended = false;
  for (int i = 0; i < 3; ++i) {
    if (!ended && typed[i] != '\0') {
      dst[i] = typed[i];
    } else {
      ended = true;
      dst[i] = ' ';
    }
  }
  dst[3] = '\0';
}

// Moves a row of widgets to its slot, keeping each widget's x and size.
void placeRow(ui::CoreWidget* const widgets[], int count, int slot) {
  const auto y = static_cast<int16_t>(kRowTop + slot * kRowStep);
  for (int i = 0; i < count; ++i) {
    ui::CoreWidget* w = widgets[i];
    w->setBounds(ui::Rect{w->bounds.x, y, w->bounds.w, w->bounds.h});
  }
}

}  // namespace

GameSetupScene::GameSetupScene() {
  title_.init(16, 8, 768, 48, "", ui::Label::Size::Title);

  diffLabel_.init(16, kRowTop, kLabelW, ui::kButtonH, "Difficulty", ui::Label::Size::Bold);
  diffBtn_.init(kValueX, kRowTop, kValueW, ui::kButtonH, "", onDifficulty, this);
  sideLabel_.init(16, kRowTop, kLabelW, ui::kButtonH, "You play", ui::Label::Size::Bold);
  sideBtn_.init(kValueX, kRowTop, kValueW, ui::kButtonH, "", onSide, this);

  rowALabel_.init(16, kRowTop, kLabelW, ui::kButtonH, "", ui::Label::Size::Bold);
  rowAValue_.init(kValueX, kRowTop, kInitialsW, ui::kButtonH, "", ui::Label::Size::Bold,
                  ui::Label::Align::Center);
  rowAEdit_.init(kEditX, kRowTop, kEditW, ui::kButtonH, "Edit", onEditA, this);
  rowBLabel_.init(16, kRowTop, kLabelW, ui::kButtonH, "", ui::Label::Size::Bold);
  rowBValue_.init(kValueX, kRowTop, kInitialsW, ui::kButtonH, "", ui::Label::Size::Bold,
                  ui::Label::Align::Center);
  rowBEdit_.init(kEditX, kRowTop, kEditW, ui::kButtonH, "Edit", onEditB, this);

  status_.init(0, kStatusY, ui::kScreenW, kStatusH, "", ui::Label::Size::Bold,
               ui::Label::Align::Center);
  status_.setColor(ui::kHudAccent);
  status_.setVisible(false);

  startBtn_.init(16, kActionY, kStartW, ui::kButtonH, "Start", onStart, this);
  backBtn_.init(kBackX, kActionY, kBackW, ui::kButtonH, "Back", onBack, this);

  keypad_.init(0, 0, ui::kScreenW, ui::kScreenH, onKeypadOk, this);
  keypad_.setVisible(false);

  group_.add(&title_);
  group_.add(&diffLabel_);
  group_.add(&diffBtn_);
  group_.add(&sideLabel_);
  group_.add(&sideBtn_);
  group_.add(&rowALabel_);
  group_.add(&rowAValue_);
  group_.add(&rowAEdit_);
  group_.add(&rowBLabel_);
  group_.add(&rowBValue_);
  group_.add(&rowBEdit_);
  group_.add(&status_);
  group_.add(&startBtn_);
  group_.add(&backBtn_);
  group_.add(&keypad_);
}

void GameSetupScene::enter(SceneContext& ctx) {
  ctx_ = &ctx;
  const SettingsData& d = ctx.settings.data();
  mode_ = ctx.config.mode;
  difficulty_ = static_cast<checkers::Difficulty>(d.defaultDifficulty <= ::Settings::kMaxDifficulty
                                                      ? d.defaultDifficulty
                                                      : ::Settings::kDefaultDifficulty);
  humanSide_ = ctx.config.humanSide;
  memcpy(initials_[0], d.p1Initials, sizeof initials_[0]);
  memcpy(initials_[1], d.p2Initials, sizeof initials_[1]);
  initials_[0][3] = '\0';
  initials_[1][3] = '\0';

  statusShown_ = false;
  status_.setVisible(false);
  keypad_.setVisible(false);
  syncWidgets();

  ctx.renderer.setCellPainter(nullptr);
  ctx.renderer.setRectPainter(&group_);
  group_.invalidateAll();
  group_.collectDirty(ctx.renderer.dirty());
}

void GameSetupScene::syncWidgets() {
  const bool vs = mode_ == GameConfig::Mode::VsComputer;
  title_.setText(vs ? "Play vs Computer" : "Two Players");

  diffLabel_.setVisible(vs);
  diffBtn_.setVisible(vs);
  sideLabel_.setVisible(vs);
  sideBtn_.setVisible(vs);
  diffBtn_.setLabel(kDifficultyNames[static_cast<int>(difficulty_)]);
  sideBtn_.setLabel(humanSide_ == checkers::Side::Black ? "Dark (moves first)" : "Light");

  rowALabel_.setText(vs ? "Your initials" : "Player 1 (dark)");
  rowBLabel_.setText("Player 2 (light)");
  rowBLabel_.setVisible(!vs);
  rowBValue_.setVisible(!vs);
  rowBEdit_.setVisible(!vs);

  char text[4];
  trimInitials(initials_[sideIndex(rowSide(0))], text);
  rowAValue_.setText(text);
  trimInitials(initials_[sideIndex(rowSide(1))], text);
  rowBValue_.setText(text);

  // Play vs Computer: difficulty and side take slots 0 and 1, the human's row slot 2.
  // Two Players: the two initials rows take slots 0 and 1.
  ui::CoreWidget* diffRow[] = {&diffLabel_, &diffBtn_};
  ui::CoreWidget* sideRow[] = {&sideLabel_, &sideBtn_};
  ui::CoreWidget* rowA[] = {&rowALabel_, &rowAValue_, &rowAEdit_};
  ui::CoreWidget* rowB[] = {&rowBLabel_, &rowBValue_, &rowBEdit_};
  placeRow(diffRow, 2, 0);
  placeRow(sideRow, 2, 1);
  placeRow(rowA, 3, vs ? 2 : 0);
  placeRow(rowB, 3, 1);
}

checkers::Side GameSetupScene::rowSide(int row) const {
  if (mode_ == GameConfig::Mode::VsComputer) return humanSide_;
  return row == 0 ? checkers::Side::Black : checkers::Side::White;
}

void GameSetupScene::openKeypad(checkers::Side side) {
  editing_ = side;
  char text[4];
  keypadPrefill(initials_[sideIndex(side)], text);
  keypad_.setText(text);
  keypad_.setVisible(true);
}

void GameSetupScene::update(uint32_t nowMs, uint32_t, SceneContext&) {
  nowMs_ = nowMs;
  // Wrap-safe comparison: the status is over once nowMs has reached statusUntilMs_.
  if (statusShown_ && static_cast<int32_t>(nowMs - statusUntilMs_) >= 0) {
    statusShown_ = false;
    status_.setVisible(false);
  }
}

void GameSetupScene::handle(const ui::TouchEvent& ev, SceneContext&) {
  group_.dispatch(ev);
}

void GameSetupScene::render(SceneContext& ctx) {
  group_.collectDirty(ctx.renderer.dirty());
}

void GameSetupScene::showComingSoon() {
  status_.setText("Coming soon");
  status_.setVisible(true);
  statusShown_ = true;
  statusUntilMs_ = nowMs_ + kStatusMs;
}

void GameSetupScene::onDifficulty(void* self) {
  auto* s = static_cast<GameSetupScene*>(self);
  s->difficulty_ =
      static_cast<checkers::Difficulty>((static_cast<int>(s->difficulty_) + 1) % kDifficultyCount);
  s->syncWidgets();
}

void GameSetupScene::onSide(void* self) {
  auto* s = static_cast<GameSetupScene*>(self);
  s->humanSide_ = checkers::opposite(s->humanSide_);
  s->syncWidgets();
}

void GameSetupScene::onEditA(void* self) {
  auto* s = static_cast<GameSetupScene*>(self);
  s->openKeypad(s->rowSide(0));
}

void GameSetupScene::onEditB(void* self) {
  auto* s = static_cast<GameSetupScene*>(self);
  s->openKeypad(s->rowSide(1));
}

void GameSetupScene::onKeypadOk(void* self, const char* initials) {
  auto* s = static_cast<GameSetupScene*>(self);
  storeInitials(s->initials_[sideIndex(s->editing_)], initials);
  s->keypad_.setVisible(false);  // dirties the full screen, so the rows below repaint
  s->syncWidgets();
}

void GameSetupScene::onStart(void* self) {
  auto* s = static_cast<GameSetupScene*>(self);
  if (!s->ctx_) return;
  SceneContext& ctx = *s->ctx_;

  SettingsData& d = ctx.settings.data();
  d.defaultDifficulty = static_cast<uint8_t>(s->difficulty_);
  memcpy(d.p1Initials, s->initials_[0], sizeof d.p1Initials);
  memcpy(d.p2Initials, s->initials_[1], sizeof d.p2Initials);
  ctx.settings.save();

  GameConfig& c = ctx.config;
  c.mode = s->mode_;
  c.difficulty = s->difficulty_;
  c.humanSide = s->humanSide_;
  memcpy(c.p1Initials, s->initials_[0], sizeof c.p1Initials);
  memcpy(c.p2Initials, s->initials_[1], sizeof c.p2Initials);
  c.resume = false;

  if (!ctx.nav.go(SceneId::Play)) s->showComingSoon();
}

void GameSetupScene::onBack(void* self) {
  auto* s = static_cast<GameSetupScene*>(self);
  if (!s->ctx_) return;
  s->ctx_->nav.go(SceneId::MainMenu);
}

}  // namespace checkers::app
