# Rendering notes: dirty-rect renderer on the RGB frame buffer (issue #20)

Measurements taken on the CrowPanel ESP32 7.0" (ESP32-S3-WROOM-1-N4R8, octal PSRAM at 80 MHz,
LovyanGFX 1.2.36 `Panel_RGB` + `Bus_RGB`, Arduino core 3.3 / ESP-IDF 5.5) with the `bench` and
`anim` serial commands in `src/ui/RenderBench.cpp`. Numbers are min/avg/max of 5 runs unless
noted; the firmware prints them as microseconds.

## 1. How pixels get to the panel

- `Bus_RGB` does **not** use ESP-IDF's `esp_lcd_rgb_panel` driver. It creates a dummy i80 bus for
  pin setup, allocates one 768 000-byte RGB565 frame buffer in PSRAM, builds its own circular GDMA
  descriptor chain (191 descriptors of 4032 bytes) and programs the LCD_CAM registers directly.
  The panel is scanned continuously; there is no second buffer and no "present" step.
- The LCD_CAM `VSYNC_END` interrupt is handled by `Bus_RGB::lcd_default_isr_handler`, which only
  resets and restarts the DMA chain. The user callback is commented out in the library, so
  **LovyanGFX exposes no vsync event** and `esp_lcd_rgb_panel_register_event_callbacks` cannot be
  used (there is no esp_lcd RGB panel handle).
- Drawing goes through the data cache. `Panel_FrameBufferBase` writes into the cached PSRAM
  lines, tracks the modified bounding box, and the pixels only reach PSRAM (and thus the panel)
  when `display()` runs `esp_cache_msync` over that box. `display()` is called automatically by
  the outermost `endWrite()` (`_auto_display`), so each top-level LovyanGFX primitive does its own
  write-back, and a batch wrapped in `startWrite()/endWrite()` is written back once. Cache
  evictions can leak lines earlier when the working set exceeds the 32 KB data cache (the whole
  board, 460 KB, does; one cell, 7.2 KB, does not).

## 2. Panel timing (from `crowpanel7_lgfx.h`, confirmed on hardware)

| pixel clock | line (928 pclk) | frame (525 lines) | refresh | active region | blanking |
|---|---|---|---|---|---|
| 15 MHz (default) | 61.87 µs | 32.48 ms | 30.8 Hz | 29.70 ms | 45 lines = 2.78 ms |
| 20 MHz | 46.40 µs | 24.36 ms | 41.1 Hz | 22.27 ms | 2.09 ms |
| 10 MHz | 92.80 µs | 48.72 ms | 20.5 Hz | 44.54 ms | 4.18 ms |

The vsync hook measured the frame period as exactly 32 480 µs at 15 MHz (24 359 at 20, 48 720 at
10), so the timing model is exact and the panel runs at 30.8 Hz, not 60.

## 3. Scan-out tracking (`src/hal/Scanout.*`)

Two things turned out to be readable:

1. **Vsync.** A second *shared* handler on `ETS_LCD_CAM_INTR_SOURCE`, gated on the same
   `LCD_LL_EVENT_VSYNC_END` status bit (`esp_intr_alloc_intrstatus`). ESP-IDF prepends new
   shared handlers, so ours runs before LovyanGFX's, sees the status bit, and only records a
   timestamp and a frame counter; LovyanGFX's handler then clears the bit and restarts the DMA.
   This works on the current core and library version; `Scanout::begin()` logs "vsync hooked".
