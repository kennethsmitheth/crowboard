#include "PlayScene.h"

#include <stdio.h>
#include <string.h>

#include "../Nav.h"
#include "../../ui/BoardGeometry.h"

namespace checkers::app {

namespace {

constexpr uint32_t kFinalPauseMs = 1500;  // final position stays up before Game Over
constexpr uint32_t kMaxDtMs = 250;        // a stalled frame must not jump the clock
constexpr uint32_t kHintBudgetMs = 300;   // Hint search budget (a quick look, not the AI's turn)
constexpr uint32_t kHintShowMs = 2000;    // how long a suggested move stays highlighted
// Hint request ids live in their own range so a hint result can never be taken for the
// computer's move (move ids count up from 1 and stay far below this).
constexpr uint32_t kHintIdBase = 0x80000000u;
constexpr uint32_t kHintSeqMask = 0x7FFFFFFFu;

// Copies up to 3 initials, dropping trailing spaces ("P1 " -> "P1").
void copyInitials(char (&dst)[4], const char* src) {
  int n = 0;
  for (; n < 3 && src[n] != '\0'; ++n) dst[n] = src[n];
  while (n > 0 && dst[n - 1] == ' ') --n;
  dst[n] = '\0';
}

bool sameState(const ui::BoardViewState& a, const ui::BoardViewState& b) {
  if (a.selected != b.selected || a.legalTargets != b.legalTargets ||
      a.forcedPieces != b.forcedPieces || a.lastFrom != b.lastFrom || a.lastTo != b.lastTo ||
      a.pendingLen != b.pendingLen || a.flipped != b.flipped || a.hintFrom != b.hintFrom ||
      a.hintTo != b.hintTo) {
    return false;
  }
  return memcmp(a.pendingPath, b.pendingPath, a.pendingLen) == 0;
}

bool sameModel(const ui::HudModel& a, const ui::HudModel& b) {
  const char* sa = a.status ? a.status : "";
  const char* sb = b.status ? b.status : "";
  return strcmp(a.p1Initials, b.p1Initials) == 0 && strcmp(a.p2Initials, b.p2Initials) == 0 &&
         a.p1Count == b.p1Count && a.p2Count == b.p2Count && a.p1Captured == b.p1Captured &&
         a.p2Captured == b.p2Captured && a.toMove == b.toMove && a.clockP1Sec == b.clockP1Sec &&
         a.clockP2Sec == b.clockP2Sec && a.showButtons == b.showButtons &&
         a.undoEnabled == b.undoEnabled && a.hintEnabled == b.hintEnabled && strcmp(sa, sb) == 0;
}

int popcount(uint32_t x) {
  return __builtin_popcount(x);
}

// Side that played ply i. Games always start from the initial position, Black to move.
Side sideOfPly(uint16_t i) {
  return (i & 1) ? Side::White : Side::Black;
}

}  // namespace

PlayScene::PlayScene(ui::Renderer& renderer, ui::Sprites& sprites)
    : view_(renderer, sprites), hud_(renderer), animator_(renderer, sprites, view_) {}

// ---- lifecycle --------------------------------------------------------------------------------

void PlayScene::enter(SceneContext& ctx) {
  ctx.renderer.setCellPainter(&view_);
  ctx.renderer.setRectPainter(&hud_);
  ctx.renderer.setOverlay(&animator_);  // another scene may have replaced it

  if (!active_) {
    // Fresh entry (not a pop back from an overlay): start or resume a game.
    cfg_ = ctx.config;
    bool resumed = false;
    if (cfg_.resume) {
      SaveHeader h{};
      if (ctx.saveGame.load(game_, h) && !game_.isOver()) {
        cfg_.mode = h.mode == 1 ? GameConfig::Mode::TwoPlayers : GameConfig::Mode::VsComputer;
        cfg_.difficulty =
            h.difficulty <= 2 ? static_cast<Difficulty>(h.difficulty) : Difficulty::Medium;
        cfg_.humanSide = h.humanSide == 1 ? Side::White : Side::Black;
        clockMs_[0] = h.p1ClockSec * 1000u;
        clockMs_[1] = h.p2ClockSec * 1000u;
        resumed = true;
      }
    }
    cfg_.resume = resumed;
    if (!resumed) startNewGame(ctx);
    sel_.clear();
    selTargets_ = 0;
    aiWaiting_ = 0;
    animator_.cancel();
    state_ = State::AwaitSelection;
    summary_ = Summary{};
    summary_.config = cfg_;
    active_ = true;
  }

  view_.setBoard(&game_.board());
  refreshView();
  refreshHud(true);
  view_.invalidateAll();
  hud_.invalidateAll();
}

void PlayScene::exit(SceneContext& ctx) {
  animator_.cancel();
  cancelHint(ctx);
  cancelAi(ctx);
  active_ = false;
}

void PlayScene::startNewGame(SceneContext& ctx) {
  game_.reset(Rules{ctx.settings.data().forcedCapture});
  clockMs_[0] = clockMs_[1] = 0;
  hintsUsed_ = 0;
}

// ---- input ------------------------------------------------------------------------------------

void PlayScene::handle(const ui::TouchEvent& ev, SceneContext& ctx) {
  if (ev.type != ui::TouchType::Tap) return;
  // Any tap clears a shown hint. The tap that clears it on the Hint button does not ask again.
  const bool hintShown = hintFrom_ != 0;
  if (hintShown) {
    dropHintHighlight();
    refreshView();
  }
  ui::HudButton b;
  if (hud_.hitButton(ev.x, ev.y, b)) {
    if (b == ui::HudButton::Undo) onUndo(ctx);
    if (b == ui::HudButton::Menu) onMenu(ctx);
    if (b == ui::HudButton::Hint && !hintShown) onHint(ctx);
    return;
  }
  if (ev.x < ui::kBoardSize && ev.y < ui::kBoardSize) onBoardTap(ui::cellAt(ev.x, ev.y), ctx);
}

void PlayScene::onBoardTap(int square, SceneContext& ctx) {
  if (state_ != State::AwaitSelection && state_ != State::Selected) return;
  if (!isHumanTurn()) return;
  const PlayTapResult r = playTap(game_.legalMoves(), sel_, square);
  if (r.resolved) {
    cancelHint(ctx);  // a move makes a pending or shown hint stale
    sel_.clear();
    selTargets_ = 0;
    state_ = State::AwaitSelection;
    applyMove(r.move, ctx);
    return;
  }
  sel_ = r.sel;
  selTargets_ = r.targets;
  state_ = sel_.active() ? State::Selected : State::AwaitSelection;
  refreshView();
}

void PlayScene::onUndo(SceneContext& ctx) {
  if (state_ == State::Animating) applyPendingNow(ctx);  // then undo it like any other move
  if (isFinished() || !game_.canUndo()) return;
  if (vsComputer()) {
    if (!humanHasMoved()) return;
    cancelAi(ctx);
    // Back to the human's turn before their last move: one ply while the AI thinks, else two.
    while (game_.undo() && game_.sideToMove() != cfg_.humanSide) {
    }
  } else {
    game_.undo();
  }
  sel_.clear();
  selTargets_ = 0;
  state_ = State::AwaitSelection;
  view_.setBoard(&game_.board());
  afterBoardChange(ctx);
}

void PlayScene::onMenu(SceneContext& ctx) {
  animator_.cancel();
  if (state_ == State::Animating) state_ = State::AwaitSelection;  // pending move is dropped
  cancelHint(ctx);
  cancelAi(ctx);
  if (!game_.isOver()) autosave(ctx);
  if (ctx.nav.go(SceneId::MainMenu)) return;
  // No main menu yet (#33): Menu starts a new game so the panel stays playable.
  ctx.saveGame.erase();
  startNewGame(ctx);
  sel_.clear();
  selTargets_ = 0;
  state_ = State::AwaitSelection;
  summary_ = Summary{};
  summary_.config = cfg_;
  view_.setBoard(&game_.board());
  refreshView();
  refreshHud();
}

// ---- per frame --------------------------------------------------------------------------------

void PlayScene::update(uint32_t nowMs, uint32_t dtMs, SceneContext& ctx) {
  if (!active_) return;
  nowMs_ = nowMs;
  if (!isFinished() && !game_.isOver()) {
    clockMs_[sideIndex(game_.sideToMove())] += dtMs > kMaxDtMs ? kMaxDtMs : dtMs;
  }
  animator_.update(nowMs);  // marks dirty cells; the manager flushes after this
  if (hintFrom_ != 0 && nowMs - hintShownMs_ >= kHintShowMs) {
    dropHintHighlight();
    refreshView();
  }
  if (state_ == State::Animating && !animator_.sliding()) completeMove(ctx);
  // An AI result arriving during the slide stays in the worker until the slide ends.
  if (state_ != State::Animating) pollAi(ctx);
  if (state_ == State::AiThinking || state_ == State::AwaitSelection || state_ == State::Selected) {
    if (!isHumanTurn() && aiWaiting_ == 0) requestAi(ctx);
  }
  if (state_ == State::FinishedPause && nowMs - pauseStartMs_ >= kFinalPauseMs) {
    state_ = State::Finished;
    refreshView();
    // Without a Game Over scene the result stays on the board and Menu returns.
    ctx.nav.go(SceneId::GameOver);
    if (!active_) return;
  }
  refreshHud();  // only calls setModel() when a value changed (e.g. the clock second)
}

void PlayScene::requestAi(SceneContext& ctx) {
  const uint32_t id = ++aiSeq_ == 0 ? ++aiSeq_ : aiSeq_;
  const AiRequest req =
      makeRequest(game_.board(), cfg_.difficulty, id, game_.rules().forcedCapture);
  if (!ctx.ai.request(req)) {
    // Worker unavailable (begin() failed): play the first legal move so the game goes on.
    const MoveList& ml = game_.legalMoves();
    if (!ml.empty()) applyMove(ml[0], ctx);
    return;
  }
  aiWaiting_ = id;
  state_ = State::AiThinking;
  sel_.clear();
  selTargets_ = 0;
  refreshView();
  refreshHud();
}

void PlayScene::pollAi(SceneContext& ctx) {
  AiResult res;
  while (ctx.ai.poll(res)) {
    if (hintWaiting_ != 0 && res.requestId == hintWaiting_) {
      hintWaiting_ = 0;
      onHintResult(res);
      continue;
    }
    if (aiWaiting_ == 0 || res.requestId != aiWaiting_) continue;  // stale
    aiWaiting_ = 0;
    if (isFinished()) continue;
    state_ = State::AwaitSelection;
    if (game_.isOver() || isHumanTurn()) continue;
    Move m = res.move;
    bool legal = false;
    for (const Move& lm : game_.legalMoves()) legal = legal || lm == m;
    if (!legal) {
      if (game_.legalMoves().empty()) continue;
      m = game_.legalMoves()[0];
    }
    applyMove(m, ctx);
  }
}

void PlayScene::cancelAi(SceneContext& ctx) {
  if (aiWaiting_ != 0) ctx.ai.cancel();
  aiWaiting_ = 0;  // its result arrives later and is dropped as stale
  if (state_ == State::AiThinking) state_ = State::AwaitSelection;
}

// ---- hint -------------------------------------------------------------------------------------

bool PlayScene::canHint() const {
  return !isFinished() && !game_.isOver() &&
         (state_ == State::AwaitSelection || state_ == State::Selected) && isHumanTurn() &&
         aiWaiting_ == 0 && hintWaiting_ == 0;
}

void PlayScene::onHint(SceneContext& ctx) {
  if (!canHint()) return;
  hintSeq_ = (hintSeq_ + 1) & kHintSeqMask;
  if (hintSeq_ == 0) hintSeq_ = 1;
  const uint32_t id = kHintIdBase | hintSeq_;
  AiRequest req = makeRequest(game_.board(), cfg_.difficulty, id, game_.rules().forcedCapture);
  req.budgetMs = kHintBudgetMs;
  req.useTT = false;
  req.randomTopN = 0;                // the suggestion is the search's best move, not a random pick
  if (!ctx.ai.request(req)) return;  // worker unavailable: no hint this time
  hintWaiting_ = id;
  refreshHud();
}

void PlayScene::onHintResult(const AiResult& res) {
  // Dropped when the human has moved, the game ended or the computer is on move meanwhile.
  if (isFinished() || !isHumanTurn() || aiWaiting_ != 0) return;
  if (state_ != State::AwaitSelection && state_ != State::Selected) return;
  if (res.move.pathLen == 0) return;  // no legal move to suggest
  for (const Move& lm : game_.legalMoves()) {
    if (!(lm == res.move)) continue;
    hintFrom_ = toNotation(lm.from);
    hintTo_ = toNotation(lm.to);
    hintShownMs_ = nowMs_;
    if (hintsUsed_ < 255) ++hintsUsed_;
    refreshView();
    return;
  }
}

void PlayScene::cancelHint(SceneContext& ctx) {
  // A hint is never in flight together with the computer's request (Hint needs the human's
  // turn), so cancelling the worker here only stops the hint search.
  if (hintWaiting_ != 0) ctx.ai.cancel();
  hintWaiting_ = 0;  // its result, when it arrives, is dropped as stale
  dropHintHighlight();
}

// ---- moves ------------------------------------------------------------------------------------

void PlayScene::applyMove(const Move& m, SceneContext& ctx) {
  // The Game stays on the pre-move board while the piece slides (origin hidden by the Animator).
  pending_ = m;
  pendingPiece_ = game_.board().pieceAt(m.from);
  sel_.clear();
  selTargets_ = 0;
  state_ = State::Animating;
  animator_.slidePiece(pendingPiece_, m, nowMs_);
  refreshView();
  refreshHud();
  if (!animator_.sliding()) completeMove(ctx);  // nothing to animate (pathLen 0)
}

void PlayScene::completeMove(SceneContext& ctx) {
  state_ = State::AwaitSelection;
  if (!game_.apply(pending_)) {
    refreshView();
    refreshHud();
    return;
  }
  onMoveApplied(pending_, pendingPiece_, ctx);
}

void PlayScene::applyPendingNow(SceneContext& ctx) {
  animator_.cancel();
  state_ = State::AwaitSelection;
  if (!game_.apply(pending_)) return;
  view_.setBoard(&game_.board());
  afterBoardChange(ctx);
}

void PlayScene::onMoveApplied(const Move& m, Piece, SceneContext& ctx) {
  // Same tick as the end of the slide, before the flush: the origin is already un-hidden, so the
  // old piece never shows for a frame. The overlays draw on the updated board and do not block
  // input; the fade covers the now-empty captured squares.
  view_.setBoard(&game_.board());
  if (m.captured != 0) animator_.fadeCapture(m.captured, nowMs_);
  if (m.promotes) animator_.promotePulse(toNotation(m.to), nowMs_);
  afterBoardChange(ctx);
}

void PlayScene::afterBoardChange(SceneContext& ctx) {
  cancelHint(ctx);  // the board changed: any hint for the old position is stale
  if (game_.isOver()) {
    finish(ctx);
    return;
  }
  autosave(ctx);
  refreshView();
  refreshHud();
}

void PlayScene::finish(SceneContext& ctx) {
  cancelAi(ctx);
  state_ = State::FinishedPause;
  pauseStartMs_ = nowMs_;
  sel_.clear();
  selTargets_ = 0;

  summary_.result = game_.result();
  summary_.reason = game_.reason();
  summary_.config = cfg_;
  summary_.finalBoard = game_.board();
  summary_.elapsedSec = (clockMs_[0] + clockMs_[1]) / 1000u;
  summary_.hintsUsed = hintsUsed_;
  uint16_t humanMoves = 0;
  for (uint16_t i = 0; i < game_.historySize(); ++i) {
    if (!vsComputer() || sideOfPly(i) == cfg_.humanSide) ++humanMoves;
  }
  summary_.humanMoves = humanMoves;

  const Result r = game_.result();
  const bool draw = r == Result::Draw;
  const Side winner = r == Result::P1Wins ? Side::Black : Side::White;
  if (vsComputer()) {
    const Outcome o =
        draw ? Outcome::Draw : (winner == cfg_.humanSide ? Outcome::Win : Outcome::Loss);
    ctx.stats.recordVsComputer(cfg_.difficulty, o, game_.historySize());
  } else {
    ctx.stats.recordTwoPlayer(game_.historySize());
  }
  ctx.stats.save();
  ctx.saveGame.erase();

  if (draw) {
    const Reason why = game_.reason();
    setStatus(why == Reason::FortyMoveRule ? "Draw: 40-move rule"
              : why == Reason::Repetition  ? "Draw: repetition"
                                           : "Draw");
  } else if (vsComputer()) {
    setStatus(winner == cfg_.humanSide ? "You win!" : "Computer wins");
  } else {
    char buf[sizeof(status_)];
    char ini[4];
    copyInitials(ini, winner == Side::Black ? cfg_.p1Initials : cfg_.p2Initials);
    snprintf(buf, sizeof(buf), "%s wins!", ini);
    setStatus(buf);
  }
  refreshView();
  refreshHud();
  // update() switches to Game Over after kFinalPauseMs.
}

void PlayScene::autosave(SceneContext& ctx) {
  if (game_.isOver()) return;
  SaveHeader h{};
  h.mode = vsComputer() ? 0 : 1;
  h.difficulty = static_cast<uint8_t>(cfg_.difficulty);
  h.humanSide = static_cast<uint8_t>(sideIndex(cfg_.humanSide));
  h.forcedCapture = game_.rules().forcedCapture ? 1 : 0;
  h.p1ClockSec = clockMs_[0] / 1000u;
  h.p2ClockSec = clockMs_[1] / 1000u;
  if (game_.historySize() == 0) {
    ctx.saveGame.erase();  // nothing worth resuming
    return;
  }
  ctx.saveGame.save(game_, h);
}

// ---- helpers ----------------------------------------------------------------------------------

bool PlayScene::isHumanTurn() const {
  return !vsComputer() || game_.sideToMove() == cfg_.humanSide;
}

bool PlayScene::humanHasMoved() const {
  // Black plays the even plies, White the odd ones.
  return game_.historySize() >= (cfg_.humanSide == Side::Black ? 1u : 2u);
}

void PlayScene::setStatus(const char* s) {
  snprintf(status_, sizeof(status_), "%s", s ? s : "");
}

void PlayScene::refreshView() {
  ui::BoardViewState s;
  s.selected = sel_.active() ? toNotation(sel_.from) : 0;
  s.legalTargets = sel_.active() ? selTargets_ : 0;
  s.hiddenSquare = view_.state().hiddenSquare;  // owned by the Animator
  const bool inputTurn = !isFinished() && state_ != State::AiThinking &&
                         state_ != State::Animating && isHumanTurn() && !game_.isOver();
  s.forcedPieces = inputTurn ? forcedPieces(game_.legalMoves()) : 0;
  if (game_.historySize() > 0) {
    const Move& last = game_.historyAt(game_.historySize() - 1);
    s.lastFrom = toNotation(last.from);
    s.lastTo = toNotation(last.to);
  }
  s.hintFrom = hintFrom_;
  s.hintTo = hintTo_;
  s.pendingLen = sel_.pathLen;
  for (int i = 0; i < sel_.pathLen && i < 10; ++i) s.pendingPath[i] = sel_.path[i];
  if (!sameState(s, view_.state())) view_.setState(s);
}

void PlayScene::refreshHud(bool force) {
  if (!isFinished()) {
    if (state_ == State::AiThinking) {
      setStatus("Computer thinking...");
    } else if (hintWaiting_ != 0) {
      setStatus("Thinking of a hint...");
    } else if (isHumanTurn() && forcedPieces(game_.legalMoves()) != 0) {
      setStatus("Capture is forced");
    } else {
      setStatus("");
    }
  }

  ui::HudModel m;
  if (vsComputer()) {
    char human[4];
    copyInitials(human, cfg_.p1Initials);
    const bool humanBlack = cfg_.humanSide == Side::Black;
    copyInitials(m.p1Initials, humanBlack ? human : "CPU");
    copyInitials(m.p2Initials, humanBlack ? "CPU" : human);
  } else {
    copyInitials(m.p1Initials, cfg_.p1Initials);
    copyInitials(m.p2Initials, cfg_.p2Initials);
  }
  const Board& b = game_.board();
  m.p1Count = static_cast<uint8_t>(popcount(b.pieces(Side::Black)));
  m.p2Count = static_cast<uint8_t>(popcount(b.pieces(Side::White)));
  m.p1Captured = static_cast<uint8_t>(game_.captured(Side::White));  // taken by P1
  m.p2Captured = static_cast<uint8_t>(game_.captured(Side::Black));
  m.toMove = game_.sideToMove();
  m.clockP1Sec = clockMs_[0] / 1000u;
  m.clockP2Sec = clockMs_[1] / 1000u;
  m.showButtons = true;
  // While animating, Undo counts the move in flight (it is applied, then undone).
  const bool anim = state_ == State::Animating;
  m.undoEnabled = !isFinished() && (game_.canUndo() || anim) &&
                  (!vsComputer() || humanHasMoved() ||
                   (anim && game_.historySize() + 1u >= (cfg_.humanSide == Side::Black ? 1u : 2u)));
  m.hintEnabled = canHint();
  m.status = status_;
  if (force || !sameModel(m, hud_.model())) hud_.setModel(m);
}

}  // namespace checkers::app
