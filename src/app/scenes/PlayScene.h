// Play scene (issue #35): the board, the HUD, human input through PlayLogic, the AI on core 0,
// undo, clocks, autosave after every move and result detection.
//
// Configuration comes from ctx.config (GameConfig). Moves are applied instantly; every applied
// move goes through onMoveApplied() so the Animator (#25) can be slotted in there. At the end of
// the game summary() describes it for the Game Over scene (#36); if that scene is not registered
// the result stays on the board and is shown in the HUD status line.
#pragma once

#include <stdint.h>

#include "../Scene.h"
#include "../GameConfig.h"
#include "../PlayLogic.h"
#include "../../ui/BoardView.h"
#include "../../ui/Hud.h"

#pragma push_macro("bit")
#undef bit
#include "../../game/Game.h"
#pragma pop_macro("bit")

namespace checkers::app {

class PlayScene : public Scene {
 public:
  enum class State : uint8_t { AwaitSelection, Selected, AiThinking, Finished };

  // What the Game Over scene needs once state() == Finished.
  struct Summary {
    Result result = Result::Ongoing;
    Reason reason = Reason::None;
    GameConfig config;  // effective configuration (from the save when resumed)
    Board finalBoard = Board::initial();
    uint16_t humanMoves = 0;  // vs computer: moves by the human; two players: all moves
    uint32_t elapsedSec = 0;  // both clocks together
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

  // Called right after `m` was applied to game_ (human or AI). `moved` is the piece that stood
  // on m.from before the move. Today it updates the view instantly; the Animator hooks in here.
  void onMoveApplied(const Move& m, Piece moved, SceneContext& ctx);

 private:
  void startNewGame(SceneContext& ctx);
  bool vsComputer() const { return cfg_.mode == GameConfig::Mode::VsComputer; }
  bool isHumanTurn() const;
  bool humanHasMoved() const;
  void applyMove(const Move& m, SceneContext& ctx);
  void afterBoardChange(SceneContext& ctx);  // autosave, HUD, end-of-game check
  void finish(SceneContext& ctx);
  void requestAi(SceneContext& ctx);
  void pollAi(SceneContext& ctx);
  void cancelAi(SceneContext& ctx);
  void onBoardTap(int square, SceneContext& ctx);
  void onUndo(SceneContext& ctx);
  void onMenu(SceneContext& ctx);
  void autosave(SceneContext& ctx);
  void refreshView();
  void refreshHud(bool force = false);  // setModel() only when a value changed (or forced)
  void setStatus(const char* s);

  ui::BoardView view_;
  ui::Hud hud_;
  Game game_;
  GameConfig cfg_;
  State state_ = State::AwaitSelection;
  PlaySelection sel_;
  uint32_t selTargets_ = 0;
  uint32_t clockMs_[2] = {0, 0};  // indexed by sideIndex(Side)
  uint32_t aiSeq_ = 0;
  uint32_t aiWaiting_ = 0;  // requestId we wait for, 0 = none
  bool active_ = false;     // a game is loaded (enter after a pop keeps it)
  char status_[32] = "";
  Summary summary_;
};

}  // namespace checkers::app