2. **Beam position.** The GDMA out channel's `dscr` register holds the address of the descriptor
   being fetched; its `buffer` pointer minus the frame buffer base, divided by 1600 bytes, is the
   line the DMA is reading. Granularity is one descriptor (2.5 lines). Measured trace at 1 ms
   steps after the ISR (15 MHz):

   `5 5 5 7 25 40 57 73 88 105 120 138 153 168 186 201 219 234 252 267 282 299 315 332 347 362
   380 395 413 428 446 461 476 5 5 5`

   So the interrupt fires at the **end of the active region**; the DMA then idles at "line 5"
   (two descriptors prefetched into the FIFOs) for the 45-line blanking, measured as 2.87 ms from
   the ISR to the first advance (2.15 ms at 20 MHz, 4.31 ms at 10 MHz), and then advances at the
   expected 16.2 lines/ms. The DMA readout therefore runs **up to ~6 lines ahead** of the pixel
   beam; `Scanout::kLeadLines = 8` is the margin used when deciding that the beam has passed a
   band.

Costs of waiting (15 MHz): `waitVsync()` from a random moment 10.1 / 14.6 / 21.3 ms (so pacing
*everything* on vsync wastes half a frame on average); `waitBeamClear()` for a one-row band from
a random moment 0.001 / 0.25 / 4.6 ms, waiting at all in 2-4 of 20 tries. GPIO 40 (VSYNC) was not
polled: it is an output driven by the peripheral and the interrupt is cleaner.

## 4. Redraw cost (15 MHz, DMA running)

### (a) Whole 480x480 board

| what | min | avg | max |
|---|---|---|---|
| 64 MockScreen-style cells (square + piece where the start position has one) via `Renderer::flush`, no sync | 22.8 ms | **23.2 ms** | 23.7 ms |
| same with `Sync::Beam` (chasing the beam): total | 31.9 | 32.9 | 34.2 |
| of which waiting | 8.9 | 9.8 | 11.1 |
| 64 `fillRect` in one `startWrite/endWrite` | 17.9 | 18.5 | 19.0 |
| of which `endWrite` (write-back of the 480x480 box) | 0.53 | 0.67 | 0.70 |
| 64 `fillRect`, each written back | 22.5 | 22.8 | 23.3 |
| one `fillRect` 480x480 | 17.8 | 18.2 | 18.9 |

A 480x480 fill is 460 800 bytes in 18.2 ms = 25 MB/s: full redraws are bound by PSRAM write
bandwidth, not by LovyanGFX. Target "< 40 ms" is met with margin either way.

### (b) One 60x60 cell straight into the frame buffer (dark square + piece, 5 primitives)

| what | min | avg | max |
|---|---|---|---|
| draw into cache (inside `startWrite`) | 203 µs | 227 µs | 318 µs |
| `endWrite` write-back (60 rows, ~94 KB address range) | 154 | 195 | 208 |
| same 5 primitives, each written back (no batching) | 596 | 685 | 712 |
| `fillRect` 60x60 incl. write-back | 240 | 258 | 265 |
| one dirty cell through `Renderer::flush`, no sync | 408 | **414** | 430 |

Target "< 2 ms" is met five times over. Batching matters: 0.41 ms versus 0.69 ms unbatched, and
batching is also what makes the commit a single burst (see section 6).

### (c) Composite the cell in a 60x60 `LGFX_Sprite`, then `pushSprite`

| what | internal RAM sprite | PSRAM sprite |
|---|---|---|
| composite square + piece into the sprite | 161 µs | 211 µs |
| `pushSprite` copy into the frame buffer cache | 41 | 38 |
| `endWrite` write-back | 208 | 210 |
| `pushSprite` incl. write-back (top-level call) | 235 | 242 |
| **total per cell (composite + push)** | **~0.40 ms** | **~0.45 ms** |

No faster than drawing directly (0.41 ms), because the write-back dominates both paths and
`pushSprite` into the cached frame buffer is already a 40 µs memcpy. The useful number here is
that pushing a *pre-rendered* 60x60 sprite costs 0.04 ms plus the 0.2 ms write-back, versus 0.23 ms
for drawing four circles: pre-rendered piece/cell sprites (ARCHITECTURE section 7) cut the paint
time in half but do not change the commit cost.

### (e) Memory bandwidth and the pixel clock

