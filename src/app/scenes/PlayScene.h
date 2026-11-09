// Play scene (issue #35): the board, the HUD, human input through PlayLogic, the AI on core 0,
// undo, clocks, autosave after every move and result detection.
//
// Configuration comes from ctx.config (GameConfig). A resolved move (human or AI) first slides
// on the pre-move board (state Animating, the Game is not updated yet); when the slide ends the
// move is applied to the Game and onMoveApplied() updates the view and starts the cosmetic
// capture fade / promotion pulse. When the game is over the final position stays up for 1.5 s
// (FinishedPause), then summary() describes it for the Game Over scene (#36); if that scene is
// not registered the result stays on the board and is shown in the HUD status line.
#pragma once

#include <stdint.h>

#include "../Scene.h"
#include "../GameConfig.h"
#include "../PlayLogic.h"
#include "../../ui/Animator.h"
#include "../../ui/BoardView.h"
#include "../../ui/Hud.h"

#pragma push_macro("bit")
#undef bit
#include "../../game/Game.h"
#pragma pop_macro("bit")

namespace checkers::app {

class PlayScene : public Scene {
 public:
  enum class State : uint8_t {
    AwaitSelection,
    Selected,
    AiThinking,
    Animating,
    FinishedPause,
    Finished
  };

  // What the Game Over scene needs once state() == Finished.
  struct Summary {
    Result result = Result::Ongoing;
    Reason reason = Reason::None;
    GameConfig config;          // effective configuration (from the save when resumed)
    bool forcedCapture = true;  // the rule the game was played under (not today's setting)
    Board finalBoard = Board::initial();
    uint16_t humanMoves = 0;  // vs computer: moves by the human; two players: all moves
    uint32_t elapsedSec = 0;  // both clocks together
    uint8_t hintsUsed = 0;    // hints shown during this game (saturates at 255; no score effect)
  };

  PlayScene(ui::Renderer& renderer, ui::Sprites& sprites);

  void enter(SceneContext& ctx) override;
  void exit(SceneContext& ctx) override;
  void handle(const ui::TouchEvent& ev, SceneContext& ctx) override;
  void update(uint32_t nowMs, uint32_t dtMs, SceneContext& ctx) override;
  void render(SceneContext&) override {}  // views mark their own dirty cells and rects

  State state() const { return state_; }
  const Game& game() const { return game_; }
  const Summary& summary() const { return summary_; }

  // The single hook after a move was applied to game_ (human or AI), once its slide ended.
  // `moved` is the piece that stood on m.from before the move. Syncs the view to the new board,
  // starts the fade / pulse overlays (they do not block input) and runs afterBoardChange().
  void onMoveApplied(const Move& m, Piece moved, SceneContext& ctx);

 private:
  void startNewGame(SceneContext& ctx);
  bool vsComputer() const { return cfg_.mode == GameConfig::Mode::VsComputer; }
  bool isHumanTurn() const;
  bool humanHasMoved() const;
  void applyMove(const Move& m, SceneContext& ctx);  // starts the slide
  void completeMove(SceneContext& ctx);              // slide ended: apply + onMoveApplied
  void applyPendingNow(SceneContext& ctx);           // cancel the animation, apply without it
  bool isFinished() const { return state_ == State::FinishedPause || state_ == State::Finished; }
  void afterBoardChange(SceneContext& ctx);  // autosave, HUD, end-of-game check
  void finish(SceneContext& ctx);
  void requestAi(SceneContext& ctx);
  void pollAi(SceneContext& ctx);
  void cancelAi(SceneContext& ctx);
  bool canHint() const;
  void onHint(SceneContext& ctx);          // submits a 300 ms search of this board
  void onHintResult(const AiResult& res);  // shows the suggestion if still valid
  void cancelHint(SceneContext& ctx);      // drops a pending request and the highlight
  void dropHintHighlight() { hintFrom_ = hintTo_ = 0; }
  void onBoardTap(int square, SceneContext& ctx);
  void onUndo(SceneContext& ctx);
  void onMenu(SceneContext& ctx);
  void autosave(SceneContext& ctx);
  void updateFlip(SceneContext& ctx);  // two-player flip: side to move at the bottom
  void refreshView();
  void refreshHud(bool force = false);  // setModel() only when a value changed (or forced)
  void setStatus(const char* s);

  ui::BoardView view_;
  ui::Hud hud_;
  ui::Animator animator_;
  Game game_;
  GameConfig cfg_;
  State state_ = State::AwaitSelection;
  PlaySelection sel_;
  uint32_t selTargets_ = 0;
  uint32_t clockMs_[2] = {0, 0};  // indexed by sideIndex(Side)
  uint32_t aiSeq_ = 0;            // move request ids (1.., within kAiIdSeqMask)
  uint32_t hintWaiting_ = 0;      // requestId of the Hint search in flight, 0 = none
  uint32_t hintSeq_ = 0;          // low bits of hint request ids (ids carry kAiIdHint)
  uint32_t hintShownMs_ = 0;      // when the current hint highlight appeared
  int hintFrom_ = 0;              // suggested move, 1-32 notation; 0 = no highlight
  int hintTo_ = 0;
  uint8_t hintsUsed_ = 0;
  Move pending_;  // the move being animated (state Animating)
  Piece pendingPiece_ = Piece::None;
  uint32_t nowMs_ = 0;  // time of the last update()
  uint32_t pauseStartMs_ = 0;
  uint32_t aiWaiting_ = 0;  // requestId we wait for, 0 = none
  bool active_ = false;     // a game is loaded (enter after a pop keeps it)
  bool flipped_ = false;    // board drawn rotated 180 degrees (two-player flipBoard setting)
  char status_[32] = "";
  Summary summary_;
};

}  // namespace checkers::app
