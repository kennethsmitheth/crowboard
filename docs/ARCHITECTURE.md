# crowboard — Checkers on the CrowPanel ESP32 7.0"

Architecture for a touch-driven game of American checkers (English draughts) running on the
Elecrow CrowPanel ESP32 7.0" HMI. This document is the reference that the GitHub issues point
at; section numbers are stable.

Status: as built in 0.1.0 (see CHANGELOG.md). The module tables (section 4), the rendering,
input and concurrency descriptions (sections 7 to 9), persistence (section 10), testing and
layout (sections 13 and 14) describe the shipped code. Where the shipped code differs from the
original design, section 17.4 says how.

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
| Internal SRAM | ~310 KB heap free after boot (309,828 B measured in #18); static RAM of the 0.1.0 build 74,652 B (22.8 %) | Stacks, game state, widgets, blob buffers. Keep > 60 KB free. |
| PSRAM | 8 MB octal | Frame buffer 768 KB (800×480×2), piece sprites (28,800 B), AI Searcher (131 KB), AI transposition table (1 MB, used by Hard). |
| Flash | 4 MB, `partitions.csv`, no OTA | Single 3 MB factory app (0.1.0 build: 507,563 B, 16.1 % of the slot). Layout: phy_init 4 KB @0x9000, otadata 8 KB @0xe000 (only because the Arduino upload flow writes boot_app0.bin there), factory app 3 MB @0x10000, NVS 64 KB @0x310000, coredump 64 KB @0x320000, LittleFS 832 KB @0x330000 (ends at 0x400000). App stays at 0x10000 because upload hard-codes that offset. |
| Display | 800×480 RGB565, parallel RGB bus, pclk 15 MHz | Single frame buffer in PSRAM scanned continuously by the LCD peripheral at ≈31 Hz. |
| Touch | GT911 capacitive, I2C (SDA 19 / SCL 20), up to 5 points | Single-touch tap detection. |
| Backlight | PWM on GPIO 2 | Brightness setting. |
| USB | CH340 on UART0 | Flashing, logs, screenshot dump. |
| Storage | NVS partition (ESP-IDF `nvs` API via `hal/NvsStorage`) | Settings, high scores, stats, autosave. RAM fallback if NVS fails (section 17.4). |

The partition table in use is `partitions.csv` (selected by `board_build.partitions` in
`platformio.ini`; `board_upload.maximum_size` is 3,145,728 bytes):

```
# Name,    Type, SubType, Offset,   Size,     Flags
phy_init,  data, phy,     0x9000,   0x1000,
otadata,   data, ota,     0xe000,   0x2000,
factory,   app,  factory, 0x10000,  0x300000,
nvs,       data, nvs,     0x310000, 0x10000,
coredump,  data, coredump,0x320000, 0x10000,
littlefs,  data, spiffs,  0x330000, 0xD0000,
```

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
│ hal/       LGFX, Touch, NvsStorage, Scanout,                     │
│            SerialCmd, Diag, Watchdog, AiWorker (task, core 0)    │
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

### hal/ (the only layer that includes Arduino, ESP-IDF or LovyanGFX)
| Module | Responsibility |
|---|---|
| `crowpanel7_lgfx.h` | LovyanGFX device config: RGB pin map, timings, backlight, GT911. The `LGFX` instance is a static in `main.cpp`. |
| `Touch` (`checkers::hal`) | Polls the GT911 through LovyanGFX, reports present or absent, feeds the debounced tap detector (`ui/TapDetector.h`). |
| `NvsStorage` | `IStorage` on the ESP-IDF NVS API. Namespace and key names are limited to 15 characters. |
| `StorageSelector`, `ForwardingStorage` | Picks NVS, or a RAM fallback when NVS init fails (#41). `ForwardingStorage` is the swappable target that the services hold. |
| `Scanout` | Vsync hook and beam position of the RGB scan-out, used by the renderer (`docs/rendering-notes.md`). |
| `AiWorker` (`checkers::hal`) | FreeRTOS task pinned to core 0: request and result slots, cancellation, Searcher and TT in PSRAM. |
| `SerialCmd` | Line-oriented serial commands (`shot`, `heap`, `ai`, `aibench`, `help`, and the debug commands). Other lines go to a handler set by `main.cpp`. |
| `Diag` | The 5-second diagnostics line: heap, PSRAM, loop time and rate, loop stack headroom, touch and storage state. |
| `Log.h` | `LOG_E` / `LOG_W` / `LOG_I` / `LOG_D` macros, filtered at compile time by `LOG_LEVEL` (default 3). |
| `BootInfo` | Reset reason at boot, and a summary of a core dump left by a crash (erased after it is logged). |
| `Watchdog` (`checkers::hal`) | Task watchdog on the Arduino loop task (`kWatchdogTimeoutMs`, 10 s). |

### game/ (host-testable)
| Module | Responsibility |
|---|---|
| `Board` | 32-square bitboards: `men[2]`, `kings[2]`, side to move. Standard 1-32 numbering, the neighbour table `kNeighbor`, (row, col) mapping, initial position, position strings for tests and saves. |
| `Move` | `from`, `to`, `captured` (bitmask), `path[]` (at most 10 squares), path length. Fixed size, no heap. |
| `MoveList` | Fixed-capacity move list. A push when full is ignored and sets an overflow flag. |
| `MoveGen` | Legal move generation: simple moves, complete multi-jump sequences, the forced-capture filter. |
| `Apply` | Applies a move to a board. It does not check legality; the move must come from `MoveGen`. |
| `Game` | Board plus history of up to 256 plies: apply and undo, repetition and 40-move counters, result and reason, and the `Rules` struct (forced capture) it was started under. |
| `Eval` | Static evaluation with a tunable weight struct. |
| `Search` | Negamax alpha-beta, iterative deepening, move ordering, stop function polled every 1024 nodes. |
| `TT` / `Zobrist` | Transposition table (1 MB in PSRAM, used by Hard) and incremental Zobrist hashing. |
| `Difficulty` | Easy / Medium / Hard profiles (budget, depth cap, random top-N, TT use) and `AiRng`. |
| `Notation` | Move strings `"11-15"` and `"22x15x8"` in 1-32 numbering, for logs, tests and saves. |
| `AiRequest`, `AiSync` | Request and result types shared by `AiWorker` and the host stand-in `AiSync`, including the request-id ranges for each client. |

### ui/
| Module | Responsibility |
|---|---|
| `Theme`, `BoardGeometry` | Palette and layout metrics (board 480 x 480 at (0, 0), 60 px cells, sidebar at x = 480, width 320); board hit-testing. |
| `Renderer` | Incremental renderer over the RGB frame buffer. Repaints dirty cells and HUD rectangles through `CellPainter` / `RectPainter`, beam-synced per band (section 7). Has one overlay painter (the Animator). |
| `DirtyRects` | 64-cell mask plus HUD rectangles. Header-only and host-tested. |
| `Sprites` | Pre-rendered anti-aliased men and kings in PSRAM (28,800 B): two colours, plus the crown. Drawn with a transparent key. |
| `BoardView`, `BoardViewState` | Cells, pieces, selection, legal targets, last move, forced-capture rings and the hint outline. The state and its diff are plain C++. |
| `Hud` | Sidebar: two player rows, turn line, the clock of the side to move, Undo / Hint / Menu buttons, status line. |
| `Animator` | Time-based overlay: piece slide along a jump path, capture fade (200 ms), promotion pulse (400 ms). |
| `Widgets`, `WidgetCore` | Button, Label, Toggle, ListView, Modal and the initials Keypad. `WidgetCore` holds the Arduino-free hit-testing and dispatch. |
| `TapDetector`, `TouchEvent` | Debounce and tap state machine (`TapConfig`), and the touch event type (`Down`, `Move`, `Up`, `Tap`). |
| `Tween` | Easing and colour blend maths. Host-tested. |
| `RenderBench`, `WidgetsDemo` | Serial-driven experiments and the `widgets` demo (section 17.4). They are not part of the scene flow. |

### app/
| Module | Responsibility |
|---|---|
| `Scene` | Scene interface: `enter`, `exit`, `handle(TouchEvent)`, `update(nowMs, dtMs)`, `render`. `SceneContext` holds the shared services. |
| `SceneStack`, `SceneManager` | Stack (depth 4) and pending transitions. Transitions are applied at the top of a tick, then touch, update, render and flush. A transition repaints the whole screen. |
| `TouchGate` | Drops touches that straddle a transition: after one, every event is dropped until a press at least 200 ms later (F6). |
| `Nav` | `SceneId` to statically allocated scene table (section 17.1). |
| `GameConfig` | Menu choices that Play reads: mode, difficulty, human side, initials and the resume flag. |
| `PlayLogic` | Selection and multi-jump continuation state machine (section 8). |
| `GameOverText` | Pure text and outcome helpers for the Game Over scene. |
| `Version` | `kFirmwareVersion` ("0.1.0"), shown on the splash screen and in Settings > About. |
| `scenes/` | Splash (1.5 s or a tap), MainMenu, GameSetup, Play, GameOver, HighScores, Settings. |

### services/ (host-testable)
| Module | Responsibility |
|---|---|
| `Scoring` | `computeScore()`: the pure formula in section 11. |
| `HighScores` | Top-10 tables per difficulty plus Overall: `qualifies`, `insert`, `reset`. Blob `scores/v1`. |
| `Settings` | Settings data with load, save and defaults. Blob `settings/v1`. |
| `SaveGame` | Autosave after each move, and resume by replaying the moves. Blob `save/v1`. |
| `Stats` | Played, won, lost and drawn per difficulty, two-player count, streaks, total moves. Blob `stats/v1`. |
| `IStorage`, `RamStorage` | Namespaced blob interface, and a fixed-capacity RAM implementation for tests and the NVS fallback. |
| `Blob`, `Crc32` | Versioned, CRC-checked blobs on top of `IStorage` (12-byte header). |
| `GameTypes` | Enums shared by the services and app layers: `Difficulty`, `Outcome`, `Side`. |

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
- Variant flag: `forcedCapture` is a Settings toggle (default on). With it off the game is casual
  and is not counted for high scores (section 11). A game that fills the 256-ply history is a
  draw by move limit (`Reason::MoveLimit`).

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

**As built (0.1.0; measurements in `docs/rendering-notes.md`).** The renderer redraws dirty cells
directly and does not composite them in a sprite. Each row band is painted in one
`startWrite()` / `endWrite()` batch, so one cache write-back commits it. Before a band is painted,
`Renderer::flush()` waits until the scan-out beam has passed it or is far enough away to finish it
first. `Scanout::beamClearPossible()` identifies the bands that can never meet that condition
(tall bands and the full-screen rectangles); they are painted without a wait. Vsync-aligned flushes
were rejected because they waste half a frame on average. A full board takes one frame (about
33 ms); one cell takes about 0.4 ms. The Animator overlay is the exception: it is painted once
after all bands, with no beam check and its own write-back, so a moving piece can tear (#47).
The `anim` experiment is the tearing check. Its `raw` mode shows the tearing baseline; `beam` and
`vsync` are expected to be clean, and that still needs a look on the panel
(`docs/rendering-notes.md` section 6).

## 8. Input

- Poll the GT911 once per loop iteration (the loop is paced to 16 ms). Debounce 30 ms. `Tap` = Down
  then Up within 12 px and 400 ms. Drag is reserved for later. `minPressMs` and
  `maxTapsPerSecond` exist for phantom taps (#45) but default to 0 (off). After a scene change,
  touches that began before it are dropped for 200 ms (section 17.1).
- Board hit-testing maps screen coordinates to a cell; light squares are ignored.
- Selection rules in the Play scene: tapping an own piece with legal moves selects it (under
  forced capture only pieces with captures are selectable); tapping a highlighted target moves;
  tapping anything else deselects. For multi-jumps where several sequences share from/to but
  differ in path, the player taps each landing square in turn.

## 9. Concurrency

- Core 1 (Arduino `loop`): feed the watchdog, diagnostics, serial commands, poll touch, then
  `SceneManager::tick()` (transition, touch dispatch, update, render, flush). The loop sleeps out
  the rest of a 16 ms frame. The serial demos (`bench`, `slide`, `widgets`) take over the screen
  from `loop()` while they run (section 17.2).
- Core 0: `AiWorker` blocks on a FreeRTOS queue. A request carries a `Game` snapshot by value,
  the difficulty and a time budget; the worker runs iterative deepening until the budget expires
  or `cancel()` is called, then posts `{move, depth, nodes, requestId}`. Request ids are split into ranges per client
  (moves, hints, serial commands; `game/AiRequest.h`). `AiWorker::pollFor(id)` takes only a result
  with a matching id, so each client leaves the others' results in the slot. A result that nobody
  collects is requested again by the Play scene (qa-report F10). The same path serves the Hint button.
- No shared mutable state between cores. The transposition table belongs to the worker.
- The UI loop must keep > 30 fps while the AI thinks; the diagnostics line verifies this.
- Measured (issue #18, docs/ai-notes.md): the search runs at ~78k nodes/s (initial position)
  to ~87k (mid-game) on core 0, up from ~30k, after bitboard mobility counting, move-free leaf
  evaluation and -O2 on the hot game files. Within its budget Medium (500 ms) reaches depth 6 / 5
  and Hard (2500 ms, 1 MB TT in PSRAM) depth 8 / 7 (initial / mid-game); Easy (depth 2, random
  among the best 3) takes 2-5 ms. The Searcher stays in PSRAM (internal RAM was no faster),
  internal heap stays at ~310 KB free and the UI loop at 99 iterations/s while the AI thinks.

## 10. Persistence (NVS)

| Namespace / key | Contents | Written when |
|---|---|---|
| `settings` / `v1` | brightness, forcedCapture, theme, firstPlayer (unused, section 17.4), defaultDifficulty, p1 and p2 initials, flipBoard. Blob version 2, 14-byte payload; version 1 (13 bytes) is still read and upgraded on save | user changes a setting |
| `scores` / `v1` | 4 tables (Easy, Medium, Hard, Overall) x 10 entries of {initials[4], score, moves, gameNo}, plus the game counter. 564-byte payload | game over with a qualifying score |
| `save` / `v1` | mode, difficulty, human side, forcedCapture, both clocks, then the move list as {from, path}. Load replays the moves from the start and rejects an illegal one | after each completed move; erased at game end |
| `stats` / `v1` | per-difficulty played, won, lost, drawn; two-player count; current and best streak; total moves. 34-byte payload | game over |


Magic values are SET1, SCOR, SAV1 and STAT. Storage goes through `IStorage` (`services/IStorage.h`):
`hal/NvsStorage` on the device, and `RamStorage` as the fallback when NVS fails.

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
score effect in v1. The formula is `computeScore()` in `services/Scoring.cpp`. The Game Over scene
applies it under the forced-capture rule the game was started with (section 17.4).

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

- `pio test -e native` runs 302 Unity test cases in 26 suites (`test/test_*`) on the host, for
  `game/`, `services/`, the header-only parts of `ui/` and the scene logic in `app/`:
  - Board mapping and neighbor-table symmetry.
  - Perft from the initial position (a multi-jump counts as one move): depth 1 to 7 = 7, 49, 302,
    1469, 7361, 36768, 179740 by default; depth 8 = 845931 with `-DPERFT_DEEP`.
  - Capture edge cases: promotion ends the sequence, kings looping, no double-jumping a captured
    piece, forced-capture filtering.
  - Game: win/loss/draw detection, undo restores counters.
  - Search finds forced wins in short test positions; deterministic at fixed depth.
  - Scoring table; HighScores ordering and persistence against the RAM storage.
- On device: the diagnostics line (heap, PSRAM, loop time), a screenshot tool that dumps the frame
  buffer over serial to a PNG, and manual checklists: `docs/play-qa-checklist.md`, `docs/robustness-checklist.md`, and the panel
  test plan in `docs/qa-report.md`. `test_selfplay` plays Hard against Medium on the host
  (`docs/ai-notes.md`).
- CI: GitHub Actions builds the firmware and runs the native tests on every push and PR.

## 14. Directory layout

```
src/
  main.cpp                 wiring: hal init, scene registration, frame loop
  hal/      crowpanel7_lgfx.h AiWorker BootInfo Diag ForwardingStorage Log NvsStorage
            Scanout SerialCmd StorageSelector Touch Watchdog
  game/     AiRequest AiSync Apply Board Difficulty Eval Game Move MoveGen MoveList
            Notation Search TT Zobrist
  ui/       Animator BoardGeometry BoardView BoardViewState DirtyRects Hud RenderBench
            Renderer Sprites TapDetector Theme TouchEvent Tween WidgetCore Widgets WidgetsDemo
  app/      GameConfig GameOverText Nav PlayLogic Scene SceneManager SceneStack TouchGate
            Version scenes/ (Splash, MainMenu, GameSetup, Play, GameOver, HighScores, Settings)
  services/ Blob Crc32 GameTypes HighScores IStorage RamStorage SaveGame Scoring Settings Stats
test/
  common/perft.h           shared perft helpers
  test_*/                  26 Unity suites, one directory each, each with its own main()
tools/
  screenshot.py            frame-buffer dump to PNG
  format.sh                clang-format driver
docs/
  ARCHITECTURE.md (this file), ai-notes.md, play-qa-checklist.md, qa-report.md,
  rendering-notes.md, robustness-checklist.md, images/
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

Status at 0.1.0: M0 to M6 are implemented (CHANGELOG.md). #42 was run without the panel. #45 and
the follow-ups #47 to #49 are open (`docs/qa-report.md`).

## 16. Model assignment policy

Each issue carries one `model:*` label naming the least expensive Claude model expected to
complete it reliably. Escalate one tier if an attempt stalls.

| Label | Use for |
|---|---|
| `model:haiku-5.5` | Well-specified, mechanical tasks confined to one module with a clear acceptance check (constants, scaffolding, simple screens, docs, CI yaml). |
| `model:sonnet-5.5` | Standard feature work across a few modules following patterns that already exist in the repo; moderate debugging. |
| `model:opus-5.5` | Algorithmic correctness or performance-critical code where subtle bugs are costly (capture generation, search, transposition table) and architecture-defining modules others build on. |
| `model:fable-5.1` | Cross-cutting hardware-level debugging with uncertain root causes (frame-buffer timing on the RGB panel) and the final whole-system integration pass. |

## 17. As built (0.1.0)

### 17.1 Scene navigation

`app/Nav.h` defines `SceneId`: Splash, MainMenu, GameSetup, Play, GameOver, HighScores and Settings.
`main.cpp` allocates every scene statically and registers each one once with `nav.set(id, &scene)`.
Scenes do not hold pointers to each other. They call `ctx.nav.go(id)` to replace the current scene,
or `push` and `pop` for stacked ones. The request is queued on `SceneManager` and applied at the top
of the next tick, before touch dispatch. After any transition the whole screen is marked dirty, and
`TouchGate` drops touches that began before the transition until a new press 200 ms later.
`SceneContext` carries the shared services. `GameConfig` is written by the menus and read by Play.
Resume sets `config.resume` and goes to Play, which loads the autosave.

Flow: Splash (1.5 s or a tap) leads to the Main Menu. The Main Menu offers Resume (only when an
autosave exists) to Play, and Play vs Computer or Two Players to Game Setup and then Play. High Scores
and Settings are leaves. Play goes to Game Over at the end of a game. Game Over goes to Rematch (a new
Play with the same settings) or to the Main Menu. The Menu button in Play autosaves and goes to the
Main Menu.

### 17.2 Frame loop

`loop()` in `main.cpp` feeds the watchdog, ticks the diagnostics, handles serial commands and polls
touch. Then it runs one of three things: the `widgets` demo (until reboot), the `slide` demo (until it
finishes), or `SceneManager::tick()`. It then sleeps out the rest of a 16 ms frame, at least 1 ms.

### 17.3 Measured figures

- AI (`docs/ai-notes.md`, section 9 here): Medium reaches depth 6 (initial) or 5 (mid-game) in 500 ms,
  Hard depth 8 or 7 in 2.5 s, Easy takes 2 to 5 ms. Measured on the board in #18.
- Rendering (`docs/rendering-notes.md`): panel refresh 30.8 Hz, one cell 0.4 ms, full board 33 ms
  beam-synced.
- Not measured on the panel yet: loop worst case during play, stack headroom, autosave time, frame
  time during slides (`docs/qa-report.md` section 3).

### 17.4 Deviations from the original design

1. **No pause menu and no resign.** Play has Undo, Hint and Menu. Menu autosaves and returns to the
   Main Menu, and Resume continues the game.
2. **Forced capture is a setting.** Settings has a forced-capture toggle (default on) instead of a
   reserved variant flag. Only vs-computer games under forced capture are scored or entered in the
   high scores. Game Over uses the rule the game was started with (`Game::rules()`, kept in the
   autosave), not the current toggle (qa-report F8).
3. **`Settings.firstPlayer` is unused.** The field is stored, but "You play" in Game Setup decides who
   moves first. Remove it or document it as reserved at the next blob version (qa-report F17).
4. **Theme is stored only.** Settings cycles Classic and Slate, but only the Classic palette is drawn.
   The screen says so.
5. **Demo scenes removed.** MockScreen and the first demo scene were replaced by BoardView, Hud and the
   real scenes (03b3407, 29c670a). The demos that remain are serial-driven (`bench`, `anim`, `view`,
   `hud think`, `slide`, `widgets`) and sit outside the scene navigator.
6. **No `Clock` or `Display` module.** Time is passed in as `nowMs` (`Scene::update`, `AiRequest`), so
   tests use constants. The `LGFX` instance is a static in `main.cpp`.
7. **Fixed history limit.** The history holds 256 plies. A game that fills it is a draw
   (`Reason::MoveLimit`, qa-report F2). The 40-move rule is 80 plies.
8. **Added after the design:** Hint (#39), board flip and per-player clocks in two-player games (#40),
   the Settings default difficulty and Restore defaults, the RAM storage fallback (#41), and the
   touch lockout after a scene change (`TouchGate`, qa-report F6).
9. **Rendering.** Flushes are beam-synced per band, as section 7 decided, not vsync-aligned. The
   Animator overlay is not beam-synced and may tear (#47).
10. **Autosave runs in the frame that ends a slide** (#48). A menu opened during a slide drops the
    pending move instead of applying it (#49).
11. **Phantom-tap guards are off** (#45). `minPressMs` and `maxTapsPerSecond` exist but default to 0.
