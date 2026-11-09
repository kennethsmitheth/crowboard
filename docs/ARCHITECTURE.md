# crowboard — Checkers on the CrowPanel ESP32 7.0"

Architecture for a touch-driven game of American checkers (English draughts) running on the
Elecrow CrowPanel ESP32 7.0" HMI. This document is the reference that the GitHub issues point
at; section numbers are stable.

## 1. Goals

- A complete, polished game of American checkers: Human vs Computer (Easy / Medium / Hard) and
  two-player pass-and-play on one screen.
- High scores, settings and an in-progress game survive power cycles.
- The rules engine and the AI compile and run on the host (Mac, CI) with unit tests; no hardware
  needed to prove correctness.
- Feels responsive: touch-to-highlight under 100 ms, animations at 30 fps or better, computer
  replies within about 1 s on Easy/Medium and 3 s on Hard.

Non-goals for v1: sound, Wi-Fi/online play, drag-and-drop (tap-tap only), international draughts.

## 2. Hardware and resource budget

| Resource | Available | Plan |
|---|---|---|
| CPU | ESP32-S3, 2 × 240 MHz Xtensa LX7 | Core 1: input + UI + rendering loop. Core 0: AI search task. |
| Internal SRAM | ~345 KB heap free at boot | Stacks, game state, widgets, DMA-capable scratch. Keep > 60 KB free. |
| PSRAM | 8 MB octal | Frame buffer 768 KB (800×480×2), piece sprites (~100 KB), AI transposition table (1 MB), save/score blobs. |
| Flash | 4 MB, `partitions.csv`, no OTA | Single 3 MB factory app (hello world is 395 KB, 13%). Layout: phy_init 4 KB @0x9000, otadata 8 KB @0xe000 (only because the Arduino upload flow writes boot_app0.bin there), factory app 3 MB @0x10000, NVS 64 KB @0x310000, coredump 64 KB @0x320000, LittleFS 832 KB @0x330000 (ends at 0x400000). App stays at 0x10000 because upload hard-codes that offset. |
| Display | 800×480 RGB565, parallel RGB bus, pclk 15 MHz | Single frame buffer in PSRAM scanned continuously by the LCD peripheral at ≈31 Hz. |
| Touch | GT911 capacitive, I2C (SDA 19 / SCL 20), up to 5 points | Single-touch tap detection. |
| Backlight | PWM on GPIO 2 | Brightness setting. |
| USB | CH340 on UART0 | Flashing, logs, screenshot dump. |
| Storage | NVS partition (Preferences API) | Settings, high scores, stats, autosave. |

## 3. Layered architecture

```
┌──────────────────────────────────────────────────────────────────┐
│ app/       Scenes + SceneManager                                 │
│            Splash → MainMenu → GameSetup → Play → GameOver        │
│            HighScores, Settings                                   │
├──────────────────────────────────────────────────────────────────┤
│ ui/        Renderer (BoardView, Hud), Widgets, Animator,         │
│            Sprites, DirtyRects, Theme                             │
├───────────────────────────────┬──────────────────────────────────┤
│ game/  pure C++17, no HW      │ services/  Scoring, HighScores,  │
│ Board, MoveGen, Rules, Game,  │            Settings, SaveGame,    │
│ Eval, Search, Zobrist/TT,     │            Stats                  │
│ Notation, Difficulty          │                                  │
├───────────────────────────────┴──────────────────────────────────┤
│ hal/       Display (LovyanGFX), Touch, Storage (NVS), Clock,     │
│            Log/Diagnostics, AiWorker (FreeRTOS task on core 0)   │
├──────────────────────────────────────────────────────────────────┤
│ Arduino core 3.3 / ESP-IDF 5.5 / FreeRTOS                        │
└──────────────────────────────────────────────────────────────────┘
```

Rules:

- Dependencies point downward only. `game/` and `services/` include nothing from Arduino or
  ESP-IDF, so they build in the PlatformIO `native` environment for tests. Anything hardware-ish
  they need (time, storage) comes in through a small interface injected from `hal/`.
- `ui/` knows about `game/` types (it renders a `Board`) but never mutates game state.
- `app/` is the only layer that talks to everything.

## 4. Module responsibilities

