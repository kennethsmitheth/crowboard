// Incremental renderer: repaints only the dirty board cells and HUD rectangles in the RGB frame
// buffer, timed against the panel's scan-out so a region is never written while the beam is
// inside it. Measurements and the reasoning behind the defaults: docs/rendering-notes.md.
//
// Usage (BoardView / Hud / Animator):
//   renderer.setCellPainter(&boardView);     // paints one 60x60 cell on request
//   renderer.setRectPainter(&hud);           // paints what intersects a HUD rectangle
//   renderer.dirty().markSquare(sq);         // ... or markCell / markCellsUnder / markRect
//   renderer.flush();                        // once per loop iteration; no-op when nothing is
//   dirty
//
// flush() groups the dirty cells into row bands (one band per board row, one per rectangle),
// wraps each band in startWrite()/endWrite() so its pixels reach PSRAM in one cache write-back,
// and with Sync::Beam waits before each band until the beam is clear of it. Bands the beam has
// already passed are painted first, then the rest from top to bottom, chasing the beam.
//
// Measured on the panel (15 MHz pixel clock, 30.8 Hz): one cell paints in about 0.4 ms including
// its write-back; a whole board in 23 ms. With Sync::Beam a flush blocks for the painting plus
// beam waits of 0.25 ms on average, about 5 ms worst for one band, 10 ms when two top bands both
// have to let the beam pass, and one frame (33 ms) for a whole board, which then never tears.
#pragma once

#include <stdint.h>

#include "../hal/crowpanel7_lgfx.h"
#include "DirtyRects.h"

namespace checkers::ui {

struct CellPainter {
  virtual ~CellPainter() = default;
  // Repaint the whole cell (row, col) whose top-left screen pixel is (x, y): background, piece,
  // overlays. The clip rectangle is set to the cell and startWrite() is active; do not call
  // startWrite()/endWrite() yourself.
  virtual void paintCell(LGFX& gfx, int row, int col, int16_t x, int16_t y) = 0;
};

struct RectPainter {
  virtual ~RectPainter() = default;
  // Repaint everything of yours that intersects r. After markAll() r is the whole screen, so
  // clip to your own regions (the board area belongs to the cell painter). The clip rectangle
  // is set to r and startWrite() is active.
  virtual void paintRect(LGFX& gfx, const Rect& r) = 0;
};

struct OverlayPainter {
  virtual ~OverlayPainter() = default;
  // Draws on top of whatever the cell and rect painters just repainted. Called once per flush
  // that painted something, after all bands, with startWrite() active and no clip rectangle. It
  // may run again with unchanged state in a later flush, so draw idempotently, and only inside
  // areas that were marked dirty (anything else would not be restored by the next flush).
  virtual void paintOverlay(LGFX& gfx) = 0;
};

struct RenderStats {
  uint32_t flushes = 0;
  uint32_t lastUs = 0;  // whole flush, waits included
  uint32_t worstUs = 0;
  uint32_t lastDrawUs = 0;  // painting plus cache write-back only
  uint32_t worstDrawUs = 0;
  uint32_t lastWaitUs = 0;  // time spent waiting for the beam
  uint32_t worstWaitUs = 0;
  uint32_t lastCells = 0;
  uint32_t lastRects = 0;
  uint64_t sumUs = 0;
  uint64_t sumDrawUs = 0;
  uint64_t sumWaitUs = 0;
  uint64_t sumCells = 0;

  uint32_t avgUs() const { return flushes ? static_cast<uint32_t>(sumUs / flushes) : 0; }
  uint32_t avgDrawUs() const { return flushes ? static_cast<uint32_t>(sumDrawUs / flushes) : 0; }
  uint32_t avgWaitUs() const { return flushes ? static_cast<uint32_t>(sumWaitUs / flushes) : 0; }
};

class Renderer {
 public:
  enum class Sync : uint8_t {
    None,  // paint immediately
    Beam,  // wait per band until the beam is clear of it (default when the scan-out is readable)
  };

  // Scan lines a painter is assumed to need per cell when deciding whether the beam is far
  // enough above a band (about 0.75 ms at 61.9 us per line; measured cells take under 0.4 ms).
  static constexpr int kCellLines = 12;
  static constexpr int kBaseLines = 8;               // per band: write-back and bookkeeping
  static constexpr uint32_t kWaitTimeoutUs = 40000;  // never block longer than ~one frame

  explicit Renderer(LGFX& gfx) : gfx_(gfx) {}

  // Hooks the scan-out tracker (call after gfx.init()). Returns true when beam sync is available
  // and selects it; otherwise flushes paint immediately.
  bool begin();

  DirtyRects& dirty() { return dirty_; }
  const DirtyRects& dirty() const { return dirty_; }
  LGFX& gfx() { return gfx_; }

  void setCellPainter(CellPainter* p) { cellPainter_ = p; }
  void setRectPainter(RectPainter* p) { rectPainter_ = p; }
  void setOverlay(OverlayPainter* p) { overlay_ = p; }

  // Beam sync silently degrades to None when the scan-out is not readable.
  void setSync(Sync s) { sync_ = s; }
  Sync sync() const;

  // Paints everything marked dirty and clears the marks. Returns true when something was drawn.
  bool flush();

  // Marks everything dirty and flushes.
  void redrawAll() {
    dirty_.markAll();
    flush();
  }

  const RenderStats& stats() const { return stats_; }
  void resetStats() { stats_ = RenderStats(); }

  // Panel refresh helpers for animators (one update per refresh). waitVsync returns false when
  // no vsync hook is available or the timeout expires.
  static bool waitVsync(uint32_t timeoutUs = kWaitTimeoutUs);
  static uint32_t frames();
  static int beamLine();

 private:
  enum class BandKind : uint8_t { Cells, Rect };
  struct Band {
    int16_t y0, y1;  // screen lines [y0, y1)
    uint16_t opLines;
    BandKind kind;
    uint8_t index;  // board row or rect index
  };
  static constexpr int kMaxBands = kBoardCells + DirtyRects::kMaxRects;

  int collectBands(Band* out) const;
  void paintBand(const Band& b);

  LGFX& gfx_;
  DirtyRects dirty_;
  CellPainter* cellPainter_ = nullptr;
  RectPainter* rectPainter_ = nullptr;
  OverlayPainter* overlay_ = nullptr;
  Sync sync_ = Sync::None;
  RenderStats stats_;
};

}  // namespace checkers::ui
