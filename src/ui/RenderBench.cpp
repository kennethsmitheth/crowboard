#include "RenderBench.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <cstring>

#pragma push_macro("bit")
#undef bit
#include "../game/MoveGen.h"
#pragma pop_macro("bit")

#include "../hal/Log.h"
#include "../hal/Scanout.h"
#include "../hal/Watchdog.h"  // feedWatchdog(): bench/anim block the loop (#41)
#include "Theme.h"

namespace checkers::ui {

using hal::Scanout;

namespace {

constexpr int kReps = 5;

// min / avg / max of a handful of microsecond samples.
struct Series {
  uint32_t min = UINT32_MAX;
  uint32_t max = 0;
  uint64_t sum = 0;
  uint32_t n = 0;

  void add(uint32_t v) {
    hal::feedWatchdog();  // one sample per bench step: keeps the 10 s task WDT fed
    if (v < min) min = v;
    if (v > max) max = v;
    sum += v;
    n++;
  }
  uint32_t avg() const { return n ? static_cast<uint32_t>(sum / n) : 0; }
  void print(const char* label) const {
    LOG_I("bench %-44s min %6lu avg %6lu max %6lu us", label, (unsigned long)(n ? min : 0),
          (unsigned long)avg(), (unsigned long)max);
  }
};

constexpr bool isPlayable(int row, int col) {
  return ((row + col) & 1) == 1;
}

uint32_t mbPerSec(uint64_t bytes, uint32_t us) {
  return us ? static_cast<uint32_t>(bytes / us) : 0;  // bytes/us == MB/s
}

}  // namespace

// ---- BenchCellPainter ---------------------------------------------------------------------------

Rect BenchCellPainter::discBounds(int16_t cx, int16_t cy) {
  return Rect{static_cast<int16_t>(cx - kPieceR), static_cast<int16_t>(cy - kPieceR),
              static_cast<int16_t>(2 * kPieceR), static_cast<int16_t>(2 * kPieceR)};
}

void BenchCellPainter::drawPiece(lgfx::LovyanGFX& g, int cx, int cy, bool darkSide, bool king) {
  const uint16_t rim = darkSide ? kP1Rim : kP2Rim;
  const uint16_t shadow = darkSide ? kP1Shadow : kP2Shadow;
  const uint16_t body = darkSide ? kP1Body : kP2Body;
  const uint16_t gloss = darkSide ? kP1Highlight : kP2Highlight;
  const int bodyR = kPieceR - kPieceRimW;
  g.fillCircle(cx, cy, kPieceR, rim);
  g.fillCircle(cx, cy, bodyR, shadow);
  g.fillCircle(cx, cy, bodyR - 2, body);
  g.fillCircle(cx - 7, cy - 7, 4, gloss);
  if (king) {
    g.fillRect(cx - 10, cy - 2, 20, 8, kCrown);
    g.fillTriangle(cx - 10, cy - 2, cx - 6, cy - 10, cx - 2, cy - 2, kCrown);
    g.fillTriangle(cx - 2, cy - 2, cx, cy - 12, cx + 2, cy - 2, kCrown);
    g.fillTriangle(cx + 2, cy - 2, cx + 6, cy - 10, cx + 10, cy - 2, kCrown);
  }
}

void BenchCellPainter::paintStatic(lgfx::LovyanGFX& g, int row, int col, int16_t x, int16_t y) {
  const bool dark = isPlayable(row, col);
  g.fillRect(x, y, kCell, kCell, dark ? kSquareDark : kSquareLight);
  if (dark && (row <= 2 || row >= 5)) {
    drawPiece(g, x + kCell / 2, y + kCell / 2, row >= 5, row == 5 && col == 4);
  }
}

void BenchCellPainter::paintCell(LGFX& gfx, int row, int col, int16_t x, int16_t y) {
  paintStatic(gfx, row, col, x, y);
  if (disc_ && discBounds(cx_, cy_).intersects(Rect{x, y, kCell, kCell})) {
    gfx.fillCircle(cx_, cy_, kPieceR, kSelect);
    gfx.fillCircle(cx_, cy_, kPieceR - 4, kCrown);
  }
}

// ---- RenderBench --------------------------------------------------------------------------------

bool RenderBench::handleCommand(const char* line) {
  if (strcmp(line, "help") == 0) {
    Serial.println(
        "          bench (timing experiments), anim [beam|vsync|raw] [seconds] (sliding disc),"
        " render (renderer stats), view 0|1|2 (board demo states), hud think (status line),"
        " widgets (touch widget demo), slide (animator demo)");
    return false;
  }
  if (strcmp(line, "bench") == 0) {
    runAll();
    return true;
  }
  if (strcmp(line, "widgets") == 0) {
    widgetsDemo_.show(renderer_);
    LOG_I("widgets demo shown; touch to try every widget");
    return true;
  }
  if (strcmp(line, "slide") == 0) {
    if (!view_ || !hud_ || !animator_) {
      LOG_W("slide: no BoardView/Animator attached");
      return true;
    }
    showView(1);
    renderer_.setCellPainter(view_);
    renderer_.setRectPainter(hud_);
    // The Animator draws through the Renderer's overlay; the Play scene installs its own, so
    // ours must be installed for the demo (and the scene's put back when it ends).
    savedOverlay_ = renderer_.overlay();
    renderer_.setOverlay(animator_);
    renderer_.redrawAll();
    // Hand-built two-jump move for the black king on 21 (not necessarily legal; visual demo).
    slideMove_ = Move{};
    slideMove_.from = static_cast<uint8_t>(fromNotation(21));
    slideMove_.path[0] = static_cast<uint8_t>(fromNotation(14));
    slideMove_.path[1] = static_cast<uint8_t>(fromNotation(7));
    slideMove_.pathLen = 2;
    slideMove_.to = slideMove_.path[1];
    slideMove_.captured = (uint32_t{1} << fromNotation(10)) | (uint32_t{1} << fromNotation(6));
    startSlideStage(1, millis());
    return true;
  }
  if (strcmp(line, "render") == 0) {
    printStats();
    return true;
  }
  if (strncmp(line, "view ", 5) == 0 || strcmp(line, "hud think") == 0) {
    if (!view_ || !hud_) {
      LOG_W("view/hud: no BoardView attached");
      return true;
    }
    if (line[0] == 'v') {
      const int n = line[5] - '0';
      if (line[6] != '\0' || n < 0 || n > 2) {
        LOG_W("view: expected 0, 1 or 2");
        return true;
      }
      showView(n);
    } else {
      thinking_ = !thinking_;
      hudModel_.status = thinking_ ? "Computer thinking..." : nullptr;
      hud_->setModel(hudModel_);
    }
    renderer_.setCellPainter(view_);
    renderer_.setRectPainter(hud_);
    renderer_.flush();
    return true;
  }
  if (strncmp(line, "anim", 4) == 0 && (line[4] == '\0' || line[4] == ' ')) {
    AnimMode mode = AnimMode::Beam;
    uint32_t seconds = 8;
    const char* p = line + 4;
    while (*p) {
      while (*p == ' ') ++p;
      if (!*p) break;
      if (*p >= '0' && *p <= '9') {
        seconds = static_cast<uint32_t>(strtoul(p, nullptr, 10));
      } else if (strncmp(p, "vsync", 5) == 0) {
        mode = AnimMode::Vsync;
      } else if (strncmp(p, "raw", 3) == 0) {
        mode = AnimMode::Raw;
      } else if (strncmp(p, "beam", 4) == 0) {
        mode = AnimMode::Beam;
      } else {
        LOG_W("anim: unknown argument '%s'", p);
        return true;
      }
      while (*p && *p != ' ') ++p;
    }
    if (seconds < 1) seconds = 1;
    if (seconds > 120) seconds = 120;
    anim(mode, seconds);
    return true;
  }
  return false;
}

void RenderBench::printStats() {
  const RenderStats& s = renderer_.stats();
  LOG_I(
      "render flushes %lu | flush avg %lu worst %lu us | draw avg %lu worst %lu us | "
      "wait avg %lu worst %lu us | last %lu us %lu cells %lu rects | sync %s | frames %lu "
      "period %lu us beam %d",
      (unsigned long)s.flushes, (unsigned long)s.avgUs(), (unsigned long)s.worstUs,
      (unsigned long)s.avgDrawUs(), (unsigned long)s.worstDrawUs, (unsigned long)s.avgWaitUs(),
      (unsigned long)s.worstWaitUs, (unsigned long)s.lastUs, (unsigned long)s.lastCells,
      (unsigned long)s.lastRects, renderer_.sync() == Renderer::Sync::Beam ? "beam" : "none",
      (unsigned long)Scanout::frames(), (unsigned long)Scanout::periodUs(), Scanout::beamLine());
}

// Demo positions for the three screenshot views. Moves and targets come from legalMoves().
void RenderBench::buildViews() {
  viewsBuilt_ = true;

  // 0: initial position, nothing highlighted.
  boards_[0] = Board::initial();

  // 1: White has just played 11-15; Black selects the man on 23 and its targets are real.
  boards_[1] = Board::initial();
  boards_[1].set(fromNotation(11), Piece::None);
  boards_[1].set(fromNotation(15), Piece::WhiteMan);
  boards_[1].set(fromNotation(21), Piece::BlackKing);  // a king for the crown
  {
    BoardViewState& s = states_[1];
    s.selected = 23;
    s.lastFrom = 11;
    s.lastTo = 15;
    MoveList moves;
    legalMoves(boards_[1], moves);
    for (const Move& m : moves) {
      if (m.from == fromNotation(s.selected)) s.legalTargets |= uint32_t{1} << m.to;
    }
  }

  // 2: Black's man just jumped 25 x 22 -> 18 and must continue over 15 to 11; a second Black
  // man on 30 also has a capture (over 26), so both carry the forced-capture ring.
  {
    Board& b = boards_[2];
    b = Board::emptyBoard();
    b.set(fromNotation(18), Piece::BlackMan);
    b.set(fromNotation(30), Piece::BlackMan);
    b.set(fromNotation(27), Piece::BlackMan);
    b.set(fromNotation(32), Piece::BlackKing);
    b.set(fromNotation(15), Piece::WhiteMan);
    b.set(fromNotation(26), Piece::WhiteMan);
    b.set(fromNotation(10), Piece::WhiteKing);
    b.set(fromNotation(1), Piece::WhiteMan);
    b.set(fromNotation(3), Piece::WhiteMan);
    b.set(fromNotation(6), Piece::WhiteMan);
    BoardViewState& s = states_[2];
    MoveList moves;
    legalMoves(b, moves);
    for (const Move& m : moves) {
      if (!m.isCapture()) continue;
      s.forcedPieces |= uint32_t{1} << m.from;
      if (m.from == fromNotation(18)) s.legalTargets |= uint32_t{1} << m.path[0];
    }
    s.pendingPath[0] = static_cast<uint8_t>(fromNotation(18));
    s.pendingLen = 1;
    s.lastFrom = 25;
    s.lastTo = 18;
  }

  std::strcpy(hudModel_.p1Initials, "KEN");
  std::strcpy(hudModel_.p2Initials, "CPU");
  hudModel_.clockP1Sec = 192;
  hudModel_.clockP2Sec = 245;
}

void RenderBench::showView(int n) {
  if (!view_ || !hud_ || n < 0 || n > 2) return;
  if (!viewsBuilt_) buildViews();
  const Board& b = boards_[n];
  hudModel_.p1Count = static_cast<uint8_t>(__builtin_popcount(b.pieces(Side::Black)));
  hudModel_.p2Count = static_cast<uint8_t>(__builtin_popcount(b.pieces(Side::White)));
  hudModel_.p1Captured = static_cast<uint8_t>(12 - hudModel_.p2Count);
  hudModel_.p2Captured = static_cast<uint8_t>(12 - hudModel_.p1Count);
  hudModel_.toMove = Side::Black;
  view_->setBoard(&b);
  view_->setState(states_[n]);
  hud_->setModel(hudModel_);
}

void RenderBench::startSlideStage(int stage, uint32_t nowMs) {
  slideStage_ = stage;
  stageStartMs_ = nowMs;
  renderer_.resetStats();
  switch (stage) {
    case 1:
      animator_->slidePiece(Piece::BlackKing, slideMove_, nowMs);
      break;
    case 2:
      animator_->fadeCapture(slideMove_.captured, nowMs);  // the white men on 10 and 6
      break;
    case 3:
      animator_->promotePulse(7, nowMs);
      break;
    default:
      break;
  }
}

void RenderBench::logSlideStage(uint32_t nowMs) {
  static const char* const kNames[] = {"", "slide 21-14-7", "capture fade", "promotion pulse"};
  const RenderStats& s = renderer_.stats();
  const uint32_t ms = nowMs - stageStartMs_;
  LOG_I("%s: %lu ms, %lu flushes (%lu fps), flush avg %lu us worst %lu us, avg cells %lu",
        kNames[slideStage_], (unsigned long)ms, (unsigned long)s.flushes,
        (unsigned long)(ms ? s.flushes * 1000UL / ms : 0), (unsigned long)s.avgUs(),
        (unsigned long)s.worstUs, (unsigned long)(s.flushes ? s.sumCells / s.flushes : 0));
}

void RenderBench::tickSlide(uint32_t nowMs) {
  if (slideStage_ == 0 || !animator_) return;
  animator_->update(nowMs);
  renderer_.flush();
  if (animator_->busy()) return;
  logSlideStage(nowMs);
  if (slideStage_ >= 3) {
    slideStage_ = 0;
    renderer_.setOverlay(savedOverlay_);
    LOG_I("slide demo done");
    return;
  }
  startSlideStage(slideStage_ + 1, nowMs);
}

void RenderBench::restoreBoard() {
  painter_.setDisc(false, 0, 0);
  renderer_.setCellPainter(&painter_);
  for (int row = 0; row < kBoardCells; ++row) renderer_.dirty().markRow(row);
  renderer_.flush();
}

void RenderBench::runAll() {
  LOG_I("bench start: pclk %lu Hz, line %lu ns, scanout beam %s vsync %s",
        (unsigned long)static_cast<lgfx::Bus_RGB*>(gfx_.getPanel()->getBus())->config().freq_write,
        (unsigned long)Scanout::lineNs(), Scanout::hasBeam() ? "yes" : "no",
        Scanout::hasVsync() ? "yes" : "no");
  const Renderer::Sync savedSync = renderer_.sync();
  renderer_.setCellPainter(&painter_);
  painter_.setDisc(false, 0, 0);

  hal::feedWatchdog();
  benchFullBoard();
  benchFillRects();
  benchCell();
  benchSprite();
  benchMemory();
  benchScanout();

  renderer_.setSync(savedSync);
  restoreBoard();
  renderer_.resetStats();
  LOG_I("bench done");
}

// (a) Whole 480x480 board: 64 mock cells through the renderer, and plain fills.
void RenderBench::benchFullBoard() {
  Series noSync, beamTotal, beamWait, beamDraw;
  renderer_.setSync(Renderer::Sync::None);
  for (int i = 0; i < kReps; ++i) {
    for (int row = 0; row < kBoardCells; ++row) renderer_.dirty().markRow(row);
    renderer_.flush();
    noSync.add(renderer_.stats().lastDrawUs);
  }
  noSync.print("full board: 64 mock cells, no sync");

  if (Scanout::hasBeam()) {
    renderer_.setSync(Renderer::Sync::Beam);
    for (int i = 0; i < kReps; ++i) {
      delay(7);  // start at varying beam positions
      for (int row = 0; row < kBoardCells; ++row) renderer_.dirty().markRow(row);
      renderer_.flush();
      beamTotal.add(renderer_.stats().lastUs);
      beamWait.add(renderer_.stats().lastWaitUs);
      beamDraw.add(renderer_.stats().lastDrawUs);
    }
    beamTotal.print("full board: 64 mock cells, beam sync total");
    beamWait.print("full board: 64 mock cells, beam sync wait");
    beamDraw.print("full board: 64 mock cells, beam sync draw");
    renderer_.setSync(Renderer::Sync::None);
  }
}

void RenderBench::benchFillRects() {
  Series batched, unbatched, single, endWrite;
  for (int i = 0; i < kReps; ++i) {
    uint32_t t0 = micros();
    gfx_.startWrite();
    for (int row = 0; row < kBoardCells; ++row) {
      for (int col = 0; col < kBoardCells; ++col) {
        gfx_.fillRect(col * kCell, row * kCell, kCell, kCell,
                      isPlayable(row, col) ? kSquareDark : kSquareLight);
      }
    }
    const uint32_t t1 = micros();
    gfx_.endWrite();
    const uint32_t t2 = micros();
    batched.add(t2 - t0);
    endWrite.add(t2 - t1);

    t0 = micros();
    for (int row = 0; row < kBoardCells; ++row) {
      for (int col = 0; col < kBoardCells; ++col) {
        gfx_.fillRect(col * kCell, row * kCell, kCell, kCell,
                      isPlayable(row, col) ? kSquareDark : kSquareLight);
      }
    }
    unbatched.add(micros() - t0);

    t0 = micros();
    gfx_.fillRect(0, 0, kBoardSize, kBoardSize, kSquareLight);
    single.add(micros() - t0);
  }
  batched.print("full board: 64 fillRect in one startWrite/endWrite");
  endWrite.print("  of which endWrite (cache write-back of 480x480)");
  unbatched.print("full board: 64 fillRect, each written back");
  single.print("full board: one fillRect 480x480");
}

// (b) One 60x60 cell drawn straight into the frame buffer.
void RenderBench::benchCell() {
  const int row = 2, col = 3;  // a dark square holding a P2 piece in the mock position
  const int16_t x = DirtyRects::cellX(col), y = DirtyRects::cellY(row);
  Series draw, writeBack, unbatched, square, viaRenderer;
  for (int i = 0; i < kReps; ++i) {
    uint32_t t0 = micros();
    gfx_.startWrite();
    gfx_.fillRect(x, y, kCell, kCell, kSquareDark);
    BenchCellPainter::drawPiece(gfx_, x + kCell / 2, y + kCell / 2, false, false);
    const uint32_t t1 = micros();
    gfx_.endWrite();
    const uint32_t t2 = micros();
    draw.add(t1 - t0);
    writeBack.add(t2 - t1);

    t0 = micros();
    gfx_.fillRect(x, y, kCell, kCell, kSquareDark);
    BenchCellPainter::drawPiece(gfx_, x + kCell / 2, y + kCell / 2, false, false);
    unbatched.add(micros() - t0);

    t0 = micros();
    gfx_.fillRect(x, y, kCell, kCell, kSquareDark);
    square.add(micros() - t0);

    renderer_.dirty().markCell(row, col);
    renderer_.flush();
    viaRenderer.add(renderer_.stats().lastDrawUs);
  }
  draw.print("cell: square + piece, draw into cache");
  writeBack.print("cell: endWrite (write-back, ~94 KB address range)");
  unbatched.print("cell: square + piece, 5 primitives each written back");
  square.print("cell: fillRect 60x60 incl. write-back");
  viaRenderer.print("cell: square + piece through Renderer::flush, no sync");
}

// (c) Composite the cell in a sprite and push it in one go.
void RenderBench::benchSprite() {
  const int row = 2, col = 3;
  const int16_t x = DirtyRects::cellX(col), y = DirtyRects::cellY(row);
  for (int pass = 0; pass < 2; ++pass) {
    const bool psram = pass == 1;
    lgfx::LGFX_Sprite spr(&gfx_);
    spr.setPsram(psram);
    spr.setColorDepth(16);
    if (!spr.createSprite(kCell, kCell)) {
      LOG_W("bench: createSprite(%s) failed", psram ? "psram" : "internal");
      continue;
    }
    Series composite, push, pushWriteBack, pushTotal;
    for (int i = 0; i < kReps; ++i) {
      uint32_t t0 = micros();
      spr.fillSprite(kSquareDark);
      BenchCellPainter::drawPiece(spr, kCell / 2, kCell / 2, true, false);
      composite.add(micros() - t0);

      t0 = micros();
      gfx_.startWrite();
      spr.pushSprite(x, y);
      const uint32_t t1 = micros();
      gfx_.endWrite();
      const uint32_t t2 = micros();
      push.add(t1 - t0);
      pushWriteBack.add(t2 - t1);

      t0 = micros();
      spr.pushSprite(x, y);
      pushTotal.add(micros() - t0);
    }
    spr.deleteSprite();
    if (psram) {
      composite.print("sprite(psram) 60x60: composite square + piece");
      push.print("sprite(psram) 60x60: pushSprite copy into cache");
      pushWriteBack.print("sprite(psram) 60x60: endWrite write-back");
      pushTotal.print("sprite(psram) 60x60: pushSprite incl. write-back");
    } else {
      composite.print("sprite(internal) 60x60: composite square + piece");
      push.print("sprite(internal) 60x60: pushSprite copy into cache");
      pushWriteBack.print("sprite(internal) 60x60: endWrite write-back");
      pushTotal.print("sprite(internal) 60x60: pushSprite incl. write-back");
    }
  }
}

// (e) Raw memory bandwidth while the LCD DMA streams the frame buffer.
void RenderBench::benchMemory() {
  constexpr size_t kBoardBytes = static_cast<size_t>(kBoardSize) * kBoardSize * 2;  // 460800
  constexpr size_t kCellBytes = static_cast<size_t>(kCell) * kCell * 2;             // 7200
  auto* psram = static_cast<uint8_t*>(heap_caps_malloc(kBoardBytes, MALLOC_CAP_SPIRAM));
  auto* internal = static_cast<uint8_t*>(heap_caps_malloc(kCellBytes * 2, MALLOC_CAP_INTERNAL));
  if (!psram || !internal) {
    LOG_W("bench: memory buffers unavailable");
    heap_caps_free(psram);
    heap_caps_free(internal);
    return;
  }
  Series psramSet, intToPsram, intToInt, psramRead;
  for (int i = 0; i < kReps; ++i) {
    uint32_t t0 = micros();
    memset(psram, i, kBoardBytes);
    psramSet.add(micros() - t0);

    t0 = micros();
    for (size_t off = 0; off + kCellBytes <= kBoardBytes; off += kCellBytes) {
      memcpy(psram + off, internal, kCellBytes);
    }
    intToPsram.add(micros() - t0);

    t0 = micros();
    for (int k = 0; k < 64; ++k) memcpy(internal + kCellBytes, internal, kCellBytes);
    intToInt.add(micros() - t0);

    t0 = micros();
    uint32_t acc = 0;
    for (size_t off = 0; off + kCellBytes <= kBoardBytes; off += kCellBytes) {
      memcpy(internal, psram + off, kCellBytes);
      acc += internal[0];
    }
    psramRead.add(micros() - t0 + (acc & 0));
  }
  LOG_I("bench memset 460800 B PSRAM          avg %6lu us = %4lu MB/s",
        (unsigned long)psramSet.avg(), (unsigned long)mbPerSec(kBoardBytes, psramSet.avg()));
  LOG_I("bench memcpy 460800 B int->PSRAM     avg %6lu us = %4lu MB/s",
        (unsigned long)intToPsram.avg(), (unsigned long)mbPerSec(kBoardBytes, intToPsram.avg()));
  LOG_I("bench memcpy 460800 B PSRAM->int     avg %6lu us = %4lu MB/s",
        (unsigned long)psramRead.avg(), (unsigned long)mbPerSec(kBoardBytes, psramRead.avg()));
  LOG_I("bench memcpy 460800 B int->int       avg %6lu us = %4lu MB/s",
        (unsigned long)intToInt.avg(), (unsigned long)mbPerSec(kBoardBytes, intToInt.avg()));
  heap_caps_free(psram);
  heap_caps_free(internal);
}

// (d) Vsync hook and beam position: sanity, timing model, cost of waiting.
void RenderBench::benchScanout() {
  if (!Scanout::hasVsync() && !Scanout::hasBeam()) {
    LOG_I("bench scanout: unavailable");
    return;
  }
  const uint32_t lineNs = Scanout::lineNs();
  if (Scanout::hasVsync()) {
    Series period;
    for (int i = 0; i < 10; ++i) {
      Scanout::waitVsync(50000);
      if (Scanout::periodUs()) period.add(Scanout::periodUs());
    }
    period.print("scanout: vsync period (expect 525 lines)");
    LOG_I("bench scanout: 525 lines x %lu ns = %lu us; frames so far %lu", (unsigned long)lineNs,
          (unsigned long)(525ULL * lineNs / 1000), (unsigned long)Scanout::frames());
  }

  if (Scanout::hasBeam() && Scanout::hasVsync()) {
    // Beam trace: sample the DMA line every millisecond after a vsync.
    int samples[36];
    Scanout::waitVsync(50000);
    const uint32_t t0 = micros();
    for (int i = 0; i < 36; ++i) {
      while (micros() - t0 < static_cast<uint32_t>(i) * 1000) {
      }
      samples[i] = Scanout::beamLine();
    }
    char buf[36 * 5 + 1];
    size_t len = 0;
    for (int i = 0; i < 36 && len < sizeof(buf) - 6; ++i) {
      len += snprintf(buf + len, sizeof(buf) - len, "%d ", samples[i]);
    }
    LOG_I("bench scanout: beam line at 1 ms steps after vsync: %s", buf);

    // Vertical blanking after the interrupt: from the vsync ISR until the DMA readout moves on
    // from the chunk it idles on (expect 45 lines: front porch 1 + pulse 31 + back porch 13).
    Series blank;
    int idle = -1;
    for (int i = 0; i < 5; ++i) {
      Scanout::waitVsync(50000);
      const uint32_t s = micros();
      idle = Scanout::beamLine();
      while (Scanout::beamLine() <= idle && micros() - s < 10000) {
      }
      blank.add(micros() - s);
    }
    blank.print("scanout: vsync ISR -> DMA advances (blanking)");
    LOG_I("bench scanout: DMA readout idles at line %d during blanking", idle);
  }

  if (Scanout::hasVsync()) {
    Series waitVs;
    for (int i = 0; i < kReps; ++i) {
      delayMicroseconds(random(32000));
      const uint32_t s = micros();
      Scanout::waitVsync(50000);
      waitVs.add(micros() - s);
    }
    waitVs.print("scanout: waitVsync from a random moment");
  }

  if (Scanout::hasBeam()) {
    Series clearWait;
    int waited = 0;
    const int y0 = DirtyRects::cellY(3), y1 = y0 + kCell;
    for (int i = 0; i < 20; ++i) {
      delayMicroseconds(random(32000));
      const uint32_t w = Scanout::waitBeamClear(y0, y1, Renderer::kBaseLines + Renderer::kCellLines,
                                                Renderer::kWaitTimeoutUs);
      clearWait.add(w);
      if (w > 50) waited++;
    }
    clearWait.print("scanout: waitBeamClear(row 3 band) random start");
    LOG_I("bench scanout: waited in %d of 20 tries", waited);
  }
}

// Sliding disc around the rim of the board through the dirty-cell path.
void RenderBench::anim(AnimMode mode, uint32_t seconds) {
  static const char* const kModeNames[] = {"beam", "vsync", "raw"};
  constexpr int kSpeedPxPerS = 240;
  constexpr int kEdge = kBoardSize - kCell;  // 420: distance between opposite cell centres
  constexpr int kLap = 4 * kEdge;            // 1680
  constexpr uint32_t kRawPeriodUs = 30000;   // deliberately off the refresh rate

  const bool vsyncPaced = mode != AnimMode::Raw && Scanout::hasVsync();
  renderer_.setCellPainter(&painter_);
  renderer_.setSync(mode == AnimMode::Beam ? Renderer::Sync::Beam : Renderer::Sync::None);
  renderer_.resetStats();
  LOG_I("anim %s for %lu s: sync %s, paced by %s, %d px/s", kModeNames[static_cast<int>(mode)],
        (unsigned long)seconds, renderer_.sync() == Renderer::Sync::Beam ? "beam" : "none",
        vsyncPaced ? "vsync" : "timer", kSpeedPxPerS);

  Series loopUs;
  bool havePrev = false;
  int16_t prevCx = 0, prevCy = 0;
  const uint32_t start = micros();
  uint32_t nextRaw = start;
  uint32_t lastLoop = 0;
  const uint32_t durationUs = seconds * 1000000UL;
  uint32_t frames = 0;

  while (true) {
    hal::feedWatchdog();
    if (vsyncPaced) {
      Scanout::waitVsync(50000);
    } else {
      while (static_cast<int32_t>(micros() - nextRaw) < 0) {
      }
      nextRaw += kRawPeriodUs;
    }
    const uint32_t now = micros();
    const uint32_t elapsed = now - start;
    if (elapsed >= durationUs) break;
    if (lastLoop) loopUs.add(now - lastLoop);
    lastLoop = now;

    const int d =
        static_cast<int>((static_cast<uint64_t>(elapsed) * kSpeedPxPerS / 1000000) % kLap);
    int16_t cx, cy;
    const int16_t c0 = kCell / 2;
    if (d < kEdge) {
      cx = c0 + d, cy = c0;
    } else if (d < 2 * kEdge) {
      cx = c0 + kEdge, cy = c0 + (d - kEdge);
    } else if (d < 3 * kEdge) {
      cx = c0 + kEdge - (d - 2 * kEdge), cy = c0 + kEdge;
    } else {
      cx = c0, cy = c0 + kEdge - (d - 3 * kEdge);
    }

    DirtyRects& dirty = renderer_.dirty();
    if (havePrev) {
      const Rect p = BenchCellPainter::discBounds(prevCx, prevCy);
      dirty.markCellsUnder(p.x, p.y, p.w, p.h);
    }
    const Rect b = BenchCellPainter::discBounds(cx, cy);
    dirty.markCellsUnder(b.x, b.y, b.w, b.h);
    painter_.setDisc(true, cx, cy);
    renderer_.flush();
    frames++;
    prevCx = cx;
    prevCy = cy;
    havePrev = true;
  }

  // Remove the disc.
  painter_.setDisc(false, 0, 0);
  if (havePrev) {
    const Rect p = BenchCellPainter::discBounds(prevCx, prevCy);
    renderer_.dirty().markCellsUnder(p.x, p.y, p.w, p.h);
    renderer_.flush();
  }

  const RenderStats& s = renderer_.stats();
  const uint32_t elapsedUs = micros() - start;
  LOG_I(
      "anim done: %lu frames in %lu ms = %lu.%lu fps | flush avg %lu worst %lu us | draw avg %lu "
      "worst %lu us | wait avg %lu worst %lu us | cells/frame %lu.%lu | loop period avg %lu "
      "worst %lu us",
      (unsigned long)frames, (unsigned long)(elapsedUs / 1000),
      (unsigned long)(static_cast<uint64_t>(frames) * 10000000ULL / elapsedUs / 10),
      (unsigned long)(static_cast<uint64_t>(frames) * 10000000ULL / elapsedUs % 10),
      (unsigned long)s.avgUs(), (unsigned long)s.worstUs, (unsigned long)s.avgDrawUs(),
      (unsigned long)s.worstDrawUs, (unsigned long)s.avgWaitUs(), (unsigned long)s.worstWaitUs,
      (unsigned long)(s.flushes ? s.sumCells * 10 / s.flushes / 10 : 0),
      (unsigned long)(s.flushes ? s.sumCells * 10 / s.flushes % 10 : 0),
      (unsigned long)loopUs.avg(), (unsigned long)loopUs.max);
  renderer_.setSync(Renderer::Sync::Beam);
}

}  // namespace checkers::ui
