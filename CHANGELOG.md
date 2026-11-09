# Changelog

All notable changes to crowboard. The format follows [Keep a Changelog](https://keepachangelog.com/).
The firmware version is `kFirmwareVersion` in `src/app/Version.h`. Work is tracked in the roadmap
issue [#44](https://github.com/kennethsmitheth/crowboard/issues/44).

## [0.1.0] - 2026-11-08

The first complete build: every roadmap milestone (M0 to M6) is implemented. It is host-verified
(`pio run -e crowpanel7` builds, and `pio test -e native` passes 302 test cases in 26 suites). Parts of
it have run on the panel, as listed under "Verified on hardware". The rest does not yet have a
hardware run; see "Not yet verified on hardware".

### Added

#### M0 Infrastructure
- Source tree split into `hal/`, `game/`, `ui/`, `app/` and `services/` (#1).
- PlatformIO native environment with Unity tests for `game/` and `services/` (#2).
- GitHub Actions workflow that builds the firmware and runs the native tests on push and pull request (#3).
- Partition table: 3 MB app, 64 KB NVS, 64 KB core dump, LittleFS, no OTA (#4).
- clang-format, `.editorconfig`, `CONTRIBUTING.md` and `tools/format.sh` (#5).
- Logging with compile-time levels, and a periodic diagnostics line: heap, PSRAM, loop time and rate (#6).
- Serial screenshot tool: `shot` streams the frame buffer, and `tools/screenshot.py` saves a PNG (#7).

#### M1 Rules engine
- 32-square bitboard board with neighbour tables, numbering 1 to 32 and position strings (#8).
- Simple move generation for men and kings (#9).
- Capture generation: multi-jumps, forced capture, and promotion ending the turn (#10).
- Game state with apply and undo, history, win and loss detection, the 40-move rule and repetition (#11).
- Move notation in 1-32 numbering (#12).
- Rules test suite with perft from the initial position and capture edge cases (#13).

#### M2 AI opponent
- Static evaluation with tunable weights: material, kings, advancement, back rank and mobility (#14).
- Alpha-beta search with iterative deepening, move ordering and a time budget (#15).
- Zobrist hashing and a transposition table in PSRAM (#16).
- `AiWorker`: search on core 0 with request and result slots and cancellation (#17).
- Difficulty profiles (Easy, Medium, Hard) and the on-device `aibench` command (#18).

#### M3 Rendering and input
- Theme and layout for 800x480, and the static board mock (#19).
- Dirty-rect renderer for the RGB frame buffer, with beam-synced flushes (#20).
- Piece sprites pre-rendered in PSRAM (#21).
- `BoardView` and `Hud`: cells, pieces, highlights, last move, captured counts, clocks and buttons (#22).
- Widgets: Button, Label, Toggle, ListView, Modal and the initials keypad (#23).
- Touch input: debounce, tap detection, cell mapping and an event queue (#24).
- Animator: piece slide along jump paths, capture fade and promotion pulse (#25).

#### M4 Persistence and scoring
- Versioned, CRC-checked blobs on NVS, with a RAM implementation for tests (#26).
- Scoring formula with table-driven tests (#27).
- High scores: top 10 per difficulty and overall (#28).
- Settings, persisted in NVS (#29).
- Autosave and resume of an in-progress game (#30).
- Stats: games played and won per difficulty, and win streaks (#31).

#### M5 Scenes
- Scene framework, `SceneManager`, the scene navigator and the scene-driven main loop (#32).
- Splash and Main Menu, with Resume shown when an autosave exists (#33).
- Game Setup: difficulty, side, and initials; two-player setup (#34).
- Play scene: selection, forced-capture cues, moves, multi-jump continuation, undo and menu (#35).
- Game Over: result, score breakdown, and high-score entry (#36).
- High Scores: a tab per difficulty, a stats footer and reset (#37).
- Settings: brightness, forced capture, theme, reset data and About (#38).
- Animator integrated into the Play scene (#46).

#### M6 Integration and polish
- Hint button: a 300 ms search outlines a suggested move (#39).
- Two-player polish: board flip (Settings) and per-player clocks (#40).
- Robustness: touch-failure fallback, task watchdog, NVS corruption fallback and reset-reason logging (#41).
- Integration pass: cross-module review and fixes, and the QA report with the manual test plan (#42).
- Documentation brought up to the as-built code, with these screenshots and this changelog (#43).

### Fixed (integration pass, #42)
- The computer's replies and hints were consumed by the serial command path before the Play scene saw them.
- A game that fills the 256-ply history now ends as a draw, instead of freezing the Play scene.
- A scene change repaints the whole screen, so no remnants of the previous scene stay on screen.
- Beam waits that can never succeed are skipped, which removes a guaranteed 40 ms stall per scene change.
- Touches that straddle a scene transition are dropped.
- Resume takes the initials from Settings and publishes the saved game's configuration.
- A finished game is scored under the rule it was played with.
- Easy's random pick is seeded per game, and the transposition table is cleared between games.
- A lost AI result is requested again, so the computer no longer waits forever.
- The loop task's minimum free stack is reported in the diagnostics line.

### Verified on hardware

Recorded in the docs, from the sessions before the scenes landed:

- Panel bring-up: 800x480 RGB output, the 15 MHz pixel clock, the 30.8 Hz scan-out and vsync and
  scan-beam tracking (#20, `docs/rendering-notes.md`).
- Renderer timing: a full-board redraw of about 33 ms beam-synced, a single cell of about 0.4 ms, and
  the `anim` and `bench` experiments (#20).
- The serial screenshot tool: `shot` and `tools/screenshot.py` on the panel (#7).
- Static board mock and sprite mock on the panel (#19, #21). These are the images in `docs/images/`.
- AI search speed and difficulty depths on the board: about 78k to 87k nodes per second, Medium at
  depth 6 and 5, Hard at depth 8 and 7, Easy in a few milliseconds (#18, `docs/ai-notes.md`).
- Heap and loop-time figures from the diagnostics line during AI runs (#6, #18).

### Not yet verified on hardware

Everything below was built and host-tested, but has not been run on the panel:

- Touch input and the widget set (#23, #24), including the touch lockout after scene changes.
- All scenes (#32 to #38), and a complete game in each mode and at each difficulty.
- Hint (#39), two-player mode with board flip and clocks (#40), and the Game Over screen and score entry (#36).
- High scores, stats and settings across power cycles (#26, #28, #29, #31).
- Autosave and resume, including a power cycle during the computer's move (#30).
- The robustness checks: the watchdog, NVS corruption, the reset reasons and the touch-absent fallback (#41).
- Each fix in the integration pass (#42).
- Open items: overlay tearing (#47), autosave time in the frame that ends a slide (#48), menu during a
  slide (#49), phantom taps (#45).
- Measurements still needed (`docs/qa-report.md` section 3): minimum free heap over a session, worst loop
  time outside animations, loop stack headroom, frame time during slides, and the AI numbers in a real game.

### Known gaps

See the README section "Known gaps". Settings has a theme toggle that changes nothing yet, and
`Settings.firstPlayer` is stored but never read.
