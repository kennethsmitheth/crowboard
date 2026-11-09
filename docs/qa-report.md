# QA report: whole-system integration pass (issue #42)

The panel was not available for this pass (it was returned to its owner), so the "three full
games per difficulty on the hardware" became two things: a cross-cutting review of the integrated
firmware for the bugs that only appear where the modules built in parallel meet (section 1, with
fixes and host regression tests where the code is host-testable), and the exact procedure the
owner runs when the panel is flashed again (sections 2 and 3). Everything below that says
"needs hardware" is unverified; everything marked "fixed" builds (`pio run -e crowpanel7`) and
passes the 302 native tests (`pio test -e native`).

Code reviewed end to end: `src/main.cpp`, `src/app/**`, `src/ui/**`, `src/hal/**`, the game
glue (`Game`, `Search`, `TT`, `AiRequest`, `AiSync`, `Difficulty`) and `src/services/**`, with
docs/ARCHITECTURE.md, docs/rendering-notes.md, docs/ai-notes.md and the two checklists as the
reference for intended behaviour.

## 1. Findings

Severity: **Critical** = the game cannot be played; **High** = a game is lost or the screen is
wrong in normal use; **Medium** = visible defect or stall in a common path; **Low** = edge case
or polish; **Info** = behaviour worth knowing when testing, not a defect.