| | 10 MHz | 15 MHz (default) | 20 MHz |
|---|---|---|---|
| DMA load on PSRAM (768 000 B x refresh) | 15.8 MB/s | 23.6 MB/s | 31.5 MB/s |
| `memset` 460 800 B into PSRAM | 30 MB/s | 27 MB/s | 24 MB/s |
| `memcpy` internal -> PSRAM | 29 | 26 | 23 |
| `memcpy` PSRAM -> internal | 49 | 45 | 40 |
| `memcpy` internal -> internal | 378 | 378 | 378 |
| full board, 64 mock cells, no sync | 21.2 ms | 23.2 ms | 25.5 ms |
| full board, beam sync total / wait | 47.4 / 26.2 ms | 32.9 / 9.8 ms | 27.7 / 2.2 ms |
| one cell through `Renderer::flush` | 0.40 ms | 0.41 ms | 0.43 ms |

PSRAM contention is real but modest: every 8 MB/s of extra DMA load costs the CPU about 3 MB/s
of write bandwidth (10 % of a full redraw between 15 and 20 MHz). CPU writes to PSRAM top out
around 30 MB/s even with little contention, so that is the ceiling for large fills.
**Keep 15 MHz**: it is the vendor value, the panel is known to be stable there, and 41 Hz would
buy smoother motion only at the price of slower drawing and an unverified panel mode. The
`CROWPANEL_PCLK_HZ` build flag stays for future experiments.

## 5. Test animation (`anim`)

A 48 px two-tone disc (amber rim, gold centre) slides around the rim of the board at 240 px/s
through the dirty-cell path (old and new bounding boxes marked with `markCellsUnder`, then
`flush()`), about 1.9 cells per frame. Three modes, 6 s each:

| mode | pacing | sync | frames | flush avg / worst | draw avg / worst | wait avg / worst |
|---|---|---|---|---|---|---|
| `beam` | one flush per vsync | `Sync::Beam` | 30.7 fps | 2.98 / 10.8 ms | 0.66 / 0.96 ms | 2.3 / 9.9 ms |
| `vsync` | one flush per vsync | none | 30.7 fps | 0.63 / 0.81 ms | same | 0 |
| `raw` | 30 ms timer (33.3 fps, drifts against the 30.8 Hz scan) | none | 33.3 fps | 0.71 / 0.96 ms | same | 0 |

The `beam` waits are large here only because the test starts every flush right at vsync, when
the beam is about to scan rows 0-1: a top-row band must wait for the beam to pass it (2.9 ms
blanking + 3.7 ms per row). From a random moment the wait averages 0.25 ms (section 3).

**What to look for on the panel** (the screenshot tool reads the frame buffer and cannot show
tearing; a human has to watch, or film at 240 fps):

- `anim raw 10`: the write moment sweeps through the frame, so the disc should show a horizontal
  shear (the top and bottom halves offset by ~8 px) a few times per second, at varying places
  along its path. This is the baseline that proves tearing is visible at all on this panel.
- `anim beam 10`: expected clean. Any shear here means the beam model is wrong; first suspects are
  `Scanout::kLeadLines` (raise it) and `Renderer::kCellLines` (raise it).
- `anim vsync 10`: expected clean too, because 1.9 cells draw in 0.6 ms, well inside the 2.87 ms
  blanking. It would tear on the top rows once a flush takes longer than the blanking (about 7
  cells), which is why it is not the chosen strategy.

Both `anim` and `bench` block the main loop while they run (the diag line stops; touch is not
polled); `render` prints the renderer's statistics at any time.

## 6. Decision

**Direct dirty-cell redraws, batched per row band, with beam-synced flushes.**

- *Direct, not sprite-composited.* Compositing a cell in an internal-RAM sprite buys nothing: the
  write-back cache already turns a batched cell into one burst, and the sprite path is not faster
  (section 4c). Pre-rendered piece sprites remain worthwhile for anti-aliased pieces and halve
  paint time, but that is a BoardView concern, not a tearing fix.