### hal/
| Module | Responsibility |
|---|---|
| `crowpanel7_lgfx.h` | LovyanGFX device config: RGB pin map, timings, backlight, GT911 (exists today). |
| `Display` | Owns the `LGFX` instance; init, brightness, frame-buffer access for screenshots. |
| `Touch` | Polls GT911 via LovyanGFX, debounces, emits `TouchEvent {Down, Move, Up, Tap, x, y, t}`. |
| `Storage` | `IStorage` interface (namespaced blobs, commit, erase) with an NVS implementation; a RAM implementation lives in tests. |
| `Clock` | `millis()` behind an interface so animations and AI budgets are testable. |
| `Log` | Levels (E/W/I/D), compile-time filter, periodic diagnostics line: heap, min heap, PSRAM, loop time, fps. |
| `AiWorker` | FreeRTOS task pinned to core 0. Request/result queues, cancellation flag. |

### game/ (host-testable)
| Module | Responsibility |
|---|---|
| `Board` | 32-square bitboards: `uint32_t men[2], kings[2]`, side to move. Standard 1–32 numbering, neighbor tables, (row, col) mapping, initial position, position string for tests. |
| `Move` | `from, to, captured (bitmask), path[] (≤ 10 squares), promotes`. Fixed-size, no heap. |
| `MoveGen` | Legal move generation honoring forced capture and full multi-jump sequences. |
| `Rules` | Variant flags (forced capture on/off), terminal detection, draw rules. |
| `Game` | Board + history: apply/undo, ply count, halfmove clock, repetition, result and reason. |
| `Eval` | Static evaluation with a tunable weight struct. |
| `Search` | Negamax alpha-beta, iterative deepening, move ordering, time budget via `shouldStop()`. |
| `Zobrist` / `TT` | Incremental hashing and a transposition table (PSRAM on device, heap on host). |
| `Difficulty` | Easy / Medium / Hard profiles (depth caps, time budgets, randomness). |
| `Notation` | `"11-15"`, `"22x15x8"` move strings; position strings. |

### ui/
| Module | Responsibility |
|---|---|
| `Theme` | Palette, fonts, layout metrics (board 480×480 at x=0, 60 px cells, sidebar x=480 w=320). |
| `DirtyRects` | 64-bit cell mask + a short list of HUD rectangles; the renderer redraws only these. |
| `Sprites` | Pre-rendered anti-aliased piece images in PSRAM (man/king × 2 colors). |
| `BoardView` | Draws cells, pieces, selection, legal targets, last move, forced-capture markers. |
| `Hud` | Captured counts, turn indicator, clocks, buttons (Undo, Hint, Menu), player initials. |
| `Animator` | Time-based tweens: piece slide along a jump path, capture fade, promotion pulse. |
| `Widgets` | Button, Label, Toggle, ListView, Modal, initials Keypad; hit-testing; each reports its dirty rect. |

### app/
| Module | Responsibility |
|---|---|
| `Scene` | `enter()`, `exit()`, `handle(TouchEvent)`, `update(dtMs)`, `render(Renderer&)`. |
| `SceneManager` | Replace/push/pop with transitions applied between frames. |
| `scenes/*` | Splash, MainMenu, GameSetup, Play, GameOver, HighScores, Settings. |

### services/
| Module | Responsibility |
|---|---|
| `Scoring` | Pure function from game outcome to points (§11). |
| `HighScores` | Top-10 tables per difficulty plus overall; qualifies/insert/reset; one versioned blob. |
| `Settings` | Brightness, forced capture, theme, default difficulty, initials. |
| `SaveGame` | Autosave after each move; resume by replaying moves. |
| `Stats` | Games played/won/lost/drawn per difficulty, streaks. |

## 5. Rules of play (American checkers)

- 8×8 board, play on the 32 dark squares. Each side starts with 12 men on the three rows nearest
  them. Player 1 (dark pieces) moves first.
- Men move one square diagonally forward. Kings move one square diagonally in any direction
  (no flying kings).
- A capture jumps over an adjacent enemy piece to the empty square directly beyond. If a capture
  is available it must be taken (any capture, not necessarily the longest). After a jump, the same
  piece must continue jumping while it can. A piece may not be jumped twice in one sequence.
- A man reaching the far row becomes a king and the turn ends immediately, even if further jumps
  would be possible.
