// Scan-out tracking for the RGB panel: where the LCD peripheral is in the frame right now.
//
// LovyanGFX's Bus_RGB feeds the panel from one PSRAM frame buffer through a GDMA channel and
// owns the LCD_CAM vsync interrupt (it restarts the DMA chain there); it exposes no vsync
// callback. This helper adds a second, observe-only handler on the same shared interrupt (frame
// counter and timestamp) and reads the GDMA channel's current descriptor to find the line being
// sent. Both are optional: every query degrades to "unknown" and every wait returns at once when
// the hook is unavailable. Measurements and the reasoning are in docs/rendering-notes.md.
#pragma once

#include <stdint.h>

class LGFX;

namespace checkers::hal {

class Scanout {
 public:
  static constexpr int kLines = 480;  // active lines
  // The DMA fetch runs ahead of the pixel output: measured, the readout idles at line 5 during
  // the vertical blanking (two 4032-byte descriptors buffered) and has 2.5-line granularity, so
  // the pixel beam is up to ~6 lines above beamLine(). 8 is the margin used for "has passed".
  static constexpr int kLeadLines = 8;

  // Call once after gfx.init(). Returns true when the scan position is readable.
  static bool begin(LGFX& gfx);

  static bool hasBeam();   // beamLine() works
  static bool hasVsync();  // frames()/waitVsync() work

  static uint32_t frames();      // vsync count since begin()
  static int64_t lastVsyncUs();  // esp_timer time of the last vsync, 0 before the first
  static uint32_t periodUs();    // interval between the last two vsyncs, 0 until known
  static uint32_t lineNs();      // scan time of one line from the panel timing config

  // Line (0..479) the scan-out DMA is fetching, or -1 if unknown. Reads ~5 during the vertical
  // blanking, when the DMA has stalled on the first chunks of the next frame.
  static int beamLine();

  // Blocks until the next vsync or the timeout; returns true on vsync.
  static bool waitVsync(uint32_t timeoutUs);

  // True when writing lines [y0, y1) over the next opLines worth of scan time cannot collide
  // with the beam: the beam is already below y1, or far enough above y0. An unknown beam (-1)
  // counts as clear.
  static bool beamClear(int beam, int y0, int y1, int opLines) {
    if (beam < 0) return true;
    return beam + opLines < y0 || beam - kLeadLines >= y1;
  }

  // True when some beam position satisfies beamClear(): the band ends at least kLeadLines above
  // the bottom (the beam passes it), or starts more than opLines + kLeadLines below the top (the
  // beam idles near line 5 during the blanking, kLeadLines is the margin). A band for which this
  // is false, such as a full-height rectangle, can only be painted without waiting.
  static constexpr bool beamClearPossible(int y0, int y1, int opLines) {
    return y1 + kLeadLines <= kLines || y0 > opLines + kLeadLines;
  }

  // Spins until beamClear() holds or the timeout expires. Returns the microseconds waited.
  static uint32_t waitBeamClear(int y0, int y1, int opLines, uint32_t timeoutUs);
};

}  // namespace checkers::hal
