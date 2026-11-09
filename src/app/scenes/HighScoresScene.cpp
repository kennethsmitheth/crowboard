#include "HighScoresScene.h"

#include <stdio.h>

#include "../../ui/Theme.h"
#include "../Nav.h"

namespace checkers::app {
namespace {

// Column positions inside the table, relative to its left edge.
constexpr int kColRank = 24;
constexpr int kColInitials = 120;
constexpr int kColScoreRight = 440;
constexpr int kColMovesRight = 640;
constexpr int kHeaderH = 24;
constexpr int kRowH = 22;
constexpr uint16_t kStripe = ui::rgb565(42, 37, 34);

constexpr const char* kTabNames[4] = {"Easy", "Medium", "Hard", "Overall"};
constexpr ScoreTable kTabTables[4] = {ScoreTable::Easy, ScoreTable::Medium, ScoreTable::Hard,
                                      ScoreTable::Overall};

}  // namespace

// ---- ScoresTable ------------------------------------------------------------------------------

void ScoresTable::init(int16_t x, int16_t y, int16_t w, int16_t h) {
  setBounds(ui::Rect{x, y, w, h});
}

void ScoresTable::setTable(ScoreTable table) {
  if (table == table_) return;
  table_ = table;
  invalidate();
}

void ScoresTable::paint(LGFX& gfx) {
  const int x = bounds.x;
  const int y = bounds.y;

  gfx.fillRect(x, y, bounds.w, bounds.h, ui::kHudBg);

  gfx.setFont(&fonts::FreeSansBold12pt7b);
  gfx.setTextColor(ui::kHudTextMuted, ui::kHudBg);
  gfx.setTextDatum(textdatum_t::middle_left);
  gfx.drawString("#", x + kColRank, y + kHeaderH / 2);
  gfx.drawString("Initials", x + kColInitials, y + kHeaderH / 2);
  gfx.setTextDatum(textdatum_t::middle_right);
  gfx.drawString("Score", x + kColScoreRight, y + kHeaderH / 2);
  gfx.drawString("Moves", x + kColMovesRight, y + kHeaderH / 2);
  gfx.drawFastHLine(x, y + kHeaderH - 1, bounds.w, ui::kButtonBorder);

  gfx.setFont(&fonts::FreeSans12pt7b);
  for (int i = 0; i < kScoreEntries; ++i) {
    const int rowY = y + kHeaderH + i * kRowH;
    const int midY = rowY + kRowH / 2;
    const uint16_t bg = (i % 2 == 0) ? ui::kHudBg : kStripe;
    gfx.fillRect(x, rowY, bounds.w, kRowH, bg);

    // Empty slots (score 0, as HighScores::count() defines them) show "---".
    const ScoreEntry* e = scores_ ? &scores_->entry(table_, i) : nullptr;
    const bool empty = !e || e->score == 0;
    char rank[8];
    char score[16];
    char moves[12];
    snprintf(rank, sizeof rank, "%d.", i + 1);
    if (empty) {
      snprintf(score, sizeof score, "---");
      snprintf(moves, sizeof moves, "---");
    } else {
      snprintf(score, sizeof score, "%lu", static_cast<unsigned long>(e->score));
      snprintf(moves, sizeof moves, "%u", static_cast<unsigned>(e->moves));
    }

    gfx.setTextColor(empty ? ui::kHudTextMuted : ui::kHudText, bg);
    gfx.setTextDatum(textdatum_t::middle_left);
    gfx.drawString(rank, x + kColRank, midY);
    if (empty) {
      gfx.drawString("---", x + kColInitials, midY);
    } else {
      gfx.drawString(e->initials, x + kColInitials, midY);
    }
    gfx.setTextDatum(textdatum_t::middle_right);
    gfx.drawString(score, x + kColScoreRight, midY);
    gfx.drawString(moves, x + kColMovesRight, midY);
  }
}

// ---- HighScoresScene --------------------------------------------------------------------------

HighScoresScene::HighScoresScene() {
  title_.init(16, 8, 400, 56, "High Scores", ui::Label::Size::Title, ui::Label::Align::Left);

  for (int i = 0; i < 4; ++i) {
    tabRefs_[i] = TabRef{this, i};
    tabs_[i].init(16 + i * (180 + 16), 72, 180, ui::kButtonH, kTabNames[i], onTab, &tabRefs_[i]);
  }
  reset_.init(440, 0, 176, ui::kButtonH, "Reset", onReset, this);
  back_.init(624, 0, 160, ui::kButtonH, "Back", onBack, this);

  // Table: header row plus 10 rows.
  table_.init(16, 144, 768, kHeaderH + kScoreEntries * kRowH);
  played_.init(16, 396, 768, 22, "", ui::Label::Size::Body, ui::Label::Align::Left);
  won_.init(16, 418, 768, 22, "", ui::Label::Size::Body, ui::Label::Align::Left);

  confirm_.init(160, 120, 480, 240, onConfirm, this);

  group_.add(&title_);
  for (auto& t : tabs_) group_.add(&t);
  group_.add(&reset_);
  group_.add(&back_);
  group_.add(&table_);
  group_.add(&played_);
  group_.add(&won_);
  group_.add(&confirm_);  // last: on top
}

void HighScoresScene::enter(SceneContext& ctx) {
  ctx_ = &ctx;
  table_.setSource(&ctx.highScores);
  selectTab(0);
  ctx.renderer.setCellPainter(nullptr);
  ctx.renderer.setRectPainter(&group_);
  group_.invalidateAll();
  refreshFooter();
  group_.collectDirty(ctx.renderer.dirty());
}

void HighScoresScene::handle(const ui::TouchEvent& ev, SceneContext& ctx) {
  ctx_ = &ctx;
  group_.dispatch(ev);
  group_.collectDirty(ctx.renderer.dirty());
}

void HighScoresScene::render(SceneContext& ctx) {
  group_.collectDirty(ctx.renderer.dirty());
}

void HighScoresScene::paintRect(LGFX& gfx, const ui::Rect& r) {
  group_.paintRect(gfx, r);
}

// Disables the active tab's button (it shows as selected) and repaints only the table.
void HighScoresScene::selectTab(int index) {
  activeTab_ = index;
  for (int i = 0; i < 4; ++i) tabs_[i].setEnabled(i != activeTab_);
  table_.setTable(kTabTables[activeTab_]);
}

void HighScoresScene::refreshFooter() {
  if (!ctx_) return;
  const StatsData& s = ctx_->stats.data();
  char line[48];
  snprintf(line, sizeof line, "Played  Easy %u   Medium %u   Hard %u",
           static_cast<unsigned>(s.byDifficulty[0].played),
           static_cast<unsigned>(s.byDifficulty[1].played),
           static_cast<unsigned>(s.byDifficulty[2].played));
  played_.setText(line);
  snprintf(line, sizeof line, "Won  Easy %u   Medium %u   Hard %u   Streak %u",
           static_cast<unsigned>(s.byDifficulty[0].won),
           static_cast<unsigned>(s.byDifficulty[1].won),
           static_cast<unsigned>(s.byDifficulty[2].won), static_cast<unsigned>(s.bestStreak));
  won_.setText(line);
}

void HighScoresScene::onTab(void* ctx) {
  const auto* ref = static_cast<const TabRef*>(ctx);
  ref->scene->selectTab(ref->index);
}

void HighScoresScene::onReset(void* ctx) {
  auto* s = static_cast<HighScoresScene*>(ctx);
  s->confirm_.show("Reset all scores and stats?",
                   "This clears every table and all stats.\nThis cannot be undone.", "Cancel",
                   "Reset");
}

void HighScoresScene::onBack(void* ctx) {
  auto* s = static_cast<HighScoresScene*>(ctx);
  if (s->ctx_) s->ctx_->nav.go(SceneId::MainMenu);
}

void HighScoresScene::onConfirm(void* ctx, int button) {
  auto* s = static_cast<HighScoresScene*>(ctx);
  if (button != 1 || !s->ctx_) return;  // 0 = Cancel
  SceneContext& c = *s->ctx_;
  c.highScores.reset();
  c.highScores.save();
  c.stats.reset();
  c.stats.save();
  s->refreshFooter();
  s->group_.invalidateAll();
}

}  // namespace checkers::app
