#include "Renderer.h"

#include <Arduino.h>

#include "../hal/Scanout.h"

namespace checkers::ui {

using hal::Scanout;

bool Renderer::begin() {
  const bool beam = Scanout::begin(gfx_);
  sync_ = beam ? Sync::Beam : Sync::None;
  return beam;
}

Renderer::Sync Renderer::sync() const {
  return (sync_ == Sync::Beam && Scanout::hasBeam()) ? Sync::Beam : Sync::None;
}

bool Renderer::waitVsync(uint32_t timeoutUs) {
  return Scanout::waitVsync(timeoutUs);
}

uint32_t Renderer::frames() {
  return Scanout::frames();
}

int Renderer::beamLine() {
  return Scanout::beamLine();
}

int Renderer::collectBands(Band* out) const {
  // Marks without a painter (cells in a widget-only scene, rects when no HUD is installed) are
  // dropped here rather than turned into empty bands that would still wait for the beam.
  int n = 0;
  for (int row = 0; cellPainter_ && row < kBoardCells; ++row) {
    const uint8_t mask = dirty_.rowMask(row);
    if (!mask) continue;
    const int cells = __builtin_popcount(mask);
    out[n++] = Band{DirtyRects::cellY(row), static_cast<int16_t>(DirtyRects::cellY(row) + kCell),
                    static_cast<uint16_t>(kBaseLines + kCellLines * cells), BandKind::Cells,
                    static_cast<uint8_t>(row)};
  }
  for (int i = 0; rectPainter_ && i < dirty_.rectCount(); ++i) {
    const Rect& r = dirty_.rect(i);
    // Same cost density as cells: kCellLines per 60x60 of area.
    const int32_t lines = kBaseLines + (r.area() * kCellLines) / (kCell * kCell);
    out[n++] = Band{r.y, r.bottom(), static_cast<uint16_t>(lines > 480 ? 480 : lines),
                    BandKind::Rect, static_cast<uint8_t>(i)};
  }
  // Insertion sort by top edge; n is at most kMaxBands (16).
  for (int i = 1; i < n; ++i) {
    const Band b = out[i];
    int j = i - 1;
    while (j >= 0 && out[j].y0 > b.y0) {
      out[j + 1] = out[j];
      --j;
    }
    out[j + 1] = b;
  }
  return n;
}

void Renderer::paintBand(const Band& b) {
  gfx_.startWrite();
  if (b.kind == BandKind::Cells) {
    if (cellPainter_) {
      const int row = b.index;
      const uint8_t mask = dirty_.rowMask(row);
      for (int col = 0; col < kBoardCells; ++col) {
        if (!(mask & (1u << col))) continue;
        const int16_t x = DirtyRects::cellX(col);
        const int16_t y = DirtyRects::cellY(row);
        gfx_.setClipRect(x, y, kCell, kCell);
        cellPainter_->paintCell(gfx_, row, col, x, y);
      }
    }
  } else if (rectPainter_) {
    const Rect& r = dirty_.rect(b.index);
    gfx_.setClipRect(r.x, r.y, r.w, r.h);
    rectPainter_->paintRect(gfx_, r);
  }
  gfx_.clearClipRect();
  gfx_.endWrite();  // one cache write-back for the band
}

bool Renderer::flush() {
  if (!dirty_.any()) return false;

  const uint32_t t0 = micros();
  Band bands[kMaxBands];
  const int n = collectBands(bands);
  const bool beamSync = sync() == Sync::Beam;

  // Start with the bands the beam has already passed this frame (safe for almost a whole frame),
  // then chase it downwards through the rest.
  int first = 0;
  if (beamSync) {
    const int beam = Scanout::beamLine();
    while (first < n && bands[first].y1 + Scanout::kLeadLines <= beam) ++first;
  }

  uint32_t waitUs = 0;
  for (int k = 0; k < n; ++k) {
    const Band& b = bands[(first + k) % n];
    // A band the beam can never be clear of (the full sidebar, a full-screen rectangle) would
    // only wait out the timeout and then be painted anyway; paint it at once instead.
    if (beamSync && Scanout::beamClearPossible(b.y0, b.y1, b.opLines)) {
      waitUs += Scanout::waitBeamClear(b.y0, b.y1, b.opLines, kWaitTimeoutUs);
    }
    paintBand(b);
  }

  if (overlay_) {
    gfx_.startWrite();
    overlay_->paintOverlay(gfx_);
    gfx_.endWrite();
  }

  const uint32_t total = micros() - t0;
  stats_.flushes++;
  stats_.lastUs = total;
  stats_.lastWaitUs = waitUs;
  stats_.lastDrawUs = total - waitUs;
  stats_.lastCells = dirty_.cellCount();
  stats_.lastRects = dirty_.rectCount();
  if (total > stats_.worstUs) stats_.worstUs = total;
  if (waitUs > stats_.worstWaitUs) stats_.worstWaitUs = waitUs;
  if (stats_.lastDrawUs > stats_.worstDrawUs) stats_.worstDrawUs = stats_.lastDrawUs;
  stats_.sumUs += total;
  stats_.sumWaitUs += waitUs;
  stats_.sumDrawUs += stats_.lastDrawUs;
  stats_.sumCells += stats_.lastCells;

  dirty_.clear();
  return true;
}

}  // namespace checkers::ui