- *Batched per band.* `Renderer::flush()` wraps each board row (and each HUD rectangle) in
  `startWrite()/endWrite()`, so a band's pixels reach PSRAM in one `esp_cache_msync` right after
  it is painted. One band of 8 heavy cells takes about 2.9 ms; a typical animation band 0.4-1 ms.
- *Beam-synced, not vsync-aligned.* Before painting a band the renderer waits until the DMA
  readout says the beam is either already below the band (with an 8-line margin) or far enough
  above it that the band will be committed before the beam arrives (`kBaseLines + kCellLines x
  cells`, 12 lines = 0.74 ms per cell, against a measured 0.41 ms). Bands the beam has passed are
  painted first, the rest in top-to-bottom order, chasing the beam. Cost: a single band waits
  0.25 ms on average and at most about one row of scan time (4.6 ms measured); a full board takes
  one frame (33 ms) instead of 23 ms but never tears. Pure vsync alignment was rejected because it
  wastes half a frame of waiting on average and only protects flushes shorter than the blanking.
- *Vsync is still used* for the frame counter and `Renderer::waitVsync()`, which an animator may
  use for pacing but need not: a free-running loop at <= 16 ms with beam-synced flushes shows one
  update per refresh anyway.
- Full-screen or full-board repaints simply mark everything (`redrawAll()`); if a scene would
  rather have 23 ms than 33 ms it can `setSync(Sync::None)` for that flush and accept one frame
  with a visible wipe.

Pending human confirmation on the panel with `anim raw` versus `anim beam` (section 5).

## 7. API for BoardView, Hud and Animator

```cpp
#include "ui/Renderer.h"       // Renderer, CellPainter, RectPainter, RenderStats
#include "ui/DirtyRects.h"     // DirtyRects, Rect (header-only, host-testable)

checkers::ui::Renderer renderer(gfx);   // one per display, owned by main/app
renderer.begin();                       // after gfx.init(); hooks Scanout, enables Sync::Beam

struct BoardView : checkers::ui::CellPainter {
  void paintCell(LGFX& gfx, int row, int col, int16_t x, int16_t y) override;
  // Paint the whole 60x60 cell: background, piece, overlays. Clip rect is already the cell,
  // startWrite() is active, never call startWrite/endWrite here.
};
struct Hud : checkers::ui::RectPainter {
  void paintRect(LGFX& gfx, const checkers::ui::Rect& r) override;
  // Repaint whatever of yours intersects r; clip to your own regions, r may be the full screen.
};
renderer.setCellPainter(&boardView);
renderer.setRectPainter(&hud);

auto& d = renderer.dirty();
d.markSquare(sq);                 // playable square 1..32
d.markCell(row, col);             // any of the 64 cells
d.markCellsUnder(x, y, w, h);     // cells under a sprite's bounding box (clipped to the board)
d.markRow(row); d.markCol(col);
d.markRect(x, y, w, h);           // HUD region (8 slots; merges, never drops)
d.markAll();                      // all cells + full-screen rect
renderer.flush();                 // paints dirty cells/rects, clears marks, returns true if drawn
renderer.redrawAll();             // markAll + flush
renderer.stats();                 // flushes, last/worst/avg us, draw vs wait split, cells, rects
renderer.setSync(Renderer::Sync::None);   // opt out of beam waits for one scene if ever needed
Renderer::waitVsync(); Renderer::frames(); Renderer::beamLine();   // pacing helpers
```

Blocking bounds of `flush()` with `Sync::Beam`: about 0.4 ms per cell of painting plus beam waits
of 0.25 ms average, 5 ms worst for one band, 10 ms if two top bands both have to let the beam
pass, 33 ms for a whole board.

Serial commands: `bench` (all experiments above), `anim [beam|vsync|raw] [seconds]`, `render`.
Build-time pixel clock override: `PLATFORMIO_BUILD_FLAGS="-DCROWPANEL_PCLK_HZ=20000000" pio run
-e crowpanel7 -t upload`.