- A player with no pieces, or no legal move on their turn, loses.
- Draw: 40 consecutive moves by each side (80 plies) with no capture and no man moved, or the
  same position with the same side to move occurring three times, or both players agree.
- Variant flag reserved: `forcedCapture = false` for casual play (not counted for high scores).

## 6. Data model

Square numbering (standard notation; row 0 is Player 2's back rank at the top of the screen):

```
     1   2   3   4      row 0
   5   6   7   8        row 1
     9  10  11  12      row 2
  13  14  15  16        row 3
    17  18  19  20      row 4
  21  22  23  24        row 5
    25  26  27  28      row 6
  29  30  31  32        row 7
```

- `Board` is 4 × `uint32_t` + side (17 bytes); copying is trivial, so the AI works on snapshots.
- Neighbors come from a precomputed `int8_t kNeighbor[32][4]` table (NW, NE, SW, SE; −1 = off
  board). Jump target = neighbor of the neighbor in the same direction. Tables beat row-parity
  shift tricks for readability and are fast enough.
- `Move` keeps the full landing path so the UI can animate multi-jumps and so two sequences with
  the same from/to but different paths are distinct moves.
- Position string for tests and saves, e.g. `W:W18,24,27,28,K10,K15:B12,16,20,K22,K25,K29`
  (side to move, white pieces, black pieces; `K` prefix for kings).
- Save blob: game config + the move list in notation; restore replays from the initial position,
  which also validates the save.

## 7. Rendering strategy

- The LCD peripheral scans one frame buffer in PSRAM continuously. Double buffering would mean
  copying 768 KB per frame through PSRAM, which is too slow, so the renderer is incremental:
  only dirty 60×60 cells and HUD rectangles are redrawn. A cell is ~7 KB and redraws in a small
  fraction of one scan, so tearing is confined to a sliver and should be invisible in practice.
  This must be measured on hardware; if tearing shows, options are compositing each cell in an
  internal-RAM sprite and pushing it in one burst, or hooking the RGB panel's vsync event.
- Pieces are pre-rendered sprites in PSRAM, pushed with a transparent key onto the cell
  background. Rendering them once at boot keeps anti-aliasing cheap.
- Animations redraw at most the few cells under the moving sprite per frame.
- Text uses LovyanGFX's bundled fonts. LittleFS is reserved for future assets; nothing is loaded
  from flash files in v1.
- Targets: full-board redraw < 40 ms, incremental frame < 10 ms, loop ≤ 16 ms during animation.

## 8. Input

- Poll the GT911 each loop (~100 Hz). Debounce 30 ms. `Tap` = Down then Up within 12 px and
  400 ms. Drag is reserved for later.
- Board hit-testing maps screen coordinates to a cell; light squares are ignored.
- Selection rules in the Play scene: tapping an own piece with legal moves selects it (under
  forced capture only pieces with captures are selectable); tapping a highlighted target moves;
  tapping anything else deselects. For multi-jumps where several sequences share from/to but
  differ in path, the player taps each landing square in turn.

## 9. Concurrency

- Core 1 (Arduino `loop`): poll touch → scene update → animator → flush dirty rects → `delay(1)`.
- Core 0: `AiWorker` blocks on a FreeRTOS queue. A request carries a `Game` snapshot by value,
  the difficulty and a time budget; the worker runs iterative deepening until the budget expires
  or `cancel()` is called, then posts `{move, depth, nodes, requestId}`. Stale results (requestId
  mismatch) are dropped. The same path serves the Hint button.
- No shared mutable state between cores. The transposition table belongs to the worker.
- The UI loop must keep > 30 fps while the AI thinks; the diagnostics line verifies this.

## 10. Persistence (NVS)

| Namespace | Contents | Written when |
|---|---|---|
| `settings` v1 | brightness, forcedCapture, theme, firstPlayer, defaultDifficulty, p1/p2 initials | user changes a setting |
| `scores` v1 | 4 tables × 10 × {initials[3], score, moves, gameNo}, gameCounter | game over with a qualifying score |
| `save` v1 | mode, difficulty, colors, clocks, move list | after each completed move; erased at game end |
| `stats` v1 | per-difficulty counters, streaks | game over |

Every blob has a header `{magic, version, length, crc32}`. A mismatch is treated as "absent" and
logged. There is no RTC, so entries carry a game sequence number instead of a date. Writes are at
most a few hundred per game, well within NVS wear limits.

## 11. Scoring

Only Human vs Computer games with forced capture enabled count. One pure function owns the formula:

| Component | Points |
|---|---|
| Result | Win 1000 × M, Draw 200 × M, Loss not recorded (M: Easy 1, Medium 2, Hard 3) |
| Material left | +50 per own man, +100 per own king |
| Efficiency | +10 × max(0, 60 − moves played by the player) |
| Speed | +max(0, 600 − elapsed seconds) ÷ 2 |

A score qualifies if it beats the 10th entry for that difficulty or for Overall. Hints have no
score effect in v1.

## 12. Screen layout (800×480)

```
0                 480                          800
┌──────────────────┬─────────────────────────────┐ 0
│                  │  KEN  ●  x12     captured 0 │
│                  │  CPU  ○  x12     captured 0 │
│    8 × 8 board   │                             │
│    60 px cells   │  ► KEN to move     03:12    │
│                  │                             │
│                  │  [ Undo ]  [ Hint ]         │
│                  │  [ Menu ]                   │
└──────────────────┴─────────────────────────────┘ 480
```

Touch targets are at least 64 px tall. Menus use the whole screen with large buttons.

## 13. Testing strategy

- `pio test -e native` runs Unity tests for `game/` and `services/` on the host:
  - Board mapping and neighbor-table symmetry.
  - Perft from the initial position (a multi-jump counts as one move):
    depth 1–8 = 7, 49, 302, 1469, 7361, 36768, 179740, 845931.
  - Capture edge cases: promotion ends the sequence, kings looping, no double-jumping a captured
    piece, forced-capture filtering.
  - Game: win/loss/draw detection, undo restores counters.
  - Search finds forced wins in short test positions; deterministic at fixed depth.
  - Scoring table; HighScores ordering and persistence against the RAM storage.
- On device: the diagnostics line (heap, PSRAM, loop time), a screenshot tool that dumps the frame
  buffer over serial to a PNG, and a manual QA checklist for the Play scene.
- CI: GitHub Actions builds the firmware and runs the native tests on every push and PR.

## 14. Directory layout

```
src/
  main.cpp                 wiring: hal init, SceneManager, loop
  hal/      crowpanel7_lgfx.h Display Touch Storage Clock Log AiWorker
  game/     Board Move MoveGen Rules Game Eval Search Zobrist TT Difficulty Notation
  ui/       Theme DirtyRects Sprites BoardView Hud Animator Widgets
  app/      Scene SceneManager scenes/
  services/ Scoring HighScores Settings SaveGame Stats
test/
  test_game/      native unit tests for game/
  test_services/  native unit tests for services/
tools/
  screenshot.py   frame-buffer dump to PNG
docs/
  ARCHITECTURE.md (this file), rendering-notes.md, qa-report.md
```

## 15. Milestones

| Milestone | Outcome |
|---|---|
| M0 Infrastructure | Source tree, native test env, CI, partition table, logging, screenshot tool. |
| M1 Rules engine | Board, move generation, game state, notation, perft-verified. |
| M2 AI opponent | Eval, alpha-beta, TT, core-0 worker, difficulty profiles. |
| M3 Rendering and input | Theme, dirty-rect renderer, sprites, board view, widgets, touch, animation. |
| M4 Persistence and scoring | Storage, scoring, high scores, settings, autosave, stats. |
| M5 Scenes | Scene framework and all screens; a full game is playable. |
| M6 Integration and polish | Hint, two-player polish, robustness, hardware QA pass, docs. |

## 16. Model assignment policy

Each issue carries one `model:*` label naming the least expensive Claude model expected to
complete it reliably. Escalate one tier if an attempt stalls.

| Label | Use for |
|---|---|
| `model:haiku-5.5` | Well-specified, mechanical tasks confined to one module with a clear acceptance check (constants, scaffolding, simple screens, docs, CI yaml). |
| `model:sonnet-5.5` | Standard feature work across a few modules following patterns that already exist in the repo; moderate debugging. |
| `model:opus-5.5` | Algorithmic correctness or performance-critical code where subtle bugs are costly (capture generation, search, transposition table) and architecture-defining modules others build on. |
| `model:fable-5.1` | Cross-cutting hardware-level debugging with uncertain root causes (frame-buffer timing on the RGB panel) and the final whole-system integration pass. |