| # | Severity | Area | Finding | Where | Status |
|---|---|---|---|---|---|
| F1 | Critical | Concurrency | `SerialCmd::tick()` polled the AiWorker's single result slot every loop iteration and dropped every result whose id it had not issued as "stale". The Play scene polls the same slot one step later in the same iteration, so almost every computer move and every hint was consumed and discarded before the scene saw it: "Computer thinking..." never ended. Nothing shared the worker when the Play scene was written, and no host test covers the FreeRTOS queue. | `src/hal/SerialCmd.cpp` (`aiPoll`), `src/app/scenes/PlayScene.cpp` (`pollAi`), `src/hal/AiWorker.*` | Fixed in 5e77850: per-client request-id ranges (`kAiIdSerial`, `kAiIdHint`, moves below both, `game/AiRequest.h`), `AiWorker::pollFor(id)` takes only a matching result, SerialCmd polls only while it has a request out. Static_assert on the ranges; not host-testable beyond that. Needs hardware to confirm the computer replies (plan step 5.1). |
| F2 | High | Rules / flow | `Game::apply()` refuses every move once the fixed history (`kMaxPlies` = 256) is full, but `evaluate()` left the result `Ongoing`. The Play scene then froze: taps did nothing and the computer was re-asked after every refused reply. Reachable by two humans shuffling kings with an irreversible move every few dozen plies. | `src/game/Game.cpp` (`evaluate`) | Fixed in c9c06b8: `Reason::MoveLimit` draw; "Draw: move limit" in the HUD, "move limit" on Game Over. Regression test `test_move_limit_draws` plays a 256-ply game. |
| F3 | High | Rendering | No scene marked the screen outside its own widgets on `enter()`: widget scenes marked their widget rectangles, Play the board and sidebar. Leaving Play for the Main Menu left the board and HUD visible around the menu buttons; the splash text stayed on the menu where no button covered it; the area outside widgets stayed black from `gfx.init()`. Only scenes with a full-screen Keypad (Game Setup, Game Over) happened to clear it. | `src/app/SceneManager.cpp` | Fixed in ccd633f: `SceneManager::tick()` calls `DirtyRects::markAll()` after any applied transition. Needs hardware for the look (plan step 1.3). |
| F4 | Medium | Rendering / latency | `Renderer::collectBands()` created a band for every dirty row and rectangle even when the matching painter was null; `paintBand()` painted nothing but `flush()` still waited for the beam (up to ~5 ms per row). With F3's `markAll()` a menu scene would have waited for eight cell rows it cannot paint. | `src/ui/Renderer.cpp` | Fixed in 063f830: bands without a painter are dropped. |
| F5 | Medium | Rendering / latency | `Scanout::waitBeamClear()` can never succeed for a band that reaches the bottom margin and starts near the top: the whole sidebar (`Hud::invalidateAll`, every Play enter), the full-screen Keypad rectangle (Game Setup, Game Over enter, keypad close) and now the transition rectangle. Each such flush waited out the 40 ms timeout and painted anyway: a guaranteed 40 ms stall on every scene change, on top of the paint time. | `src/ui/Renderer.cpp` (`flush`), `src/hal/Scanout.h` | Fixed in 9074fe6: `Scanout::beamClearPossible()` decides from the geometry; tearing outcome for those bands unchanged. Test `test_beam_clear_possible_matches_beam_clear` checks it against an exhaustive sweep of `beamClear()`. |
| F6 | Medium | Input | The touch queue is drained after the pending transition is applied, so a press that began on the old scene delivered its Up and Tap (or all its events) to the new scene; a Tap alone activates a widget (`RegionPress::tap`). The second tap of a quick double tap on Game Over's "Main Menu" landed on the menu's Settings button (same y range), a late tap on Game Setup's "Start" selected a piece. | `src/app/SceneManager.cpp`, new `src/app/TouchGate.h` | Fixed in 7597a3c: after a transition every event is dropped until a Down at least 200 ms later (the new screen is on the panel ~70 ms after the transition). Five tests in `test_scenes`. Side effect to know: a 200 ms input lockout after each scene change. |
| F7 | Medium | Game flow | Resume took mode, difficulty and side from the save header but the initials from `ctx.config`, which after a power cycle holds the boot defaults: the HUD and Game Over showed "P1", the high-score keypad was prefilled with "P". `ctx.config` was never updated from the save, so Rematch after a resumed two-player (or Hard) game started whatever had been set up before the power cycle (default vs Computer / Medium). `hintsUsed_` carried over on resume. | `src/app/scenes/PlayScene.cpp` (`enter`) | Fixed in 37a3781: initials from Settings on resume, hint counter reset, effective config copied back into `ctx.config`. Needs hardware (plan step 5.9). |
| F8 | Medium | Scoring | Game Over decided "recorded or not" from today's Settings `forcedCapture` toggle, not the rule the game was played under (`Game::rules()`, kept in the autosave). Toggling the setting mid-game (Menu, Settings, Resume) recorded a casual game or refused a forced-capture game. | `src/app/scenes/GameOverScene.cpp` | Fixed in da71fc1: `PlayScene::Summary::forcedCapture` from `game_.rules()`. |
| F9 | Low | AI | The Play scene used `makeRequest()`'s defaults: seed 0 and `newGame` false. Easy picks among its best three moves with `AiRng(seed)`, deterministic for a given seed, so Easy replayed the same game against the same human moves (Difficulty.h says the scene seeds from `millis()`). The 1 MB TT was never cleared between games, contrary to docs/ai-notes.md. | `src/app/scenes/PlayScene.cpp` (`requestAi`) | Fixed in b36e0d5: seed from frame time and id; first request of a new or resumed game carries `newGame`. Needs hardware for the Easy variety check (plan step 5.6). |
| F10 | Low | Robustness | A result nobody can take any more (the peek/receive window in `pollFor`, see F20; a serial `ai`/`aibench` typed during a game) left the scene on "Computer thinking..." / "Thinking of a hint..." for ever, Hint disabled. | `src/app/scenes/PlayScene.cpp` (`update`) | Fixed in 32551a7: when the worker is idle, its slot is empty and the scene has waited >= 250 ms, the move is re-requested (`[W] play: AI result N lost, asking again`) and a hint is dropped. `busy()` is true from `request()` until the result is in the slot, so this cannot fire on a search in flight. |
| F11 | Low | Diagnostics | The Arduino loop task runs every scene, the renderer and the autosave (`SaveGame::save` and `Blob::save` each hold a ~1 KB buffer, then NVS) on an 8 KB stack; nothing measured the headroom. Estimated deepest path ~5 KB. | `src/hal/Diag.cpp` | Fixed in ad09782: the diag line carries `stack min free N` (bytes). Measurement needs hardware (section 3). |
| F12 | Low | Debug tooling | The serial `slide` demo drove main.cpp's Animator without installing it as the Renderer's overlay (the Play scene's Animator owns that slot), so it hid the origin square and marked cells but drew nothing. | `src/ui/RenderBench.cpp` | Fixed in 6a2c6dd: installed for the demo, the scene's overlay restored when it ends. |
| F13 | Medium | Rendering | The `OverlayPainter` (sliding piece, capture fade disc, promotion ring) is called once after all bands with no beam check and its own write-back, so the moving piece can tear although the cells under it were painted beam-synced. The `anim` experiments drew through the cell painter, so this path was never checked on the panel. | `src/ui/Renderer.cpp:117` | Open, needs hardware: **#47**. Check with `slide` and real multi-jumps (section 3). |
| F14 | Medium | Latency / persistence | `afterBoardChange()` runs the autosave synchronously in the frame that ends a slide: `nvs_set_blob` + `nvs_commit` of up to ~1 KB, normally a few ms, tens of ms when a page fills and a sector is erased; game end adds `Stats::save()` and `SaveGame::erase()`. Lands during the capture fade / promotion pulse, so a long one is a hitch at the start of every fade. | `src/app/scenes/PlayScene.cpp:467-480` | Open, needs hardware: **#48**. Measure with `heap` (diag worst loop) and `render` (paint time only) after a few hundred moves. |
| F15 | Low | Game flow | Menu during a slide drops the move being animated ("pending move is dropped"), so after Resume the human has to play it again; Undo during a slide applies it first (`applyPendingNow`). The two interruptions disagree. | `src/app/scenes/PlayScene.cpp:195` | Open: **#49** (suggested fix: `applyPendingNow()` before the autosave). |
| F16 | Low | Input | Phantom taps (#45): `TapConfig::minPressMs` and `maxTapsPerSecond` exist but are not wired (defaults 0); the new TouchGate only protects scene changes. On the board a phantom Tap on a legal-target dot plays a move; on a menu it presses a button. | `src/ui/TapDetector.h:18`, `src/hal/Touch.h` | Open, tracked in **#45**; the test plan logs idle taps (step 9.11). |
| F17 | Low | Persistence | `SettingsData::firstPlayer` is stored (blob v1/v2) but never read; Game Setup's "You play Dark (moves first) / Light" decides who opens, and the AI opens when the human plays Light (verified in `PlayScene::update`). | `src/services/Settings.h:19` | Open: remove in the next blob version or document as reserved; fold into **#43** (as-built docs). |
| F18 | Low | Memory | The NVS fallback `RamStorage` (`RamStorageT<16, 1100>`) reserves 18.4 KB of internal RAM for four blobs of at most 1036 bytes; `RamStorageT<4, Blob::kHeaderSize + Blob::kMaxPayload>` would save ~14 KB. Static RAM is 74.6 KB (22.8%), so not needed now. | `src/main.cpp:50` | Open, not filed (no pressure). |
| F19 | Low | Persistence | `SaveGame::save()` returns false without writing when a game's move bytes exceed 1010 (only possible with many multi-jumps); `autosave()` ignores the return, so the previous save stays and Resume restores an older position. | `src/services/SaveGame.cpp:41` | Open, not filed; log the failure if it is ever seen (`[W]`), or drop the oldest plies from the save. |
| F20 | Info | Concurrency | `AiWorker::pollFor()` peeks then receives; if the worker overwrites the slot in between (a newer result for a newer request), the result taken belongs to another client and is dropped. Window of a few hundred ns, needs two clients with requests out (a serial command during a game). | `src/hal/AiWorker.cpp:86` | Documented; F10's guard re-requests the lost move. |
| F21 | Info | Debug tooling | `view 0..2`, `hud think`, `widgets` and `slide` install their own painters; the active scene's painters come back only at the next scene change. | `src/ui/RenderBench.cpp:123,162` | Use them from a menu, then navigate away and back (section 3). |
| F22 | Info | Watchdog | The core-0 idle task is watched (10 s after `watchdogBegin()`); the AI task (priority 1, pinned to core 0) starves it for the length of a search. Hard's 2.5 s and `aibench`'s fixed depth 6 (< 1 s) are safe; a fixed-depth run deeper than ~8 with budget 0 would trip it. | `src/hal/Watchdog.cpp:15`, `src/hal/AiWorker.cpp:58` | Needs hardware: robustness row 8. |
| F23 | Info | Latency | A scene change now repaints the whole screen once: ~30 ms for a menu (800x480 fill at ~25 MB/s) and ~35-45 ms for Play (64 cells + sidebar, beam-chased). One-off per transition, under the 50 ms budget on paper. | `src/app/SceneManager.cpp` | Needs hardware: diag `worst` right after a transition (section 3). |
| F24 | Info | Concurrency | A serial `ai`/`aibench` typed during a game aborts the game's search (`request()` bumps the cancel generation); the scene validates and plays the partial result. `aibench` output is only meaningful from a menu. | `src/hal/SerialCmd.h` (documented) | Documented. |

Checked and found correct (no change): the flip mapping is applied consistently in taps
(`cellAt(x, y, flipped_)`), cell painting, dirty marks, the Animator's path and the hint
outline; the hidden square is reset by `Animator::cancel()` on exit, enter, Undo and Menu;
stats are recorded and the save erased exactly once (`finish()` is reachable once per game);
Game Over reads `summary()` before Rematch resets it; Undo during AI thinking cancels the request
and takes back one ply, the stale result is never applied; Undo during a slide applies then
undoes; the AI opens when the human plays Light; clocks are restored from the save and stop at
game end; Settings v2 reads v1 blobs and upgrades on save; NVS names are within 15 characters;
`RamStorage` holds the largest blob (1036 bytes); hint results are validated against the current
legal moves and dropped after any board change; the request-id ranges of moves, hints and serial
commands cannot collide; no heap allocation on the per-frame path (`Serial.printf` of a line over
64 characters mallocs, which only the 5-second diag line does); `Searcher` (131 KB), the TT
(1 MB) and the sprites live in PSRAM; `Game` is 18.7 KB static inside the Play scene.

## 2. Manual test plan (run on the panel, in this order)

Serial monitor at 921600 (`pio device monitor`). The firmware is built with
`-DCROWBOARD_DEBUG_CMDS` (fault injection commands). Where a step says "checklist", run the named
section of docs/play-qa-checklist.md or docs/robustness-checklist.md as written; this plan only
adds the ordering, the serial lines and the checks that come out of section 1.

### Step 0: build, flash, boot

1. `pio run -e crowpanel7 -t upload`, then `pio device monitor`.
2. Expected boot lines, in order: `[I] CrowPanel 7.0" hello world`, `[I] boot: reset reason
   POWERON (1)` (or `SW`/`EXT` after the upload reset), the PSRAM and heap lines, `[I] Sprites:
   28800 bytes, ...`, `[I] AiWorker: ready, Searcher 131416 bytes in PSRAM, TT 65536 entries,
   stack 24576`, `[I] Settings: brightness 80% forcedCapture 1 theme 0 difficulty 1 save none`,
   `[I] touch: ready`, `[I] scanout: ... vsync hooked`, `[I] Display ready: 800 x 480`,
   `[I] wdt: loop task watched, timeout 10000 ms`. No `[E]` lines, no `coredump:` lines.
3. Screen: the whole panel turns the dark HUD grey (not black with grey strips, F3), the title
   "Crowboard Checkers" and the version line; after 1.5 s (or a tap) the Main Menu.
4. Every 5 s one line `[I] diag heap ... (min ...) psram ... | loop avg a.b ms worst c.d ms |
   N loops/s | stack min free S | touch: ok | storage: nvs`. Note the first values (section 3).

### Step 1: Main Menu

1. First boot: no Resume button; Play vs Computer, Two Players, High Scores, Settings stacked
   from the top with no gap.
2. Tap each button and come back (Back / Main Menu); every screen opens.
3. F3: on each arrival nothing of the previous screen remains anywhere (corners, between
   buttons, the bottom strip). Later, after a game: Menu from Play must show no board or sidebar
   around the buttons.
4. F6: on Game Over tap "Main Menu" twice as fast as you can; the menu must appear and stay
   (Settings must not open). Tap a menu button within 0.2 s of the screen appearing: nothing
   happens (input lockout); tap again: it works.

### Step 2: Settings

1. Brightness - / +: 10% steps between 10 and 100, the backlight follows at once, the value
   label updates. Reboot: the value persists.
2. Forced capture and Flip board toggles; Default difficulty cycles Easy / Medium / Hard; Theme
   cycles Classic / Slate (stored only). Each change persists across a reboot.
3. Reset scores and data: the modal appears on top (no other button reacts while it shows);
   Cancel keeps everything; Reset clears scores, stats and the saved game (Resume disappears).
4. Restore defaults: 80%, forced capture on, flip off, Medium, Classic.
5. Robustness row 3 (`nvs corrupt settings v1`, reset): defaults come back, saving a setting
   rewrites a good blob.

### Step 3: High Scores

1. Tabs Easy / Medium / Hard / Overall: the active tab is greyed, only the table repaints.
2. Empty tables show `---` in every row; the footer reads `Played Easy 0 Medium 0 Hard 0` and
   `Won ... Streak 0`.
3. Reset behind its modal clears every table and the stats.

### Step 4: Game Setup

1. From Play vs Computer: Difficulty cycles; "You play" toggles `Dark (moves first)` / `Light`;
   "Your initials" Edit opens the full-screen keypad (letters only, Del, OK disabled until one
   letter is typed, prefilled with the stored initials when they are letters).
2. From Two Players: two initials rows, no difficulty or side row.
3. Start opens Play; Back returns. After Start, Settings shows the chosen default difficulty and
   the initials persist across a reboot.

### Step 5: Play vs Computer (one game per difficulty here; the measured games are step 8)

1. **F1**: after your first move the status reads "Computer thinking..." and the computer
   replies within its budget (Easy well under a second, Medium ~0.5 s, Hard ~2.5 s). If it never
   replies, the fix did not take: look for `[W] play: AI result N lost, asking again` (the F10
   guard) in the log.
2. Checklist "Board and HUD", "Selection", "Captures", "Animation", "Promotion" and "AI", in
   that order.
3. Checklist "Hint": also confirm that Hint stays greyed while a hint is pending and that the
   status clears when the outline appears.
4. Checklist "Undo", including Undo during "Computer thinking..." (one ply back, no late move)
   and Undo during a slide.
5. Checklist "Game end": win by capture, loss by no moves, and the 40-move and repetition draws
   (two-player is easier for the draws, step 7). "Draw: move limit" (F2) is only reachable in a
   256-ply game; it is covered by the host test.
6. **F9**: start two Easy games and play the same opening moves; the computer should not reply
   identically in both (its Easy pick is random among the best three).
7. **F8**: with forced capture on, play a game to the end: Game Over shows the breakdown and
   "New high score!" or a plain total. Then: Menu from a running forced-capture game, Settings,
   turn forced capture off, Menu, Resume, finish the game: the score must still be recorded (the
   game's rule wins). Start a new game now (casual rule): Game Over must say "Not recorded".
8. Checklist "Autosave and resume" (power cycle mid-game, power cycle during "Computer
   thinking...", Menu then Resume). After a finished game the next boot shows no Resume.
9. **F7**: set initials in Game Setup (e.g. KEN), play two moves, power-cycle, Resume: the HUD
   shows KEN, not P1; finish the game: the headline and the keypad use KEN; Rematch starts the
   same mode and difficulty as the resumed game.
10. Menu during a slide (F15 / #49, current behaviour): the move being animated is not in the
    save; Resume shows the position before it.

### Step 6: Game Over

1. Headline "You win!" / "Computer wins" / "Draw", reason line ("no pieces left", "no legal
   moves", "40-move rule", "threefold repetition"), the five breakdown rows for vs-computer
   games, no rows for two-player games (note line instead).
2. Qualifying score: the keypad appears prefilled with your initials; OK inserts and the note
   reads `Rank r (overall o)`; High Scores shows the entry with the game number order.
3. Rematch: same mode, difficulty and side, a fresh board, clocks at 00:00, Undo disabled.
4. Main Menu: Resume is not shown (the save was erased at game end).

### Step 7: Two players

1. Checklist "Two-player (pass and play)": dual clocks, the amber bar, flip board off and on,
   taps on the flipped board (selection dots, last-move outline, forced rings), slide and
   capture fade on the flipped board, Undo flips back, Resume keeps the orientation.
2. Undo takes back exactly one ply; Hint works on either side's turn.
3. 40-move draw and repetition draw by king shuffling (status "Draw: 40-move rule" / "Draw:
   repetition"; Game Over reason line matches).
4. Game Over headline uses the winner's initials; the two-player count in stats increments.

### Step 8: measured games (section 3 tells you what to record)

1. Three complete games on Easy, three on Medium, three on Hard (vs computer, forced capture
   on), two two-player games (one with flip board on). Vary who plays Light.
2. After each game, from the Game Over screen, type `heap` and `render` and record the lines.
   After the last game type `aibench` from the Main Menu.
3. No reboot during the session: every boot line that is not the first means a crash; read the
   `coredump:` lines and keep them.

### Step 9: robustness (docs/robustness-checklist.md rows 1-10, in order)

1. Row 1 normal boot was step 0. Rows 2 (touch unplugged), 3 and 4 (corrupt blobs), 5 (`crash`),
   6 (`crash hang`), 7 (`bench`, `anim beam 60`, `anim vsync 60`, `anim raw 60`, `slide`), 8
   (`ai hard` and `aibench` while `anim beam 30` runs), 9 (NVS partition damaged), 10 (reset
   reasons).
2. Run `bench`, `anim`, `view`, `slide` and `widgets` from a menu, not from a game (F21, F24);
   after `view`/`slide` go to another scene and back to repaint. `widgets` owns the screen until
   reboot.
3. The diag line must keep coming during `ai hard` (`loops/s` > 0) and must show `storage: nvs`
   (RAM-ONLY means NVS failed, row 9).
4. **F16 / #45**: leave the panel untouched on the Main Menu for 10 minutes, then during
   `anim beam 10` and during `ai hard`; the menu must not change scene and no button may show as
   pressed. If it does, note the time and what was running.

## 3. Measurements that still need the hardware

All commands go to the serial console. Record the raw lines in this file under "Results" when
the pass is done.

| Measurement | How | Acceptance (issue #42) / expectation |
|---|---|---|
| Minimum free heap and PSRAM over the session | `heap` after every game (step 8) and read the 5-second diag line: `heap H (min M) psram P`. | `min` > 60 KB. Expected around 300 KB internal (309,712 measured in #18) and ~5.5 MB PSRAM. A falling `min` across games means a leak. |
| Worst loop time outside animations | Diag line `loop avg a.b ms worst c.d ms`; the window resets every 5 s, so read it while idle on a menu, while idle in Play with the computer thinking, and in the window right after a scene change (F23) and right after a move (F14). | Worst < 50 ms outside slides. Expected: idle < 5 ms; a scene change ~35-45 ms once; after a move the autosave adds the NVS time (#48 if it exceeds the budget). |
| Loop task stack headroom | Diag line `stack min free S` (bytes, F11), after a full game and after `nvs corrupt ...`. | S > 2048 after three games; below 1536 raise `CONFIG_ARDUINO_LOOP_STACK_SIZE` or move the blob buffers off the stack. |
| Frame time during animation | `render` right after the computer's reply has finished sliding (stats are cumulative since boot: note `flushes`, `last`, `worst`, `avg`, `draw`/`wait` split, `cells`); for per-stage numbers use `slide` from a menu: it prints `slide 21-14-7: N ms, F flushes (fps), flush avg/worst, avg cells`, then the same for the capture fade and the promotion pulse. | Incremental flush avg < 3 ms and worst < 16 ms during a slide (docs/rendering-notes.md measured 0.6-3 ms); ~30 fps through the slide. |
| AI depth and time per difficulty | `aibench` from the Main Menu (ten `aibench:` lines plus `aibench: done in ... heap ... (min ...)`), and `ai easy` / `ai medium` / `ai hard` for one line each (`move`, `depth`, `nodes`, `ms`, `completed`, worker `stack free`). For in-game depth build once with `-DLOG_LEVEL=4` and read `AiWorker: id ... depth ...` per move. | Matches docs/ai-notes.md: Medium depth 6 / 5 (initial / mid) in ~500 ms, Hard 8 / 7 in ~2500 ms, 75-90k nodes/s, Easy a few ms; worker stack free >= ~16 KB; the UI diag keeps `loops/s` around 60 while Hard thinks. |
| Tearing by eye | `anim raw 10` (baseline: a shear of the disc a few times per second proves tearing is visible on this panel), then `anim beam 10` and `anim vsync 10` (both clean). Then `slide` and real multi-jumps in a game for the overlay path (#47). Then watch the sidebar clock tick in Play and a Game Setup keypad open/close (tall bands that are painted without waiting, F5). | `anim beam` clean; the sliding piece, fade disc and pulse ring clean (otherwise #47); a tall-band paint may show a one-frame wipe, acceptable. |
| Transition cost and look | Watch each scene change (F3, F23) and read the diag `worst` in that window. | One full repaint, no remnants, ~30-45 ms once. |
| Watchdog under load | Robustness rows 7 and 8; and `aibench` during `anim beam 30`. | No `Task watchdog got triggered`, no reboot; `loops/s` stays > 0 in the diag line. |
| Screenshots for the docs (#43) | `shot` then `python tools/screenshot.py` per docs; one per scene. | Frame buffer dumps; they cannot show tearing. |

### Results

_(fill in when the panel is available)_

## 4. Follow-up issues opened

- **#47** Animator overlay is painted outside the beam-synced bands and may tear (F13).
- **#48** Autosave writes NVS in the frame that ends a slide; measure and defer if it stalls
  the loop (F14).
- **#49** Menu during a slide drops the move being animated instead of applying it (F15).

Related existing issues: **#45** phantom taps (F16: `minPressMs` / `maxTapsPerSecond` are still
unwired; the step 9.4 idle log is its reproduction), **#43** as-built docs (F17 `firstPlayer`,
the new `Reason::MoveLimit` draw and the `stack min free` diag field belong in ARCHITECTURE §5,
§9 and the robustness checklist).

## 5. Commits in this pass

| Commit | Change |
|---|---|
| 5e77850 | Stop SerialCmd from consuming the Play scene's AI results (F1) |
| c9c06b8 | End a game that fills the move history as a draw (F2, test) |
| ccd633f | Repaint the whole screen after every scene transition (F3) |
| 063f830 | Renderer: do not wait for the beam on bands nobody paints (F4) |
| 9074fe6 | Renderer: skip beam waits that can never succeed (F5, test) |
| 7597a3c | Drop touches that straddle a scene transition (F6, tests) |
| 37a3781 | Resume: take the initials from Settings and publish the game's config (F7) |
| da71fc1 | Score a finished game under the rule it was played with (F8) |
| b36e0d5 | Play: seed Easy's random pick and clear the TT per game (F9) |
| 32551a7 | Play: re-request an AI move or hint whose result was lost (F10) |
| ad09782 | Diag: report the loop task's minimum free stack (F11) |
| 6a2c6dd | RenderBench: install the demo Animator as overlay during `slide` (F12) |

Verification after the last commit: `pio run -e crowpanel7` succeeds (RAM 74,652 bytes, 22.8%
of 320 KB; flash 16.1%), `pio test -e native` passes 302 test cases in 26 suites.

## 6. Not assessable without the panel

The acceptance criteria of #42 (no crash in a session, worst loop under 50 ms outside
animations, minimum free heap above 60 KB) and everything in section 3. In particular: that F1's
fix makes the computer reply (the FreeRTOS queue path has no host test), the look of the full
repaint on scene changes, whether the overlay tears (#47), the real NVS write time (#48), the
loop stack headroom (F11), Easy's variety (F9), and the phantom-tap rate (#45).
