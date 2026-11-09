// High Scores scene: one tab per table (Easy, Medium, Hard, Overall), the 10 rows of the active
// table, a per-difficulty stats footer, and Reset (behind a confirmation modal) and Back buttons.
#pragma once

#include <stdint.h>

#include "../../services/HighScores.h"
#include "../../ui/Widgets.h"
#include "../Scene.h"

namespace checkers::app {

// Paints the header row and the 10 rows of one ScoreTable in a single pass.
class ScoresTable : public ui::Widget {
 public:
  void init(int16_t x, int16_t y, int16_t w, int16_t h);
  void setSource(const HighScores* scores) { scores_ = scores; }
  void setTable(ScoreTable table);
  void paint(LGFX& gfx) override;

 private:
  const HighScores* scores_ = nullptr;
  ScoreTable table_ = ScoreTable::Easy;
};

class HighScoresScene : public Scene, public ui::RectPainter {
 public:
  HighScoresScene();

  void enter(SceneContext& ctx) override;
  void handle(const ui::TouchEvent& ev, SceneContext& ctx) override;
  void render(SceneContext& ctx) override;
  void paintRect(LGFX& gfx, const ui::Rect& r) override;

 private:
  // Button context: the scene and which tab the button selects.
  struct TabRef {
    HighScoresScene* scene;
    int index;
  };

  static void onTab(void* ctx);
  static void onReset(void* ctx);
  static void onBack(void* ctx);
  static void onConfirm(void* ctx, int button);

  void selectTab(int index);
  void refreshFooter();

  SceneContext* ctx_ = nullptr;
  int activeTab_ = 0;

  ui::WidgetGroup group_;
  ui::Label title_;
  TabRef tabRefs_[4] = {};
  ui::Button tabs_[4];
  ui::Button reset_;
  ui::Button back_;
  ScoresTable table_;
  ui::Label played_;
  ui::Label won_;
  ui::Modal confirm_;
};

}  // namespace checkers::app
