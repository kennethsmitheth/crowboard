// Settings scene (issue #38): brightness steps applied live, forced capture, default difficulty,
// theme choice, reset of scores and data behind a confirmation, restore defaults and About.
// Every change is saved to the settings service at once.
#pragma once

#include <stdint.h>

#include "../../ui/Widgets.h"
#include "../Scene.h"

namespace checkers::app {

class SettingsScene : public Scene {
 public:
  SettingsScene();

  void enter(SceneContext& ctx) override;
  void handle(const ui::TouchEvent& ev, SceneContext& ctx) override;
  void render(SceneContext& ctx) override;

 private:
  static void onBrightnessDown(void* self);
  static void onBrightnessUp(void* self);
  static void onForcedCapture(void* self, bool on);
  static void onDifficulty(void* self);
  static void onTheme(void* self);
  static void onReset(void* self);
  static void onConfirmReset(void* self, int button);
  static void onRestoreDefaults(void* self);
  static void onBack(void* self);

  // Copies the stored values into the widgets (labels, toggle, value buttons).
  void syncWidgets();
  // Sets the backlight from settings.data().brightness.
  void applyBrightness();

  SceneContext* ctx_ = nullptr;
  ui::WidgetGroup group_;
  ui::Label title_;
  ui::Label brightLabel_;
  ui::Button brightDown_;
  ui::Label brightValue_;
  ui::Button brightUp_;
  ui::Toggle forced_;
  ui::Label diffLabel_;
  ui::Button diffBtn_;
  ui::Label themeLabel_;
  ui::Button themeBtn_;
  ui::Label themeNote_;
  ui::Button resetBtn_;
  ui::Button restoreBtn_;
  ui::Button backBtn_;
  ui::Label aboutLabel_;
  ui::Modal confirm_;  // last in the group: on top and modal
  char brightText_[8] = {};
  char aboutText_[64] = {};
};

}  // namespace checkers::app
